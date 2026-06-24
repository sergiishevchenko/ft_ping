# Internet Checksum (RFC 1071)

The **Internet Checksum** is a 16-bit integrity check used in IPv4, ICMP, TCP, and UDP headers. It detects bit-level corruption introduced during transmission — flipped bits in cables, faulty router memory, electromagnetic interference, etc. If a received packet's checksum does not verify, the packet is silently discarded.

In **ft_ping**, the checksum covers the entire ICMP message (8-byte header + payload). It is computed in `checksum.c` and called from `send.c` before every `sendto()`.

---

## Algorithm overview

The algorithm (defined in [RFC 1071](../rfc/rfc1071.txt)) works in **one's complement arithmetic**:

1. Treat the data as a sequence of **16-bit words**.
2. Sum all words into a **32-bit accumulator** (to capture carries).
3. If the data has an **odd** number of bytes, pad the last byte with a zero byte and add it.
4. **Fold** the 32-bit sum into 16 bits by adding the upper 16 bits (carry) back into the lower 16 bits — twice, to handle a carry produced by the first fold.
5. Return the **bitwise complement** (`~`) of the result.

---

## Implementation in ft_ping

```c
uint16_t  checksum(void *data, size_t len)
{
    uint16_t  *ptr;
    uint32_t  sum;

    ptr = (uint16_t *)data;
    sum = 0;
    while (len > 1)
    {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1)
        sum += *(uint8_t *)ptr;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ((uint16_t)~sum);
}
```

---

## Real example — full binary walkthrough

### The packet

An ICMP Echo Request to `8.8.8.8`, id = `0x1234`, seq = `0x0001`, with 4 bytes of payload `DE AD BE EF`. Total 12 bytes. Before checksum computation, the checksum field is zeroed:

```
Byte offset:  00   01   02   03   04   05   06   07   08   09   0A   0B
              ─────────────────────────────────────────────────────────────
Hex:          08   00   00   00   12   34   00   01   DE   AD   BE   EF
              │    │    │    │    │    │    │    │    └───────────────────┘
              type code checksum  identifier  sequence     payload
                        (= 0)
```

Binary representation of every byte:

```
Offset  Hex   Binary
  00    0x08  0000 1000      ← type = 8 (Echo Request)
  01    0x00  0000 0000      ← code = 0
  02    0x00  0000 0000      ← checksum high byte (zeroed)
  03    0x00  0000 0000      ← checksum low byte  (zeroed)
  04    0x12  0001 0010      ← id high byte
  05    0x34  0011 0100      ← id low byte
  06    0x00  0000 0000      ← seq high byte
  07    0x01  0000 0001      ← seq low byte
  08    0xDE  1101 1110      ← payload byte 0
  09    0xAD  1010 1101      ← payload byte 1
  0A    0xBE  1011 1110      ← payload byte 2
  0B    0xEF  1110 1111      ← payload byte 3
```

---

### Step 1 — Split into 16-bit words and sum

The pointer `ptr` is cast to `uint16_t *`. Each iteration reads **2 bytes** and adds them to the 32-bit accumulator `sum`. On the wire, the protocol defines fields in **big-endian** (network byte order), but the checksum algorithm reads raw memory, so on a **little-endian** CPU (x86, ARM) the byte pair is swapped inside the 16-bit register:

```
                     Memory layout          CPU reads as uint16_t
                     (big-endian wire)       (little-endian)
                     ───────────────         ─────────────────────
Word 0: bytes [08, 00]  → 0x0800 on wire  → 0x0008 in register
Word 1: bytes [00, 00]  → 0x0000 on wire  → 0x0000 in register
Word 2: bytes [12, 34]  → 0x1234 on wire  → 0x3412 in register
Word 3: bytes [00, 01]  → 0x0001 on wire  → 0x0100 in register
Word 4: bytes [DE, AD]  → 0xDEAD on wire  → 0xADDE in register
Word 5: bytes [BE, EF]  → 0xBEEF on wire  → 0xEFBE in register
```

> **Why does endianness not matter?** The one's complement sum is byte-order independent. On a big-endian machine the individual word values differ, but the final complemented checksum, when stored back into memory, produces exactly the same two bytes. Verification yields 0 on both architectures. This is a key property proven in RFC 1071 Appendix B.

Now the summation in binary, step by step (using the big-endian wire values for clarity — the math is identical either way):

