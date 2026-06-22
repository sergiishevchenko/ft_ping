# `getaddrinfo()` in `ft_ping`

`getaddrinfo()` is the POSIX API that turns a **hostname or IP string** into a **binary network address**. In **ft_ping** it runs **once** at startup inside `resolve_host()` (`srcs/dns.c`), called from `parse_args()` in `srcs/main.c`.

Related: [DNS.md](DNS.md) (forward vs reverse DNS), [IPv4.md](IPv4.md) (`sockaddr_in`, `sendto`), [GETOPT-LONG.md](../GETOPT-LONG.md) (when the host argument is parsed).

---

## Full code: `resolve_host()`

```c
#include "ft_ping.h"

int	resolve_host(t_ping *ping, const char *host)
{
	struct addrinfo	hints;
	struct addrinfo	*res;
	int				ret;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;
	ret = getaddrinfo(host, NULL, &hints, &res);
	if (ret != 0)
		return (-1);
	ping->dest_addr = *(struct sockaddr_in *)res->ai_addr;
	inet_ntop(AF_INET, &ping->dest_addr.sin_addr,
		ping->ip_str, INET_ADDRSTRLEN);
	ping->hostname = strdup(host);
	if (!ping->hostname)
	{
		freeaddrinfo(res);
		return (-1);
	}
	freeaddrinfo(res);
	return (0);
}
```

On failure, `parse_args()` prints `unknown host <name>` and exits. On success, three fields in `t_ping` are filled (see comparison table below).

---

## What `getaddrinfo()` does

Signature (simplified):

```c
int getaddrinfo(const char *node,
                const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res);
```

| Argument | In `ft_ping` | Role |
|----------|--------------|------|
| `node` | `host` — CLI string (`google.com`, `8.8.8.8`) | What to resolve |
| `service` | `NULL` | Port name or number; **not used** — ICMP has no ports |
| `hints` | IPv4 + raw + ICMP filters | Tell the resolver what kind of address you need |
| `res` | out pointer | Receives a **linked list** of `struct addrinfo` results |

Return value:

| Value | Meaning |
|-------|---------|
| `0` | Success — `*res` points to the first result node |
| non-zero | Failure — use `gai_strerror(ret)` for a message (`ft_ping` does not print it) |

The function replaces the older `gethostbyname()`. It is thread-safe and supports both IPv4 and IPv6 (we restrict to IPv4 via `hints`).

---

## Internal flow (two input cases)

`getaddrinfo()` always receives the same four arguments in `resolve_host()`:

```c
getaddrinfo(host, NULL, &hints, &res);
```

What happens **inside** libc depends on whether `host` looks like a **numeric IPv4 literal** or a **name** that must be looked up. Both paths end the same way for `ft_ping`: one `sockaddr_in` in `*res`, copied to `dest_addr`.

### Shared end state (both cases)

After a successful call, libc has:

1. Allocated one or more `struct addrinfo` nodes (linked list).
2. Filled each node to match `hints` (`AF_INET`, `SOCK_RAW`, `IPPROTO_ICMP`).
3. Set `res->ai_addr` to a `struct sockaddr_in` with:
   - `sin_family = AF_INET`
   - `sin_port = 0` (no port — `service` was `NULL`)
   - `sin_addr` = 4-byte IPv4 in **network byte order** (big-endian)

Example for `8.8.8.8`:

```
sin_addr bytes:  08 08 08 08
text form:       "8.8.8.8"
```

`ft_ping` then:

```c
ping->dest_addr = *(struct sockaddr_in *)res->ai_addr;  /* copy whole struct */
inet_ntop(..., ping->ip_str, ...);                       /* "8.8.8.8" or resolved IP */
ping->hostname = strdup(host);                           /* original CLI string */
freeaddrinfo(res);
```

The **internal path** to that end state differs.

---

### Case A: numeric IPv4 (`8.8.8.8`, `127.0.0.1`)

Input is already an IPv4 address written as text. No human-readable name to resolve.

