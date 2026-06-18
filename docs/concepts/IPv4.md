# IPv4 header

Every **ft_ping** probe travels inside an **IPv4 packet**. On send, your code supplies only the ICMP message; the **kernel builds the IP header**. On receive, a raw socket delivers **IP + ICMP**, and the program parses `struct ip` before touching ICMP.

This page describes the IPv4 header layout (RFC 791), field by field, and how **ft_ping** reads or configures each part.

---

## Position in the packet

```
┌────────────────────────────────────────────────────────────┐
│  IPv4 header (20 bytes minimum, up to 60 with options)     │
├────────────────────────────────────────────────────────────┤
│  ICMP message (8-byte header + payload)                    │
└────────────────────────────────────────────────────────────┘
```

| Direction | Who builds the IP header | What `ft_ping` sees |
|-----------|--------------------------|---------------------|
| **Send** | Kernel | Nothing — only ICMP passed to `sendto()` |
| **Receive** | Remote host or router | Full buffer: IP first, then ICMP |

See [OSI-TCP-IP.md](OSI-TCP-IP.md) for where IP sits in the stack.

---

## Full header layout (20-byte base)

All multi-byte integers on the wire are **big-endian** (network byte order).

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|      Fragment Offset    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |         Header Checksum       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Source Address                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination Address                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options (if IHL > 5)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Field summary

| Byte(s) | Field | `struct ip` (Linux/BSD) | Size | Role |
|---------|-------|-------------------------|------|------|
| 0 | Version + IHL | `ip_v`, `ip_hl` | 4 + 4 bits | IPv4 = 4; IHL = header length in 32-bit words |
| 1 | Type of Service | `ip_tos` | 1 | QoS / DSCP hints — [TOS.md](TOS.md) |
| 2–3 | Total Length | `ip_len` | 2 | Entire IP datagram (header + payload) |
| 4–5 | Identification | `ip_id` | 2 | Fragment reassembly ID |
| 6–7 | Flags + Fragment Offset | `ip_off` | 2 | DF/MF flags + fragment position |
| 8 | Time to Live | `ip_ttl` | 1 | Hop limit — [TTL.md](TTL.md) |
| 9 | Protocol | `ip_p` | 1 | Upper-layer protocol (`1` = ICMP) |
| 10–11 | Header Checksum | `ip_sum` | 2 | Checksum over IP header only |
| 12–15 | Source Address | `ip_src` | 4 | Sender IPv4 |
| 16–19 | Destination Address | `ip_dst` | 4 | Receiver IPv4 |
| 20+ | Options | (after fixed 20 bytes) | 0–40 | Timestamp, record route, etc. |

**Header length in bytes:**

```c
ip_hdr_len = ip_hdr->ip_hl << 2;   /* recv.c, print.c */
```

Minimum `ip_hl` is **5** → 5 × 4 = **20 bytes**. Maximum with options is **15** → **60 bytes**.

---

## Version (4 bits)

| Value | Meaning |
|-------|---------|
| **4** | IPv4 |
| **6** | IPv6 (different header — not used by `ft_ping`) |

Printed in verbose dumps as `Vr` (version): `print_ip_header_dump()` uses `ip_hdr->ip_v`.

---

## IHL — Internet Header Length (4 bits)

Number of **32-bit words** in the IP header, including options.

| IHL | Header size |
|-----|-------------|
| 5 | 20 bytes (no options) |
| 6 | 24 bytes (4 bytes of options) |
| … | … |
| 15 | 60 bytes (max options) |

**Critical for parsing:** ICMP does not start at a fixed offset. Always compute:

```c
icmp_hdr = (t_icmphdr *)(buf + (ip_hdr->ip_hl << 2));
```

Never assume ICMP begins at byte 20 unless you know `ip_hl == 5`.

---

## Type of Service (`ip_tos`)

One byte for precedence / DSCP / ECN. Configured with **`-T`** via `setsockopt(IP_TOS)` — see [TOS.md](TOS.md).

Verbose dump column: `TOS` → `%02x` from `ip_hdr->ip_tos`.

---

## Total Length (`ip_len`)

Length of the **whole IP datagram** in bytes: IP header + everything after it (ICMP header + ICMP data for ping).

