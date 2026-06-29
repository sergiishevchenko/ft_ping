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

#### Why invert (`~`)?

The `~` is not byte swapping — it flips **every bit** (`0→1`, `1→0`). It is the final step of the **one's complement** algorithm (RFC 1071), not an optional trick.

After summing all words (with the checksum field zeroed) and folding carries, you get a 16-bit value `sum`. The checksum stored in the packet is `~sum`. The receiver then runs the **same function** over the entire message **including** that checksum field. If the data is intact, the folded sum is always `0xFFFF`, and `~0xFFFF = 0x0000` — a clean pass/fail test.

See [Why inversion (~)?](#why-inversion-) for a concrete numeric walkthrough using `sum = 0x9E3F` → checksum `0x61C0`.

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

## Line-by-line walkthrough with a second example

A different packet that covers the **odd-byte branch** (`if (len == 1)`). ICMP Echo Request: type = 8, code = 0, id = `0xC0DE`, seq = `0x0003`, payload = `AA BB CC` (3 bytes). Total **11 bytes** (odd length).

```
Addr:   0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A
Hex:      08    00    00    00    C0    DE    00    03    AA    BB    CC
          type  code  cksum(=0)  id          seq         payload (3 bytes)
```

### Hex notation primer

Every hex digit maps to exactly 4 bits. For example, `0x0001_9E3E`:

```
Hex digit:  0    0    0    1  _  9    E    3    E
            │    │    │    │     │    │    │    │
Binary:   0000 0000 0000 0001  1001 1110 0011 1110
          ──── upper 16 bits── ──── lower 16 bits──
                 = 0x0001            = 0x9E3E
                 (carry)             (sum)
```

The `_` is a visual separator — `0x0001_9E3E` = `0x00019E3E`. It makes the carry/sum split obvious at a glance.

### Line 5: `uint16_t *ptr;`

Declares a pointer to a 16-bit word. The **type** of the pointer controls two things:
- `*ptr` (dereference) reads `sizeof(uint16_t)` = **2 bytes** from memory.
- `ptr++` (increment) advances the address by `sizeof(uint16_t)` = **2 bytes**.

```
ptr = ???  (uninitialized)
```

### Line 6: `uint32_t sum;`

A 32-bit accumulator. Uninitialized.

### Line 8: `ptr = (uint16_t *)data;`

The `void *data` pointer (pointing at the start of our 11-byte packet) is cast to `uint16_t *`. From now on, each `*ptr` reads 2 bytes at a time.

```
ptr → address 0x00 (first byte of packet: 0x08)
len = 11  (function argument, unchanged)
```

### Line 9: `sum = 0;`

```
sum = 0x0000_0000
      binary:  0000 0000 0000 0000 0000 0000 0000 0000
      decimal: 0
```

### Lines 10–14: `while (len > 1)` loop

The loop runs while at least 2 bytes remain.

#### Iteration 1: `len = 11 > 1` → true

**Line 12: `sum += *ptr++;`**

`*ptr` reads 2 bytes at address 0x00: bytes `[08, 00]`. On little-endian (x86/ARM), the lower address is the **least-significant** byte:

```
Memory:     08        00
            LSB       MSB        (little-endian: LSB at lower address)
uint16_t:   0x0008
binary:     0000 0000 0000 1000
decimal:    8
```

After add, `ptr` advances by 2 bytes:

```
sum = 0x0000_0000 + 0x0000_0008 = 0x0000_0008
      binary:  0000 0000 0000 0000 0000 0000 0000 1000
      decimal: 8
ptr → address 0x02
```

**Line 13: `len -= 2;`** → `len = 11 − 2 = 9`

#### Iteration 2: `len = 9 > 1` → true

**Line 12: `sum += *ptr++;`**

Bytes `[00, 00]` (checksum field, zeroed):

```
uint16_t: 0x0000    binary: 0000 0000 0000 0000    decimal: 0
```

```
sum = 0x0000_0008 + 0x0000_0000 = 0x0000_0008
      binary:  0000 0000 0000 0000 0000 0000 0000 1000
      decimal: 8
ptr → address 0x04
len = 9 − 2 = 7
```

#### Iteration 3: `len = 7 > 1` → true

**Line 12: `sum += *ptr++;`**

Bytes `[C0, DE]` (id field, `0xC0DE` on the wire):

```
Memory:     C0        DE
            LSB       MSB        (little-endian)
uint16_t:   0xDEC0
binary:     1101 1110 1100 0000
decimal:    57024
```

```
sum = 0x0000_0008 + 0x0000_DEC0 = 0x0000_DEC8
      binary:  0000 0000 0000 0000 1101 1110 1100 1000
      decimal: 57032
ptr → address 0x06
len = 7 − 2 = 5
```

#### Iteration 4: `len = 5 > 1` → true

**Line 12: `sum += *ptr++;`**

Bytes `[00, 03]` (seq field, `0x0003` on the wire):

```
Memory:     00        03
uint16_t:   0x0300
binary:     0000 0011 0000 0000
decimal:    768
```

```
sum = 0x0000_DEC8 + 0x0000_0300 = 0x0000_E1C8
      binary:  0000 0000 0000 0000 1110 0001 1100 1000
      decimal: 57800
ptr → address 0x08
len = 5 − 2 = 3
```

#### Iteration 5: `len = 3 > 1` → true

**Line 12: `sum += *ptr++;`**

Bytes `[AA, BB]` (first two payload bytes):

```
Memory:     AA        BB
uint16_t:   0xBBAA
binary:     1011 1011 1010 1010
decimal:    48042
```

```
sum = 0x0000_E1C8 + 0x0000_BBAA = 0x0001_9D72
      binary:  0000 0000 0000 0001 1001 1101 0111 0010
      decimal: 105842               ↑
                               bit 16 is set — CARRY!
ptr → address 0x0A
len = 3 − 2 = 1
```

#### Loop check: `len = 1 > 1` → false — exit loop

### Line 15: `if (len == 1)` → true

One byte remains (packet length was odd: 11 bytes). Enter the branch.

### Line 16: `sum += *(uint8_t *)ptr;`

`ptr` now points at address 0x0A — the last byte `0xCC`. Cast to `uint8_t *`, read 1 byte:

```
Memory:     CC
uint8_t:    0xCC
binary:     1100 1100
decimal:    204
```

The 8-bit value is promoted to 32 bits when added to `sum`:

```
0xCC → 0x0000_00CC
       binary:  0000 0000 0000 0000 0000 0000 1100 1100
       decimal: 204

sum = 0x0001_9D72 + 0x0000_00CC = 0x0001_9E3E
      binary:  0000 0000 0000 0001 1001 1110 0011 1110
      decimal: 106046
```

### Line 17: `sum = (sum >> 16) + (sum & 0xFFFF);` — first fold

Split the 32-bit `sum` into upper and lower halves.

**`sum >> 16`** — shift right by 16 bits (extract carry):

```
sum      = 0000 0000 0000 0001 | 1001 1110 0011 1110
                                 ↓ shift everything right by 16
sum >> 16= 0000 0000 0000 0000 | 0000 0000 0000 0001 = 0x0001
           decimal: 1
```

**`sum & 0xFFFF`** — mask, keep only lower 16 bits:

```
sum      = 0000 0000 0000 0001 1001 1110 0011 1110
0xFFFF   = 0000 0000 0000 0000 1111 1111 1111 1111
           ──────────────────────────────────────────  AND
result   = 0000 0000 0000 0000 1001 1110 0011 1110 = 0x9E3E
           decimal: 40510
```

Add them:

```
    0000 0000 0000 0001    0x0001   (carry = 1)     decimal: 1
  + 1001 1110 0011 1110    0x9E3E   (lower 16 bits) decimal: 40510
  ─────────────────────
    1001 1110 0011 1111    0x9E3F                    decimal: 40511

bit 16 = 0 → no new carry
```

```
sum = 0x0000_9E3F
      binary:  0000 0000 0000 0000 1001 1110 0011 1111
      decimal: 40511
```

### Line 18: `sum += (sum >> 16);` — second fold

```
sum >> 16 = 0x0000_9E3F >> 16 = 0x0000_0000
            binary:  0000 0000 0000 0000 0000 0000 0000 0000
            decimal: 0

sum = 0x0000_9E3F + 0x0000_0000 = 0x0000_9E3F   (unchanged — no carry existed)
      decimal: 40511
```

### Line 19: `return ((uint16_t)~sum);` — bitwise complement

`~sum` inverts all 32 bits, then `(uint16_t)` truncates to the lower 16:

**`~sum` (32-bit inversion):**

```
sum  = 0000 0000 0000 0000 1001 1110 0011 1111
~sum = 1111 1111 1111 1111 0110 0001 1100 0000
```

**`(uint16_t)~sum` — keep only lower 16 bits:**

```
Position:  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
           ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ───
sum:        1   0   0   1   1   1   1   0   0   0   1   1   1   1   1   1
~sum:       0   1   1   0   0   0   0   1   1   1   0   0   0   0   0   0

0110 0001 1100 0000 = 0x61C0
decimal: 25024
```

**Return value: `0x61C0`** (decimal 25024). This checksum is written into the ICMP header.

When the receiver sums all words **including** this checksum, the folded result is `0xFFFF`, and `~0xFFFF = 0x0000` — proof the packet is intact. See [Why inversion (~)?](#why-inversion-) for the full `0x9E3F + 0x61C0 = 0xFFFF` walkthrough.

### Variable state summary

```
Line   │ Action                           │ ptr    │ len │ sum (hex)    │ sum (dec) │ sum (binary, 32 bits)
───────┼──────────────────────────────────-┼────────┼─────┼──────────────┼───────────┼──────────────────────────────────
  8    │ ptr = (uint16_t *)data            │ →0x00  │  11 │ ???          │ ???       │ ???
  9    │ sum = 0                           │ →0x00  │  11 │ 0x0000_0000  │ 0         │ 00000000 00000000 00000000 00000000
 12(1) │ sum += [08,00] = 0x0008; ptr++    │ →0x02  │  11 │ 0x0000_0008  │ 8         │ 00000000 00000000 00000000 00001000
 13(1) │ len -= 2                          │ →0x02  │   9 │     —        │     —     │ (unchanged)
 12(2) │ sum += [00,00] = 0x0000; ptr++    │ →0x04  │   9 │ 0x0000_0008  │ 8         │ 00000000 00000000 00000000 00001000
 13(2) │ len -= 2                          │ →0x04  │   7 │     —        │     —     │ (unchanged)
 12(3) │ sum += [C0,DE] = 0xDEC0; ptr++    │ →0x06  │   7 │ 0x0000_DEC8  │ 57032     │ 00000000 00000000 11011110 11001000
 13(3) │ len -= 2                          │ →0x06  │   5 │     —        │     —     │ (unchanged)
 12(4) │ sum += [00,03] = 0x0300; ptr++    │ →0x08  │   5 │ 0x0000_E1C8  │ 57800     │ 00000000 00000000 11100001 11001000
 13(4) │ len -= 2                          │ →0x08  │   3 │     —        │     —     │ (unchanged)
 12(5) │ sum += [AA,BB] = 0xBBAA; ptr++    │ →0x0A  │   3 │ 0x0001_9D72  │ 105842    │ 00000000 00000001 10011101 01110010
 13(5) │ len -= 2                          │ →0x0A  │   1 │     —        │     —     │ (unchanged)  ↑ bit 16 = carry
  16   │ sum += [CC] = 0xCC (odd byte)     │ →0x0A  │   1 │ 0x0001_9E3E  │ 106046    │ 00000000 00000001 10011110 00111110
  17   │ fold 1: (>>16) + (&0xFFFF)        │   —    │   — │ 0x0000_9E3F  │ 40511     │ 00000000 00000000 10011110 00111111
  18   │ fold 2: sum += (sum >> 16)        │   —    │   — │ 0x0000_9E3F  │ 40511     │ 00000000 00000000 10011110 00111111
  19   │ return (uint16_t)~sum             │   —    │   — │ 0x61C0       │ 25024     │                   01100001 11000000
```

### Verification (receiver side)

The receiver gets the packet with checksum `0x61C0` stored as `uint16_t` in bytes 02–03. On a little-endian machine, `uint16_t` value `0x61C0` is stored as byte `0xC0` at address 02 (LSB) and byte `0x61` at address 03 (MSB):

```
Memory: 08 00 C0 61 C0 DE 00 03 AA BB CC

Words (little-endian uint16_t reads):
  W0: [08, 00] = 0x0008
  W1: [C0, 61] = 0x61C0   ← checksum
  W2: [C0, DE] = 0xDEC0
  W3: [00, 03] = 0x0300
  W4: [AA, BB] = 0xBBAA
  odd byte: CC = 0x00CC
```

```
  0x0008
+ 0x61C0 = 0x0000_61C8
+ 0xDEC0 = 0x0001_4088   ← carry
+ 0x0300 = 0x0001_4388
+ 0xBBAA = 0x0001_FF32   ← carry
+ 0x00CC = 0x0001_FFFE

Fold 1:  0x0001 + 0xFFFE = 0xFFFF
Fold 2:  0x0000 + 0xFFFF = 0xFFFF

~0xFFFF = 0x0000  ✓   Packet is intact!
```

---

## Why inversion (~)?

Inversion is the `~` operator on the last line of `checksum()`:

```c
return ((uint16_t)~sum);
```

It flips every bit of the folded 16-bit sum — **not** the byte order. This is what makes the algorithm a **one's complement checksum**: the value written into the packet is chosen so that, together with the data, the receiver's sum collapses to a known constant.

### Sender side

Suppose that after summing all 16-bit words (with the checksum field set to zero) and folding carries, you get:

```
sum = 0x9E3F
      binary:  1001 1110 0011 1111
      decimal: 40511
```

Invert every bit:

```
~sum = 0x61C0   ← written into the ICMP checksum field
       binary:  0110 0001 1100 0000
       decimal: 25024
```

Bit-by-bit:

```
Position:  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
           ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── ───
sum:        1   0   0   1   1   1   1   0   0   0   1   1   1   1   1   1
~sum:       0   1   1   0   0   0   0   1   1   1   0   0   0   0   0   0
```

This value (`0x61C0`) is exactly what the [line-by-line walkthrough](#line-19-return-uint16_tsum--bitwise-complement) produces for the 11-byte odd-length example.

### Receiver side

The receiver does **not** zero the checksum field. It sums **all** words, including the checksum that was just written in:

```
sum_of_all_words + checksum
= 0x9E3F + 0x61C0
= 0xFFFF
```

In binary:

```
  1001 1110 0011 1111    0x9E3F
+ 0110 0001 1100 0000    0x61C0
─────────────────────
 1111 1111 1111 1111    0xFFFF
```

Then the same function inverts the result:

```
~0xFFFF = 0x0000   ✓   packet is intact
```

So the checksum is **chosen** so that data + checksum always produce the "perfect" one's complement sum `0xFFFF`. Any bit corruption during transit breaks this balance → the folded sum is no longer `0xFFFF` → `~sum ≠ 0x0000` → packet discarded.

### What the receiver actually checks

The receiver does not compare "my sum == your checksum". It runs one test:

```
checksum(entire_packet_including_checksum_field) == 0x0000 ?
```

| Folded sum before `~` | After `~` | Meaning |
|-----------------------|-----------|---------|
| `0xFFFF` | `0x0000` | Packet intact |
| anything else | non-zero | Packet corrupted → discard |

### Why not store `sum` directly?

| Approach | Problem |
|----------|---------|
| Store raw `sum` | Receiver must know to subtract or compare separately — two different code paths |
| Store `~sum` (RFC 1071) | One function, one test: result == 0 means OK |
| Skip `~` | Not an Internet Checksum — verification math breaks |

In one's complement arithmetic, `0x0000` and `0xFFFF` both represent zero. The `~` step also ensures the transmitted checksum is rarely all-zeros, which would be ambiguous with "checksum not computed".

### Not byte swapping

Inversion (`~`) and endianness are unrelated:

| Operation | What it does |
|-----------|--------------|
| `~sum` | Flips bits: `0x9E3F` → `0x61C0` |
| Little-endian storage | Stores `0x61C0` as bytes `[C0, 61]` in memory |
| `htons` / `ntohs` | Swaps byte order for network wire format |

The checksum algorithm reads raw memory; endianness affects which numeric value each `uint16_t` word contributes, but the final complemented checksum still verifies to 0 on any architecture (RFC 1071 Appendix B).

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
- **[RFC 1624](../rfc/rfc1624.txt)** — Incremental checksum update

---

## See also

- [ICMP.md](ICMP.md) — ICMP message layout, echo request/reply, where checksum fits in the header
- [IPv4.md](IPv4.md) — IPv4 header checksum (same algorithm, covers only the IP header)
- [ARCHITECTURE.md](../ARCHITECTURE.md) — module map (`checksum.c`, `send.c`, `recv.c`)