#### Step-by-step

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. You pass host = "8.8.8.8"                                    │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. getaddrinfo() detects dotted-decimal IPv4 syntax             │
│    (four numbers separated by dots)                             │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. Parse string → 4 bytes                                       │
│    "8" "." "8" "." "8" "." "8"  →  0x08, 0x08, 0x08, 0x08       │
│    (same as inet_pton(AF_INET, "8.8.8.8", &addr))               │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. Usually NO DNS query on the network                          │
│    (pure string parsing inside libc)                            │
│    Note: resolver policy may still consult files first on some  │
│    systems, but "8.8.8.8" is recognized as literal IPv4.      │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Build addrinfo list (often a single node)                    │
│    ai_family   = AF_INET                                        │
│    ai_socktype = SOCK_RAW    ← from hints                       │
│    ai_protocol = IPPROTO_ICMP ← from hints                      │
│    ai_addr     → sockaddr_in { sin_addr = 08.08.08.08 }         │
│    ai_next     = NULL                                           │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Return 0; *res points to that node                           │
└─────────────────────────────────────────────────────────────────┘
```

#### Concrete example: `./ft_ping 127.0.0.1`

| Stage | Value |
|-------|-------|
| CLI `host` | `"127.0.0.1"` |
| DNS over the network? | **No** (local parse) |
| `sin_addr` (hex) | `7f 00 00 01` |
| `ip_str` after `inet_ntop` | `"127.0.0.1"` |
| `hostname` after `strdup` | `"127.0.0.1"` |
| Header | `PING 127.0.0.1 (127.0.0.1): 56 data bytes` |

Works **offline** — no nameserver required.

#### Invalid numeric input

| Input | Typical result |
|-------|----------------|
| `999.999.999.999` | `getaddrinfo` fails → `unknown host` |
| `not.an.ip.addr` | Treated as **hostname** → Case B (DNS), not numeric parse |
| `08.8.8.8` | May fail or parse (leading zeros — platform-dependent) |

---

### Case B: hostname (`google.com`, `localhost`)

Input is a **name**. The system must map it to an IPv4 address.

#### Step-by-step

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. You pass host = "google.com"                                 │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. getaddrinfo() sees: not a valid dotted IPv4 literal          │
│    → hand off to the OS **name resolver** (NSS on Linux)        │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. Resolver reads policy (e.g. /etc/nsswitch.conf on Linux)    │
│                                                                 │
│    Typical line:  hosts: files dns                              │
│    Meaning: try files first, then DNS                           │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4a. "files" → check /etc/hosts                                  │
│     grep for "google.com"                                       │
│     If found: use that IPv4 → skip DNS                          │
│     If not found: continue to next source                       │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4b. "dns" → DNS query to configured resolver                    │
│     (from /etc/resolv.conf: nameserver 8.8.8.8, etc.)           │
│                                                                 │
│     Question:  "What is the A record for google.com?"           │
│     Answer:    142.250.185.46 (example; may vary)               │
│                                                                 │
│     ft_ping hints restrict to IPv4 (AF_INET) — AAAA ignored.    │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. Resolver returns IPv4 address(es) to getaddrinfo              │
│    Multiple A records possible → multiple addrinfo nodes        │
│    ft_ping uses only res (first node)                           │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. Build addrinfo list                                          │
│    ai_addr → sockaddr_in { sin_addr = resolved 4 bytes }        │
│    ai_socktype / ai_protocol from hints                         │
└────────────────────────────┬────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. Return 0; *res ready for copy to dest_addr                   │
└─────────────────────────────────────────────────────────────────┘
```

#### Concrete example: `./ft_ping google.com`

| Stage | Value |
|-------|-------|
| CLI `host` | `"google.com"` |
| `/etc/hosts` hit? | Usually **no** for public names |
| DNS A record? | **Yes** (unless cached) |
| `sin_addr` | e.g. `8e fa 7a 2e` (142.250.185.46 — example) |
| `ip_str` | `"142.250.185.46"` |
| `hostname` | `"google.com"` (what you typed) |
| Header | `PING google.com (142.250.185.46): 56 data bytes` |

Requires working resolver (or `/etc/hosts` entry). Fails offline with `unknown host google.com`.

#### Special case: `localhost`

```
/etc/hosts often contains:
127.0.0.1   localhost
```