```
                          Binary                           Hex
                          ───────────────────────────────   ──────────
  sum  = 0000 0000 0000 0000 0000 0000 0000 0000           0x0000_0000
+ W0     0000 0000 0000 0000 0000 1000 0000 0000           0x0000_0800
= sum    0000 0000 0000 0000 0000 1000 0000 0000           0x0000_0800

+ W1     0000 0000 0000 0000 0000 0000 0000 0000           0x0000_0000
= sum    0000 0000 0000 0000 0000 1000 0000 0000           0x0000_0800

+ W2     0000 0000 0000 0000 0001 0010 0011 0100           0x0000_1234
= sum    0000 0000 0000 0000 0001 1010 0011 0100           0x0000_1A34

+ W3     0000 0000 0000 0000 0000 0000 0000 0001           0x0000_0001
= sum    0000 0000 0000 0000 0001 1010 0011 0101           0x0000_1A35

+ W4     0000 0000 0000 0000 1101 1110 1010 1101           0x0000_DEAD
= sum    0000 0000 0000 0000 1111 1000 1110 0010           0x0000_F8E2

+ W5     0000 0000 0000 0000 1011 1110 1110 1111           0x0000_BEEF
= sum    0000 0000 0000 0001 1011 0111 1101 0001           0x0001_B7D1
                         ↑
                   CARRY! bit 16 is set — the sum overflowed 16 bits
```

The accumulator is `uint32_t` (32 bits), so the carry is safely stored in bit 16. A `uint16_t` accumulator would have lost it.

---

### Step 2 — Handle odd byte (when length is not even)

```c
if (len == 1)
    sum += *(uint8_t *)ptr;
```

If the data had 13 bytes instead of 12, after the while-loop `len` would be 1 and there would be one byte left (say `0x42`). It is read as `uint8_t` and added:

```
Last byte: 0x42 = 0100 0010

Added to sum as: 0000 0000 0100 0010   (zero-padded to 16 bits)
                                ^^^^
                           only the low byte is meaningful
```

In our 12-byte example `len` reaches 0 after the loop, so this branch is skipped.

---

### Step 3 — Folding carries into 16 bits

In **one's complement arithmetic**, there is no "overflow" — a carry out of the most significant bit wraps around and is added back to the least significant bit. The 32-bit accumulator captured all carries in bits 16–31. Now we fold them back:

```c
sum = (sum >> 16) + (sum & 0xFFFF);   // first fold
sum += (sum >> 16);                    // second fold
```

#### First fold

```
sum = 0x0001_B7D1

Decompose into upper and lower 16 bits:

  sum >> 16:     0000 0000 0000 0000 | 0000 0000 0000 0001     = 0x0001
                                       ↑ this is the carry

  sum & 0xFFFF:  0000 0000 0000 0000 | 1011 0111 1101 0001     = 0xB7D1
                                       ↑ the "real" 16-bit sum

Add them:

    0000 0000 0000 0001    0x0001  (carry)
  + 1011 0111 1101 0001    0xB7D1  (lower)
  ─────────────────────
    1011 0111 1101 0010    0xB7D2  ← no new carry (bit 16 = 0)
```

#### Why a second fold is needed

The first fold can itself produce a new carry. Consider a different sum:

```
sum = 0x0002_FFFF

First fold:

    0000 0000 0000 0010    0x0002
  + 1111 1111 1111 1111    0xFFFF
  ─────────────────────
  1 0000 0000 0000 0001    0x1_0001  ← NEW CARRY in bit 16!
  ↑

Second fold needed:

    0000 0000 0000 0001    0x0001  (new carry)
  + 0000 0000 0000 0001    0x0001  (lower)
  ─────────────────────
    0000 0000 0000 0010    0x0002  ← clean 16-bit result

Without the second fold, the result would be 0x1_0001 — a 17-bit number,
and (uint16_t)~sum would invert only the low 16 bits, producing a wrong checksum.
```

A third fold is never needed: the maximum value after the first fold is `0xFFFF + 0x0001 = 0x1_0000`, so the second fold adds at most 1.

#### Back to our example

```
sum = 0xB7D2 after first fold

Second fold:
  sum >> 16 = 0x0000  (no carry)
  sum = 0xB7D2 + 0x0000 = 0xB7D2
```

Final 16-bit sum: `0xB7D2`.

---

### Step 4 — Bitwise complement (inversion)

```c
return ((uint16_t)~sum);
```

The `~` operator flips every bit — each `0` becomes `1` and each `1` becomes `0`:

```
sum  = 0xB7D2 = 1011 0111 1101 0010
                ││││ ││││ ││││ ││││
                ↓↓↓↓ ↓↓↓↓ ↓↓↓↓ ↓↓↓↓    flip every bit
~sum = 0x482D = 0100 1000 0010 1101
```

Bit-by-bit:

```
Position:  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
           ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ───
sum:        1   0   1   1   0   1   1   1   1   1   0   1   0   0   1   0
~sum:       0   1   0   0   1   0   0   0   0   0   1   0   1   1   0   1
```

**Result: `0x482D`** — this is the checksum written into the ICMP header before sending.

---

### Step 5 — Verification (receiver side)

The receiver gets the packet with checksum `0x482D` already in place and runs the **same function** over the entire message. The checksum field is **not** zeroed — it participates in the sum:

