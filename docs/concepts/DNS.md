# DNS in `ft_ping`

**DNS** (Domain Name System) maps human-readable names (`google.com`) to IP addresses (`142.250.185.46`). **ft_ping** uses DNS **once at startup** to turn the CLI destination into an IPv4 address for sending probes. It never calls **reverse DNS** on incoming replies.

Implementation: `resolve_host()` in `srcs/dns.c`, called from `parse_args()` in `srcs/main.c`.

---

## What DNS does (overview)

Without DNS you would have to type numeric IPv4 addresses. DNS is a distributed database:

```
  Client                    DNS resolver              Authoritative servers
     |                            |                            |
     |  "What is google.com?"     |                            |
     |--------------------------->|  recursive query           |
     |                            |--------------------------->|
     |                            |<---------------------------|
     |<---------------------------|  A record: 142.250.185.46  |
```

| Record type | Meaning | Used by `ft_ping` |
|-------------|---------|-------------------|
| **A** | IPv4 address | **Yes** — `AF_INET` only |
| **AAAA** | IPv6 address | **No** — project is IPv4-only |
| **PTR** | Name for an IP (reverse) | **No** — not used on receive |

DNS lives at the **application** layer: your program calls a library API; the OS resolver talks to configured nameservers (`/etc/resolv.conf`, systemd-resolved, etc.).

See [OSI-TCP-IP.md](OSI-TCP-IP.md) for where naming fits in the stack.

---

## Forward vs reverse DNS

| Direction | Question | API (typical) | `ft_ping` |
|-----------|----------|---------------|-----------|
| **Forward** | What IP is `google.com`? | `getaddrinfo()` | **Once** in `resolve_host()` |
| **Reverse** | What name is `8.8.8.8`? | `getnameinfo()`, `gethostbyaddr()` | **Never** |

### Forward lookup (what we do)

```bash
sudo ./ft_ping google.com
```

1. You pass the string `google.com`.
2. `getaddrinfo("google.com", …)` returns an IPv4 address.
3. All ICMP packets go to that address.
4. Header shows what you typed: `PING google.com (142.250.185.46): 56 data bytes`.

### Reverse lookup (what we skip)

When a reply arrives, the packet contains only a **source IP**. System `ping` with default settings may call reverse DNS to print `bytes from lax17s32-in-f14.1e100.net`. **ft_ping** prints the IP directly:

```c
inet_ntoa(((struct sockaddr_in *)msg->msg_name)->sin_addr);   /* print.c */
```

So reply lines look like:

```
64 bytes from 142.250.185.46: icmp_seq=0 ttl=118 time=1.234 ms
```

The **`-n`** flag (numeric) is accepted for inetutils compatibility but does not change behavior — we already never reverse-resolve. Forward DNS still runs if the target is a hostname.

---

## When resolution happens

```
main()
  └─ parse_args()
       ├─ getopt_long()     /* all flags */
       └─ resolve_host()    /* first positional argument only */
```

| When | DNS? |
|------|------|
| `./ft_ping -?` | No — help exits before host |
| `./ft_ping 8.8.8.8` | `getaddrinfo` treats dotted-quad as numeric (usually no network query) |
| `./ft_ping google.com` | Forward lookup via resolver |
| Each echo reply | No |
| `Ctrl+C` / statistics | No |

Resolution is **not repeated** if the target’s IP changes in DNS during a long run — `dest_addr` is fixed at startup.

---

## `resolve_host()` step by step

Full source (`srcs/dns.c`):

```c
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

### 1. Zero `hints`

`memset(&hints, 0, sizeof(hints))` — unset fields mean “no preference” except where we set them explicitly.

### 2. Restrict to IPv4 ICMP

| Hint | Value | Why |
|------|-------|-----|
| `ai_family` | `AF_INET` | IPv4 only; no AAAA / IPv6 |
| `ai_socktype` | `SOCK_RAW` | Matches how we open the socket later |
| `ai_protocol` | `IPPROTO_ICMP` | ICMP echo, not TCP/UDP |

These hints tell the resolver which kind of address to return. `ft_ping` does not support IPv6.

### 3. `getaddrinfo(host, NULL, &hints, &res)`

Modern POSIX API replacing `gethostbyname()`.

| Argument | Value | Meaning |
|----------|-------|---------|
| `host` | CLI string | `"8.8.8.8"` or `"google.com"` |
| `service` | `NULL` | No port (ICMP has no ports) |
| `hints` | filters above | IPv4, raw, ICMP |
| `res` | out pointer | Linked list of results |

On success, `res->ai_addr` points to a `struct sockaddr_in` ready for `sendto()`.

On failure (`ret != 0`), `parse_args()` prints `unknown host` and exits. `gai_strerror(ret)` is **not** printed — inetutils-style simple message.

### 4. Copy address → `dest_addr`

```c
ping->dest_addr = *(struct sockaddr_in *)res->ai_addr;
```

Used on every `sendto()` in `send_ping()`:

```c
sendto(ping->sockfd, packet, pkt_sz, 0,
    (struct sockaddr *)&ping->dest_addr, sizeof(ping->dest_addr));