| Stage | What happens |
|-------|----------------|
| Input | `"localhost"` |
| DNS over Internet? | **Often no** — matched in `/etc/hosts` first |
| Result IP | `127.0.0.1` |
| `hostname` | `"localhost"` |
| Header | `PING localhost (127.0.0.1): 56 data bytes` |

Name in header stays `localhost`; packets go to `127.0.0.1` in `dest_addr`.

#### Failure in Case B

```
host = "this-domain-does-not-exist-xyz.invalid"
        │
        ▼
resolver tries files → dns
        │
        ▼
DNS: NXDOMAIN (name does not exist)
        │
        ▼
getaddrinfo returns non-zero (e.g. EAI_NONAME)
        │
        ▼
resolve_host() returns -1
        │
        ▼
ft_ping: unknown host this-domain-does-not-exist-xyz.invalid
```

No socket is opened. No root needed to hit this error path.

---

### Side-by-side comparison

| | **Case A: numeric IPv4** | **Case B: hostname** |
|---|---------------------------|----------------------|
| **Examples** | `8.8.8.8`, `127.0.0.1` | `google.com`, `localhost` |
| **Main work** | Parse string to 4 bytes | Name → IP lookup |
| **DNS network query** | Usually **no** | **Yes** (unless `/etc/hosts` wins) |
| **Works offline** | Yes (valid literal) | Only if name in `/etc/hosts` |
| **`hostname` field** | Same as typed IP | Original name (e.g. `google.com`) |
| **`ip_str` field** | Same dotted string | Resolved IP string |
| **`dest_addr`** | Binary of that IP | Binary of resolved IP |
| **Multiple IPs** | One | Possible; `ft_ping` takes **first** only |
| **IPv6 (AAAA)** | Not used (`AF_INET` hint) | Not used — IPv4 A record only |

---

### What `hints` does in both cases

`hints` does **not** change **how** the name is resolved. It changes **what** comes back:

```
                    ┌──────────────────┐
   host string ───► │  resolve to IPv4  │
                    └────────┬─────────┘
                             │
                    ┌────────▼─────────┐
                    │ filter / package  │
                    │ per hints:        │
                    │  AF_INET          │
                    │  SOCK_RAW         │
                    │  IPPROTO_ICMP     │
                    └────────┬─────────┘
                             │
                             ▼
                      *res  (addrinfo list)
```

If `hints.ai_family` were `AF_UNSPEC`, IPv6 results could appear — `ft_ping` explicitly avoids that.

---

### Timeline in the program

```
Case A (8.8.8.8)                    Case B (google.com)
─────────────────                   ───────────────────
parse_args()                        parse_args()
    │                                   │
resolve_host("8.8.8.8")             resolve_host("google.com")
    │                                   │
getaddrinfo                           getaddrinfo
    │                                   ├─ /etc/hosts?
parse "8.8.8.8" → bytes               └─ DNS query → A record
    │                                   │
    ▼                                   ▼
dest_addr, ip_str, hostname           dest_addr, ip_str, hostname
    │                                   │
    └──────────────┬────────────────────┘
                   ▼
            create_socket()
            ping_loop()
            sendto(dest_addr)   ← same API for both cases
```

After `resolve_host()` succeeds, **the rest of the program cannot tell** whether you typed an IP or a hostname — only `dest_addr` matters for sending, and `hostname` / `ip_str` for printing.

---

### Multiple A records (Case B only)

If DNS returns several IPv4 addresses for one name:

```
res  →  addrinfo #1  ai_addr = 142.250.185.46
        ai_next ──►  addrinfo #2  ai_addr = 142.250.185.110
        ai_next ──►  addrinfo #3  ...
        ai_next ──►  NULL
```

`ft_ping` copies **only** `res->ai_addr` (first). The others are freed by `freeaddrinfo(res)` and never used. A long ping session does **not** rotate or fail over to other addresses.

`ft_ping` is **IPv4-only** (`AF_INET`). It does not request or use AAAA (IPv6) records.

---

## `hints` — what we ask for

```c
memset(&hints, 0, sizeof(hints));
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_RAW;
hints.ai_protocol = IPPROTO_ICMP;
```

