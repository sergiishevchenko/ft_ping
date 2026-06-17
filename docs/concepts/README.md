# Network concepts in `ft_ping`

This folder explains the three IPv4/ICMP building blocks that matter most for understanding how **ft_ping** works:

| Document | Topic |
|----------|--------|
| [ICMP.md](ICMP.md) | Internet Control Message Protocol — echo request/reply (detailed), errors, checksum |
| [TTL.md](TTL.md) | Time To Live — hop limit, decrements, “Time to live exceeded” |
| [TOS.md](TOS.md) | Type of Service — QoS byte in the IP header, `-T` flag |

Each page covers:

1. **What it is** — field or protocol role on the wire
2. **How it works** — packet layout and typical network behavior
3. **In ft_ping** — flags, source files, and what you see in output

For program structure and module map, see [ARCHITECTURE.md](../ARCHITECTURE.md). For CLI flags, see [FLAGS.md](../FLAGS.md).