- On the wire: big-endian.
- Linux `struct ip` may store `ip_len` in host or network form depending on context; `print_ip_header_dump()` uses a heuristic (`ip_len > 0x2000` → `ntohs`) when printing.

For echo replies, `print_echo_reply()` does **not** print total length; it prints ICMP-level size:

```c
datalen = bytes_recv - ip_hdr_len;   /* ICMP + payload only */
```

---

## Identification (`ip_id`)

16-bit value assigned by the sender. Routers use **ID + src + dst + protocol** to group fragments of the same datagram.

Ping packets are small (default ~84 bytes IP total) and rarely fragmented, so `ip_id` is usually invisible in normal output. It appears in **`-v`** `IP Hdr Dump:` as `ID`.

---

## Flags and Fragment Offset (`ip_off`)

The 16-bit field splits into:

```
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|R|D|F|    Fragment Offset      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Bits | Name | Meaning |
|------|------|---------|
| 0–12 | Fragment Offset | Position of this fragment in 8-byte units |
| 13 | RF | Reserved (zero) |
| 14 | **DF** | Don’t Fragment — drop if MTU too small |
| 15 | **MF** | More Fragments follow |

`print_ip_header_dump()` prints:

- `Flg` — top 3 bits of `ntohs(ip_off) & 0xe000`
- `off` — low 13 bits (`& 0x1fff`)

ICMP **code 1** under Time Exceeded (“Frag reassembly time exceeded”) relates to fragmented packets that never completed reassembly.

---

## Time to Live (`ip_ttl`)

Hop counter, decremented by each router. At 0 → packet dropped, often **ICMP Time Exceeded**.

| In `ft_ping` | Detail |
|--------------|--------|
| Outgoing | `--ttl` → `setsockopt(IP_TTL)` |
| Incoming reply | `ttl=%d` from `ip_hdr->ip_ttl` in `print_echo_reply()` |

Full behavior: [TTL.md](TTL.md).

---

## Protocol (`ip_p`)

Identifies the payload protocol:

| Value | Protocol | `ft_ping` |
|-------|----------|-----------|
| **1** | ICMP | Echo request/reply and errors |
| 6 | TCP | Shown only in verbose inner dumps |
| 17 | UDP | Shown only in verbose inner dumps |

Outgoing probes always use ICMP; the kernel sets `ip_p = IPPROTO_ICMP`.

---

## Header Checksum (`ip_sum`)

Covers **only the IP header** (not ICMP). Recalculated when TTL changes in transit.

Algorithm: same RFC 1071 style as ICMP checksum. Computed by the kernel on send and by each router that decrements TTL.

Verbose dump: `cks` → `ntohs(ip_hdr->ip_sum)`.

---

## Source and Destination Address

Four bytes each (`struct in_addr` / `in_addr_t`).

| Field | Outgoing echo request | Incoming echo reply |
|-------|----------------------|---------------------|
| **Source** | Your host’s IP | Remote host (shown as `bytes from X`) |
| **Destination** | Target (`ping->dest_addr`) | Your host |

**DNS:** CLI hostname is resolved once to `dest_addr` / `ip_str` in `resolve_host()`. Reply lines use the **source IP from the packet**, not reverse DNS.

**ICMP errors:** `print_icmp_error()` compares the **inner** (quoted) packet’s `ip_dst` to `ping->dest_addr` to filter unrelated errors:

```c
inner_ip = (struct ip *)((uint8_t *)icmp_hdr + PING_PKT_HDR_SZ);
if (inner_ip->ip_dst.s_addr != ping->dest_addr.sin_addr.s_addr)
    return;   /* without -v */
```

---

## IP options (optional tail)

If `ip_hl > 5`, bytes 20 … `(ip_hl * 4 - 1)` hold **options**. Common types:

| Option | Value | `ft_ping` flag |
|--------|-------|----------------|
| End of options | `IPOPT_EOL` | — |
| No operation | `IPOPT_NOP` | — |
| Record Route | `IPOPT_RR` | — |
| Timestamp | `IPOPT_TS` | `--ip-timestamp tsonly` / `tsaddr` |

`set_ip_timestamp()` in `srcs/socket.c` builds an option buffer and passes it with `setsockopt(IP_OPTIONS)`.

On echo replies, `print_ip_opt()` in `srcs/print.c` walks options after the fixed 20-byte header and may print:

```
TS:
    12345 ms