| Field | Value | Meaning |
|-------|-------|---------|
| (zeroed) | `0` | Unset fields = no extra preference |
| `ai_family` | `AF_INET` | IPv4 only |
| `ai_socktype` | `SOCK_RAW` | Address suitable for a **raw socket** (same as `socket()` in `socket.c`) |
| `ai_protocol` | `IPPROTO_ICMP` | ICMP protocol — matches `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)` |

These hints do not open a socket; they only filter what `getaddrinfo` returns.

### Socket types (context)

| Type | Constant | Typical protocol | Used by `ft_ping` |
|------|----------|------------------|-------------------|
| Stream | `SOCK_STREAM` | TCP (HTTP, SSH) | No |
| Datagram | `SOCK_DGRAM` | UDP (DNS, games) | No |
| Raw | `SOCK_RAW` | IP / ICMP directly | **Yes** — program builds ICMP; kernel adds IP header |

---

## Result structure: where `ai_addr` comes from

`getaddrinfo` **allocates** memory and builds a linked list. You do not create `ai_addr` yourself.

```
*res  ──►  struct addrinfo
               ├── ai_family    (e.g. AF_INET)
               ├── ai_socktype  (e.g. SOCK_RAW)
               ├── ai_protocol  (e.g. IPPROTO_ICMP)
               ├── ai_addr  ──►  struct sockaddr_in   ← binary address
               └── ai_next    ──►  next result (or NULL)
```

`ai_addr` is a field inside each `addrinfo` node. Its declared type is `struct sockaddr *` (generic). For IPv4 we cast:

```c
(struct sockaddr_in *)res->ai_addr
```

Inside `sockaddr_in`:

```
struct sockaddr_in {
    sa_family_t    sin_family;   /* AF_INET */
    in_port_t      sin_port;     /* port (unused for ICMP) */
    struct in_addr sin_addr;     /* 4-byte IPv4 address (network byte order) */
    ...
};
```

`ft_ping` copies only the **first** result:

```c
ping->dest_addr = *(struct sockaddr_in *)res->ai_addr;
```

If DNS returns multiple A records, only the first is used.

Always release the list when done:

```c
freeaddrinfo(res);
```

Call it on **both** success and failure paths that still hold a valid `res` (see `strdup` failure branch in the full code above).

---

## After `getaddrinfo`: three fields in `t_ping`

One call fills the destination three different ways:

```c
ping->dest_addr = *(struct sockaddr_in *)res->ai_addr;   /* binary → send */
inet_ntop(AF_INET, &ping->dest_addr.sin_addr,
    ping->ip_str, INET_ADDRSTRLEN);                        /* text → display */
ping->hostname = strdup(host);                             /* CLI → display */
```

### Comparison table: `hostname` vs `ip_str` vs `dest_addr`

| | `hostname` | `ip_str` | `dest_addr` |
|---|------------|----------|-------------|
| **C type** | `char *` | `char[INET_ADDRSTRLEN]` | `struct sockaddr_in` |
| **Source** | `strdup(host)` — copy of CLI argument | `inet_ntop()` from resolved address | `getaddrinfo` → `res->ai_addr` |
| **Format** | Human string as typed | Dotted IPv4 string | Binary IP + socket metadata |
| **Example** (`./ft_ping google.com`) | `google.com` | `142.250.185.46` | `sin_addr` = 4 bytes for that IP |
| **Example** (`./ft_ping 8.8.8.8`) | `8.8.8.8` | `8.8.8.8` | `sin_addr` = 4 bytes for 8.8.8.8 |
| **Used for** | Header and statistics **label** | IP in parentheses in header | **`sendto()`** on every probe |
| **Read by** | `print_header()`, `print_statistics()` | `print_header()` | `send_ping()` in `send.c` |
| **Network layer** | Not sent on the wire | Not sent on the wire | **Actual destination** for ICMP |
| **Changes during run** | No | No | No — fixed at startup |

### How they appear together

```bash
sudo ./ft_ping google.com
```

