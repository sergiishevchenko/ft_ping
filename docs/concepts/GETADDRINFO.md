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

### Case A: numeric IPv4 (`8.8.8.8`, `127.0.0.1`)

```
"8.8.8.8"
    │
    ▼
getaddrinfo parses the string (no DNS network query in the common case)
    │
    ▼
builds struct sockaddr_in  →  linked list in *res
```

### Case B: hostname (`google.com`)

```
"google.com"
    │
    ▼
OS resolver (reads /etc/nsswitch.conf, /etc/hosts, DNS servers, …)
    │
    ▼
DNS A record lookup → IPv4 address
    │
    ▼
builds struct sockaddr_in  →  linked list in *res
```

`ft_ping` is **IPv4-only** (`AF_INET`). It does not request AAAA (IPv6) records.

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