```
Packet as received:

Offset:  00   01   02   03   04   05   06   07   08   09   0A   0B
Hex:     08   00   48   2D   12   34   00   01   DE   AD   BE   EF
                   ↑↑   ↑↑
              checksum = 0x482D (was 0x0000 during computation)

Words (big-endian wire order):
  W0 = 0x0800
  W1 = 0x482D   ← the checksum itself
  W2 = 0x1234
  W3 = 0x0001
  W4 = 0xDEAD
  W5 = 0xBEEF
```

Sum step by step:

```
  0x0800
+ 0x482D = 0x502D
+ 0x1234 = 0x6261
+ 0x0001 = 0x6262
+ 0xDEAD = 0x1_410F   ← carry
+ 0xBEEF = 0x1_FFFE   ← carry
```

Fold:

```
First fold:   0x0001 + 0xFFFE = 0xFFFF
Second fold:  0x0000 + 0xFFFF = 0xFFFF

~0xFFFF:

  sum  = 1111 1111 1111 1111
  ~sum = 0000 0000 0000 0000 = 0x0000  ✓
```

**Result is `0x0000`** — the packet is intact. If even a single bit had flipped during transit, the sum would not be `0xFFFF` and the result would be non-zero → packet discarded.

---

### What happens if a bit flips?

Say byte 08 (`DE`) at offset 0x08 got corrupted: bit 5 flipped from 1 to 0:

```
Original: 0xDE = 1101 1110
                      ↑ bit 5
Corrupted: 0xBE = 1011 1110  (bit 5: 1→0, lost 0x20)
                      ↑ flipped

Word 4 becomes 0xBEAD instead of 0xDEAD → sum decreases by 0x2000.
Final sum ≠ 0xFFFF → ~sum ≠ 0x0000 → PACKET DISCARDED.
```

---

## Odd-length example (13 bytes)

Same packet + one extra payload byte `0x42`:

```
Bytes: 08 00 00 00 12 34 00 01 DE AD BE EF 42
       ├──── 6 words (12 bytes) ────────────┤ ↑
                                              1 byte left
```

After the while-loop, `len = 1`. The remaining byte is added:

```
sum after 6 words = 0x0001_B7D1 (same as before)

Odd byte: 0x42 = 0100 0010

  0x0001_B7D1
+ 0x0000_0042     (uint8_t, zero-padded to 16 bits)
= 0x0001_B813

Fold 1:  0x0001 + 0xB813 = 0xB814
Fold 2:  0x0000 + 0xB814 = 0xB814

~0xB814 = 0x47EB  ← checksum for the 13-byte message
```

---

## Why one's complement?

| Property | Benefit |
|----------|---------|
| **Byte-order independent** | The one's complement sum of 16-bit words produces the same checksum on big-endian and little-endian machines (the byte-swapped sum, when complemented, gives the byte-swapped checksum — verification still yields 0). |
| **Simple hardware** | A single pass with an adder and carry feedback — no multiplication, no lookup tables. |
| **Incremental update** | When a router decrements TTL or NAT rewrites an address, it can adjust the checksum arithmetically without re-scanning the entire header (RFC 1624). |
| **Zero detection** | In one's complement, `0x0000` and `0xFFFF` are both representations of zero. The complement step ensures the transmitted checksum is never all-zeros (which would be ambiguous with "no checksum computed"). |

---

## Usage in ft_ping

In `send.c`, checksum is called once per probe:

```c
ICMP_HDR_CKSUM(icmp_hdr) = 0;                       // zero the field first
ICMP_HDR_CKSUM(icmp_hdr) = checksum(packet, pkt_sz); // compute over header + payload
```

The checksum covers:

| Component | Size |
|-----------|------|
| ICMP header (type, code, cksum, id, seq) | 8 bytes (`PING_PKT_HDR_SZ`) |
| Payload (timestamp + fill pattern) | `data_length` bytes (default 56) |
| **Total** | **64 bytes** (default) |

On the receive side, the kernel verifies the checksum before delivering the packet to the raw socket. If verification fails, the packet never reaches `recv_ping()`.

---

## RFC references

- **[RFC 1071](../rfc/rfc1071.txt)** — Computing the Internet Checksum (the algorithm itself)
- **[RFC 792](../rfc/rfc792.txt)** — ICMP — defines the checksum field in ICMP messages
- **[RFC 791](../rfc/rfc791.txt)** — IPv4 — defines the header checksum (same algorithm, different scope)
- **[RFC 1624](https://www.rfc-editor.org/rfc/rfc1624)** — Incremental checksum update

---

## See also

- [ICMP.md](ICMP.md) — ICMP message layout, echo request/reply, where checksum fits in the header
- [IPv4.md](IPv4.md) — IPv4 header checksum (same algorithm, covers only the IP header)
- [ARCHITECTURE.md](../ARCHITECTURE.md) — module map (`checksum.c`, `send.c`, `recv.c`)