```
PING google.com (142.250.185.46): 56 data bytes
      ↑              ↑
  hostname         ip_str

Packets go to dest_addr (same IP as ip_str, but in binary form).
```

```bash
sudo ./ft_ping 8.8.8.8
```

```
PING 8.8.8.8 (8.8.8.8): 56 data bytes
```

Here `hostname` and `ip_str` look the same in text, but `dest_addr` is still the binary `sockaddr_in` used by `sendto()`.

### Data flow diagram

```
CLI:  ./ft_ping  google.com
                    │
                    ▼
            getaddrinfo("google.com", …)
                    │
        ┌───────────┼───────────────┐
        ▼           ▼               ▼
   hostname      ip_str         dest_addr
   strdup()    inet_ntop()    copy ai_addr
   (display)    (display)      (network)
        │           │               │
        ▼           ▼               ▼
  PING google.com (142.250.…)    sendto(…, &dest_addr, …)
```

---

## Line-by-line walkthrough

| Line(s) | What happens |
|---------|----------------|
| `struct addrinfo hints` | Empty form we fill before the call |
| `struct addrinfo *res` | Will point to the result list (allocated by libc) |
| `memset(&hints, 0, …)` | Zero all hint fields first |
| `hints.ai_family = AF_INET` | IPv4 only |
| `hints.ai_socktype = SOCK_RAW` | Raw socket |
| `hints.ai_protocol = IPPROTO_ICMP` | ICMP |
| `getaddrinfo(host, NULL, &hints, &res)` | Resolve; `NULL` = no port |
| `if (ret != 0) return (-1)` | Unknown host / parse error |
| `ping->dest_addr = *(…*)res->ai_addr` | Copy binary address for sending |
| `inet_ntop(…, ping->ip_str, …)` | Printable IP for the header |
| `ping->hostname = strdup(host)` | Keep original CLI string for output |
| `if (!ping->hostname) { freeaddrinfo; return -1 }` | OOM cleanup |
| `freeaddrinfo(res)` | Free list allocated by `getaddrinfo` |
| `return (0)` | Success |

---

## When it runs

```
main()
  └─ parse_args()
       ├─ getopt_long()          /* all flags first */
       └─ resolve_host(host)     /* first positional argument */
            └─ getaddrinfo()
```

| Command | `getaddrinfo` called? |
|---------|----------------------|
| `./ft_ping -?` | No — help exits before host |
| `./ft_ping 127.0.0.1` | Yes — usually parsed locally |
| `./ft_ping google.com` | Yes — DNS via OS resolver |
| Each echo reply | **No** |
| Long run after DNS change | **No** — `dest_addr` stays fixed |

---

## Error handling in `ft_ping`

| Situation | Behavior |
|-----------|----------|
| `getaddrinfo` fails | `resolve_host` returns `-1` → `unknown host <name>` |
| `strdup` fails | `freeaddrinfo(res)` then `-1` (same user message) |
| Missing host argument | `missing host operand` (before `getaddrinfo`) |
| Two hosts | `only one host allowed` |

DNS failure happens **before** raw socket creation — no root required to fail on a bad name.

---

## What `getaddrinfo` is **not** used for

| Feature | Uses `getaddrinfo`? |
|---------|---------------------|
| Reply line `bytes from …` | No — IP taken from received packet |
| Reverse DNS (name from IP) | No — `ft_ping` never calls `getnameinfo()` |
| `-n` flag | No effect — replies already show numeric IP |
| ICMP payload / checksum | No |
| TTL, TOS, `--ttl`, `-T` | No |

---

## `sendto()` connection

After resolution, every probe uses `dest_addr`:

```c
sendto(ping->sockfd, packet, pkt_sz, 0,
    (struct sockaddr *)&ping->dest_addr,
    sizeof(ping->dest_addr));
```

The kernel reads `sin_addr` from `dest_addr` and sets the **destination IP** on the outgoing IPv4 header. `hostname` and `ip_str` are never passed to `sendto()`.

---

## Manual pages

- `getaddrinfo(3)` — resolve host/service
- `freeaddrinfo(3)` — free result list
- `gai_strerror(3)` — error code to string
- `inet_ntop(3)` — binary IPv4 → text
