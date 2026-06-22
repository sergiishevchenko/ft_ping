# DNS in `ft_ping`

**DNS** (Domain Name System) maps human-readable names (`google.com`) to IP addresses (`142.250.185.46`). **ft_ping** uses DNS **once at startup** to turn the CLI destination into an IPv4 address for sending probes. It never calls **reverse DNS** on incoming replies.

Implementation: `resolve_host()` in `srcs/dns.c`, called from `parse_args()` in `srcs/main.c`.

For a dedicated walkthrough of `getaddrinfo()`, the full `resolve_host()` source, `ai_addr`, socket types, and the `hostname` / `ip_str` / `dest_addr` comparison table, see **[GETADDRINFO.md](GETADDRINFO.md)**.

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

## `resolve_host()` and `getaddrinfo()`

See **[GETADDRINFO.md](GETADDRINFO.md)** for:

- Full `resolve_host()` source (`srcs/dns.c`)
- How `getaddrinfo()` works (numeric IP vs hostname, `hints`, `ai_addr`)
- Comparison table: `hostname` vs `ip_str` vs `dest_addr`
- Line-by-line walkthrough and `sendto()` connection

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