```

### 5. Dotted string → `ip_str`

```c
inet_ntop(AF_INET, &ping->dest_addr.sin_addr, ping->ip_str, INET_ADDRSTRLEN);
```

Thread-safe conversion to printable form (`"142.250.185.46"`) for the header line. Buffer size `INET_ADDRSTRLEN` (16) fits any IPv4 string.

### 6. Save original CLI string → `hostname`

```c
ping->hostname = strdup(host);
```

Keeps **exactly what the user typed** for output:

```
PING google.com (142.250.185.46): 56 data bytes
--- google.com ping statistics ---
```

If you pass `8.8.8.8`, both `hostname` and `ip_str` show the same address.

### 7. `freeaddrinfo(res)`

Release the linked list allocated by `getaddrinfo()`. Must be called even after copying the address.

---

## Three related fields in `t_ping`

| Field | Content | Example (`google.com`) | Used for |
|-------|---------|------------------------|----------|
| `hostname` | CLI argument (strdup) | `google.com` | Header, statistics title |
| `ip_str` | Resolved IPv4 text | `142.250.185.46` | Header parenthesis |
| `dest_addr` | Binary `sockaddr_in` | `sin_addr` in network order | `sendto()` destination |

```
User types:  google.com
                 │
                 ▼
         resolve_host()
                 │
     ┌───────────┼───────────┐
     ▼           ▼           ▼
 hostname     ip_str     dest_addr
 (display)   (display)   (network)
```

---

## Numeric IP vs hostname

`getaddrinfo()` accepts both:

| Input | Typical resolver behavior |
|-------|---------------------------|
| `127.0.0.1` | Parsed as IPv4 literal; often **no DNS query** |
| `google.com` | DNS **A** record lookup |
| Invalid name | `getaddrinfo` fails → `unknown host` |

This is why `sudo ./ft_ping 127.0.0.1` works offline while `google.com` needs working DNS (or `/etc/hosts` entry).

---

## `/etc/hosts` and local resolver

Before querying the Internet, the system resolver usually checks:

1. **`/etc/hosts`** — static mappings (`127.0.0.1 localhost`)
2. **DNS** — configured nameservers
3. **mDNS / LLMNR** — on some setups (platform-dependent)

`ft_ping` does not read these files directly — it only calls `getaddrinfo()`, which uses the OS resolver policy (`nsswitch.conf` on Linux).

---

## Error cases

| Situation | Message | Exit |
|-----------|---------|------|
| No positional argument | `missing host operand` | non-zero |
| `getaddrinfo` fails | `unknown host <name>` | non-zero |
| Two hostnames | `only one host allowed` | non-zero |
| `strdup` fails | `unknown host` (same path as DNS fail) | non-zero |

DNS failure happens **before** socket creation — no root needed to fail on bad hostname (but you still need root to ping once parsing succeeds).

---

## What DNS is *not* involved in

| Feature | DNS role |
|---------|----------|
| ICMP echo payload | None |
| TTL / TOS / `--ttl` / `-T` | None |
| Reply `bytes from …` | IP only, no lookup |
| `-v` IP Hdr Dump | None |
| ICMP error filtering (`inner_ip->ip_dst`) | Compares binary IP to `dest_addr`, not names |
| Verbose inner host names | None |

---

## Flow diagram

```
  argv:  ./ft_ping  -c 3  google.com
                          │
                          ▼
                   parse_args()
                          │
              getopt_long (-c → count=3)
                          │
                          ▼
              resolve_host("google.com")
                          │
              getaddrinfo(AF_INET, …)
                          │
         ┌────────────────┴────────────────┐
         ▼                                 ▼
    success                           failure
         │                                 │
         ▼                                 ▼
 dest_addr = 142.250.…              unknown host → exit
 ip_str    = "142.250.…"
 hostname  = "google.com"
         │
         ▼
   create_socket() → ping_loop() → sendto(dest_addr)
         │
         ▼
   recv: print IP from packet (no reverse DNS)
```

---

## Comparison with inetutils `ping`

| Behavior | inetutils `ping` | `ft_ping` |
|----------|------------------|-----------|
| Forward DNS for target | Yes | Yes (`getaddrinfo`) |
| Reverse DNS on replies | Optional (default often on) | **Never** |
| `-n` | Disables reverse DNS in replies | Accepted; no effect on replies |
| Header with hostname | `PING host (ip)` | Same |
| IPv6 | Supported | **Not supported** |

**man pages:** `getaddrinfo(3)`, `freeaddrinfo(3)`, `inet_ntop(3)`, `gai_strerror(3)`.