RR:
    192.168.1.1
```

---

## Linux `struct ip` and parsing discipline

`ft_ping` includes `<netinet/ip.h>`. Typical usage:

```c
struct ip *ip_hdr = (struct ip *)buf;
int ip_hdr_len = ip_hdr->ip_hl << 2;
t_icmphdr *icmp_hdr = (t_icmphdr *)(buf + ip_hdr_len);
```

| Rule | Why |
|------|-----|
| Cast buffer to `struct ip *` | First bytes of `recvmsg` buffer are the IP header |
| Use `ip_hl << 2` | ICMP offset depends on options |
| Use `ntohs()` / `ntohs()` for multi-byte fields when comparing or printing wire values | Network byte order |
| Check `bytes >= ip_hdr_len + PING_PKT_HDR_SZ` | Avoid reading past buffer on truncated packets |

---

## Verbose output: `IP Hdr Dump`

With **`-v`** and ICMP errors, `print_ip_header_dump()` prints inetutils-style output:

```
IP Hdr Dump:
 4500 003c 1a2b 4000 4001 0000 c0a80101 08080808
Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src     Dst     Data
 4  5  00  003c 1a2b   4 0000  40  01 f7d2 192.168.1.1  8.8.8.8
```

| Column | Field |
|--------|-------|
| `Vr` | Version (`ip_v`) |
| `HL` | IHL (`ip_hl`) |
| `TOS` | `ip_tos` |
| `Len` | Total length |
| `ID` | Identification |
| `Flg` | DF/MF flags |
| `off` | Fragment offset |
| `TTL` | `ip_ttl` |
| `Pro` | Protocol (`ip_p`) |
| `cks` | Header checksum |
| `Src` / `Dst` | IPv4 addresses |
| `Data` | Option bytes (hex) |

For errors, the dump usually shows the **inner** quoted IP header (your original probe), not the outer router packet.

---

## Send path: what the kernel fills

`sendto(sockfd, icmp_packet, …)` — kernel supplies:

| Field | Typical source |
|-------|----------------|
| `ip_v`, `ip_hl` | 4, 5 (or larger if `IP_OPTIONS` set) |
| `ip_tos` | `-T` or default 0 |
| `ip_len` | Header + ICMP size |
| `ip_id` | Kernel counter |
| `ip_off` | Often DF set on Linux |
| `ip_ttl` | `--ttl` (default 64) |
| `ip_p` | `IPPROTO_ICMP` (1) |
| `ip_sum` | Kernel |
| `ip_src` | Chosen route / interface |
| `ip_dst` | `dest_addr` from `sendto` |

Your code never writes these bytes directly on send.

---

## Receive path: what `ft_ping` reads

| Location | Fields used |
|----------|-------------|
| `recv_ping()` | `ip_hl` → locate ICMP |
| `print_echo_reply()` | `ip_ttl`; options via `print_ip_opt()` |
| `print_icmp_error()` | Outer length; inner `ip_dst` for filtering |
| `print_inner_ip_data()` | Full inner header dump + `ip_p` for protocol hint |

---

## Typical sizes for default ping

| Part | Default size |
|------|----------------|
| IP header (no options) | 20 bytes |
| ICMP header | 8 bytes |
| ICMP data | 56 bytes |
| **IP total length** | 20 + 8 + 56 = **84 bytes** |

Reply line `64 bytes from …` is **ICMP only** (8 + 56), not including the 20-byte IP header.

---

## Quick reference: source files

| File | IPv4-related code |
|------|-------------------|
| `srcs/socket.c` | `IP_TTL`, `IP_TOS`, `IP_OPTIONS` (timestamp) |
| `srcs/recv.c` | Parse `struct ip`, compute `ip_hdr_len` |
| `srcs/print.c` | `ttl=` on replies; `print_ip_header_dump`, `print_ip_opt` |
| `srcs/dns.c` | Resolve hostname → destination IPv4 |
| `includes/ft_ping.h` | `ip_str`, `dest_addr` |

**RFC 791** — Internet Protocol (IPv4 header definition).
