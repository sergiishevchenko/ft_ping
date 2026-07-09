# Little-endian and big-endian

When a CPU stores a multi-byte integer (16, 32, or 64 bits), it must decide **which byte goes at the lowest memory address**. That choice is called **endianness**. Network protocols pick one layout for the wire; your machine may use another. **ft_ping** must convert at the boundary between C variables and packet buffers.

Related pages: [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md) (`htons` / `ntohs` in practice), [IPv4.md](IPv4.md) (header fields on the wire), [CHECKSUM.md](CHECKSUM.md) (why checksum math is byte-order independent).

---

## What endianness means

A **byte** is 8 bits. A **16-bit** value uses two bytes; a **32-bit** value uses four.

Endianness answers: for the number `0x12345678`, does memory look like `12 34 56 78` or `78 56 34 12`?

| Term | Meaning | Mnemonic |
|------|---------|----------|
| **Big-endian** | Most significant byte (MSB) at the **lowest** address | “Big end first” |
| **Little-endian** | Least significant byte (LSB) at the **lowest** address | “Little end first” |

The **numeric value** is the same; only the **byte layout in RAM** differs.

### Example: 16-bit value `0x1234`

| Endianness | Address → | Byte 0 | Byte 1 |
|------------|-----------|--------|--------|
| **Big-endian** | low → high | `0x12` (MSB) | `0x34` (LSB) |
| **Little-endian** | low → high | `0x34` (LSB) | `0x12` (MSB) |

### Example: 32-bit value `0x12345678`

| Endianness | Bytes in memory (low address first) |
|------------|-------------------------------------|
| **Big-endian** | `12 34 56 78` |
| **Little-endian** | `78 56 34 12` |

Single-byte fields (TTL, protocol, ICMP type/code) have no endian issue — one byte is one byte.

---

## Which systems use which order?

| Typical platform | Endianness |
|------------------|------------|
| x86 / x86_64 (Intel, AMD) | Little-endian |
| Apple Silicon (M1/M2/M3, …) | Little-endian |
| Many ARM boards (Raspberry Pi, …) | Little-endian |
| Classic MIPS, some PowerPC, SPARC (historical) | Often big-endian |
| **Internet protocols (IP, ICMP, TCP, UDP)** | **Big-endian on the wire** |

**Network byte order** is the name for big-endian layout in packets ([RFC 791](../rfc/rfc791.txt), [RFC 792](../rfc/rfc792.txt)). Every host must speak it on the wire, regardless of local CPU.

---

## Why it matters for `ft_ping`

`ft_ping` runs on a **little-endian** host in the common case (macOS, Linux on x86/ARM). Packet buffers and kernel APIs expose header fields as the bytes **on the wire** — big-endian.

C variables such as `ping->ident` and `ping->seq` live in **host byte order** (little-endian on typical dev machines). Writing them into an ICMP header without conversion would put the wrong bytes on the network.

### Failure without conversion

Suppose `ping->ident = 1` (`0x0001`):

| Location | Little-endian layout | What a peer reads |
|----------|----------------------|-------------------|
| `ping->ident` in RAM | `01 00` | (host variable — correct locally) |
| ICMP `id` on wire **without** `htons` | `01 00` | `0x0100` = **256** |
| ICMP `id` on wire **with** `htons` | `00 01` | `0x0001` = **1** |

Replies would never match `ping->ident`, and printed sequence numbers would be wrong.

---

## Host order vs network order

| Order | Where | Used for |
|-------|-------|----------|
| **Host byte order** | CPU registers, `ping->ident`, `ping->seq`, loop counters | Logic, comparisons between app variables |
| **Network byte order** | Bytes in `sendto` / `recvmsg` buffers, on the actual link | IP, ICMP, TCP, UDP header fields wider than 1 byte |

Rule: **convert at the boundary** — when copying between host variables and packet memory.

---

## Conversion functions (`<arpa/inet.h>`)

POSIX provides macros/functions that swap bytes when host ≠ network order, and do nothing when they already match (e.g. big-endian host).

| Function | Size | Direction | Typical use in `ft_ping` |
|----------|------|-----------|---------------------------|
| `htons(x)` | 16-bit | host → network | ICMP `id`, `seq` before send |
| `ntohs(x)` | 16-bit | network → host | ICMP `id`, `seq` after receive |
| `htonl(x)` | 32-bit | host → network | IPv4 addresses in some contexts |
| `ntohl(x)` | 32-bit | network → host | `ip_src` / `ip_dst` in verbose dump |

