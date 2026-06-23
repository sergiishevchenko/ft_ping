# Network concepts in `ft_ping`

This folder explains the network concepts that matter most for understanding how **ft_ping** works:

| Document | Topic |
|----------|--------|
| [OSI-TCP-IP.md](OSI-TCP-IP.md) | OSI 7-layer and TCP/IP 4-layer models — where ping fits in the stack |
| [DNS.md](DNS.md) | DNS — forward lookup, no reverse DNS on replies |
| [GETADDRINFO.md](GETADDRINFO.md) | `getaddrinfo()` — full `resolve_host()` code, `ai_addr`, `hostname` / `ip_str` / `dest_addr` |
| [IPv4.md](IPv4.md) | IPv4 header — fields, layout, parsing, verbose dump |
| [ICMP.md](ICMP.md) | Internet Control Message Protocol — echo request/reply (detailed), errors, checksum |
| [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md) | ICMP identifier — why `getpid() & 0xFFFF`, filtering replies, collisions |
| [TTL.md](TTL.md) | Time To Live — hop limit, decrements, “Time to live exceeded” |
| [TOS.md](TOS.md) | Type of Service — QoS byte in the IP header, `-T` flag |
| [SIGNALS.md](SIGNALS.md) | SIGINT / Ctrl+C — `setup_signals`, `g_stop`, graceful shutdown |

**RFC standards:** [rfc/README.md](../rfc/README.md) — [791](../rfc/rfc791.txt), [792](../rfc/rfc792.txt), [1071](../rfc/rfc1071.txt), [1122](../rfc/rfc1122.txt).

Each page covers:

1. **What it is** — field or protocol role on the wire
2. **How it works** — packet layout and typical network behavior
3. **In ft_ping** — flags, source files, and what you see in output

For program structure and module map, see [ARCHITECTURE.md](../ARCHITECTURE.md). For CLI flags, see [FLAGS.md](../FLAGS.md). For RFC standards, see [rfc/](../rfc/README.md).
