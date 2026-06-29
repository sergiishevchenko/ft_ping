# RFC standards (full text)

Official RFC texts from [RFC Editor](https://www.rfc-editor.org/), stored as plain `.txt` files.

| RFC | File | Title | Date |
|-----|------|-------|------|
| 791 | [rfc791.txt](rfc791.txt) | Internet Protocol (IPv4) | Sep 1981 |
| 792 | [rfc792.txt](rfc792.txt) | Internet Control Message Protocol | Sep 1981 |
| 1071 | [rfc1071.txt](rfc1071.txt) | Computing the Internet Checksum | Sep 1988 |
| 1122 | [rfc1122.txt](rfc1122.txt) | Requirements for Internet Hosts — Communication Layers | Oct 1989 |
| 1624 | [rfc1624.txt](rfc1624.txt) | Computation of the Internet Checksum via Incremental Update | May 1994 |

**Online:** https://datatracker.ietf.org/doc/html/rfc{N}

**Relevance in `ft_ping`:**

| RFC | What it defines | Where in the project |
|-----|-----------------|----------------------|
| 791 | IPv4 header, TTL, TOS, IP options | `socket.c`, `recv.c`, `print.c` — see [IPv4.md](../concepts/IPv4.md) |
| 792 | ICMP Echo Request/Reply, errors | `send.c`, `recv.c`, `print.c` — see [ICMP.md](../concepts/ICMP.md) |
| 1071 | One's complement checksum | `checksum.c` |
| 1122 | Host must reply to ICMP echo | behavior of remote hosts being probed |
| 1624 | Incremental checksum update (TTL/NAT) | background in [CHECKSUM.md](../concepts/CHECKSUM.md) |

Concept pages: [IPv4](../concepts/IPv4.md), [ICMP](../concepts/ICMP.md), [TTL](../concepts/TTL.md), [TOS](../concepts/TOS.md).