**s** = **short** (16 bits). **l** = **long** (32 bits, in BSD socket naming — not C `long` necessarily).

### Send path (`srcs/send.c`)

```c
ICMP_HDR_ID(icmp_hdr) = htons(ping->ident);
ICMP_HDR_SEQ(icmp_hdr) = htons(ping->seq);
```

Host-order counters go into the packet as wire-order fields.

### Receive path (`srcs/recv.c`)

```c
ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident
```

Wire-order field is converted before comparing to a host-order variable.

### Print path (`srcs/print.c`)

IP and ICMP multi-byte fields use `ntohs` / `ntohl` so humans see the same values that appear in RFC diagrams and `tcpdump`.

---

## Fields affected in `ft_ping`

| Protocol | Field | Size | Conversion |
|----------|-------|------|------------|
| ICMP | `id`, `seq` | 16-bit | `htons` send, `ntohs` recv/print |
| ICMP | `checksum` | 16-bit | Set by `checksum()`; see [CHECKSUM.md](CHECKSUM.md) |
| IPv4 | `ip_len`, `ip_id`, `ip_off`, `ip_sum` | 16-bit | `ntohs` when printing (`-v`) |
| IPv4 | `ip_src`, `ip_dst` | 32-bit | `ntohl` in verbose IP dump |
| IPv4 | `ip_ttl`, `ip_p`, version/IHL | 8-bit | No conversion |

For ICMP identifier semantics (PID, filtering), see [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md).

---

## Worked example: echo request `id = 1`, `seq = 0`

```
Logical values:  ident = 1 (0x0001),  seq = 0 (0x0000)

ping->ident / ping->seq in RAM (little-endian):
  ident:  01 00
  seq:    00 00

After htons, bytes in ICMP header (network / big-endian):
  id:     00 01
  seq:    00 00

Remote host or local stack reads id as 1.

Reply arrives; buffer still holds 00 01 at id offset.
ntohs(ICMP_HDR_ID(...)) → 1  →  matches ping->ident
```

---

## Big-endian hosts

On a CPU where host order **is** big-endian, `htons` and `ntohs` typically return the argument unchanged. Code still calls them so the same source works on every platform — never skip conversion because “it works on my laptop.”

---

## Endianness and the Internet checksum

The checksum algorithm ([RFC 1071](../rfc/rfc1071.txt)) reads 16-bit **words** from memory. On little-endian machines those words have swapped numeric values compared to big-endian wire diagrams — but the **final checksum bytes on the wire** are identical, and verification still yields zero on any architecture. See [CHECKSUM.md](CHECKSUM.md) for the full proof sketch.

Endianness matters for **interpreting** header integers; it does not break checksum correctness.

---

## Common mistakes

| Mistake | Symptom |
|---------|---------|
| Compare `ICMP_HDR_ID(hdr)` to `ping->ident` without `ntohs` | Replies ignored on little-endian hosts |
| Print `ICMP_HDR_SEQ` without `ntohs` | Wrong `icmp_seq=` in output |
| Assume `struct ip` fields are always host order | Inconsistent `-v` dumps; platform-dependent |
| Omit `htons` on send “because the value is small” | Values like `0x0100` encode wrong on the wire |
| Use `htonl` on a 16-bit field (or vice versa) | Wrong width — swaps wrong number of bytes |

### Rule of thumb

| Memory region | Byte order | Action |
|---------------|------------|--------|
| `ping->*` app state | Host | Use as-is in C |
| Outgoing ICMP buffer | Network | `htons` / `htonl` on write |
| Incoming packet buffer | Network | `ntohs` / `ntohl` on read |
| Comparisons and `printf` of header fields | Host | Convert from buffer first |

---

## Quick reference

```
  ┌─────────────────┐     htons/htonl      ┌─────────────────┐
  │  Host variables │ ──────────────────►  │  Packet buffer  │
  │  ping->ident    │                      │  (wire layout)  │
  │  ping->seq      │ ◄──────────────────  │                 │
  └─────────────────┘     ntohs/ntohl      └─────────────────┘
         ▲                                           │
         │                                           │
    C logic, stats                            sendto / recvmsg
```

| Concept | One-line summary |
|---------|------------------|
| **Little-endian** | LSB at lowest address (x86, Apple Silicon) |
| **Big-endian** | MSB at lowest address (network wire format) |
| **Network byte order** | Big-endian; required for IP/ICMP/TCP/UDP headers |
| **`htons` / `ntohs`** | 16-bit host ↔ network |
| **`htonl` / `ntohl`** | 32-bit host ↔ network |
