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

### Step-by-step binary walkthrough

Example data — a minimal ICMP Echo Request header (8 bytes):

```
Offset:  00   01   02   03   04   05   06   07
Bytes:   08   00   00   00   AB   CD   00   01
         type code cksum=0  id        seq
```

#### Step 1 — Sum 16-bit words

The data pointer is cast to `uint16_t *`. On a little-endian machine (x86/ARM), byte pairs are read in reversed order:

```
Word 0: bytes [08, 00] → 0x0008
Word 1: bytes [00, 00] → 0x0000   (checksum field, zeroed before computation)
Word 2: bytes [AB, CD] → 0xCDAB
Word 3: bytes [00, 01] → 0x0100

sum = 0x0008 + 0x0000 + 0xCDAB + 0x0100 = 0x0000_CDB3
```

The accumulator is `uint32_t` so carries are preserved — the sum of four 16-bit values can be up to `4 × 0xFFFF = 0x3_FFFC`.

#### Step 2 — Handle odd byte

If `len` were odd (e.g., 9 bytes), the remaining single byte would be added as a `uint8_t`, effectively zero-padded to 16 bits:

```c
if (len == 1)
    sum += *(uint8_t *)ptr;   // e.g. 0xDE → added as 0x00DE
```

In our 8-byte example, `len` is even, so this branch is skipped.

#### Step 3 — Fold carries

One's complement addition wraps carries from bit 16 back into bit 0:

```
sum = 0x0000_CDB3

First fold:
  upper = sum >> 16       = 0x0000
  lower = sum & 0xFFFF    = 0xCDB3
  sum   = 0x0000 + 0xCDB3 = 0xCDB3

Second fold (handles carry from first fold):
  sum >> 16 = 0x0000
  sum       = 0xCDB3 + 0x0000 = 0xCDB3
```

A case where the second fold matters:

```
sum = 0x0001_FFFE

First fold:  0x0001 + 0xFFFE = 0xFFFF
Second fold: 0x0000 + 0xFFFF = 0xFFFF  (no new carry)

But:
sum = 0x0002_FFFF

First fold:  0x0002 + 0xFFFF = 0x1_0001
Second fold: 0x0001 + 0x0001 = 0x0002   ← carry folded again
```

#### Step 4 — Bitwise complement

```
sum  = 0xCDB3 = 1100 1101 1011 0011
~sum = 0x324C = 0011 0010 0100 1100
```

The returned value `0x324C` is written into the ICMP checksum field before sending.

---

## Verification on the receiver side

The receiver runs the **same algorithm** over the entire ICMP message **including** the checksum field. If no bits were corrupted:

```
sum of all words (with checksum included) = 0xFFFF   (one's complement)
~0xFFFF = 0x0000
```

A result of **0** means the message is intact. Any non-zero value → the packet is discarded.

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
