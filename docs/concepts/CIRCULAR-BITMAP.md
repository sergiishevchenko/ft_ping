# Circular Bitmap for Duplicate Detection

When **ping** receives ICMP Echo Replies, the same probe can arrive more than once (retransmissions, multipath routing, duplicated packets in the network). The program must detect **duplicate replies** — two responses with the same **sequence number** (`seq`) — and print `(DUP!)` instead of counting them as new successful receives.

**ft_ping** solves this with a fixed-size **circular bitmap** stored in `recv_table`. The logic lives in `check_duplicate()` in `srcs/print.c`.

---

## What is a bitmap?

A **bitmap** (bit table) stores one boolean flag per item using a single bit instead of a full `int` or `bool`.

| Approach | Memory for 1024 flags |
|----------|----------------------|
| `bool seen[1024]` | ~1024 bytes (often 1 byte per flag) |
| `uint8_t table[128]` bitmap | **128 bytes** (8 flags per byte) |

Each sequence number maps to one bit:

- bit `0` → not seen yet
- bit `1` → already received

Setting and testing a bit uses fast bitwise operations (`|`, `&`).

---

## What does “circular” mean?

ICMP sequence numbers are `uint16_t`: they run `0, 1, 2, …` and eventually wrap at **65536**. A ping session can also send thousands of probes. Storing “seen or not” for every possible `seq` would need a huge table.

A **circular** (ring) bitmap uses only **N** slots and maps any `seq` into that range with modulo:

```
slot = seq % N
```

When `seq` grows past `N`, it **wraps around** and reuses the same slots:

```
seq:   0    1    2  …  1023   1024   1025  …
slot:  0    1    2  …  1023     0      1   …
       └──────── one lap ────────┘└─ same slots as 0, 1, …
```

So the table is a **ring**: after slot 1023 comes slot 0 again.

This is “circular” because there is no end — indices repeat forever, like numbers on a clock face.

---

## Layout in ft_ping

| Constant / field | Value | Meaning |
|------------------|-------|---------|
| `PING_CKTAB_SZ` | 128 | Size of `recv_table` in **bytes** |
| `PING_CKTAB_SZ * 8` | 1024 | Number of **bits** (distinct slots) |
| `t_ping.recv_table` | `unsigned char[128]` | Bitmap, zeroed at startup |

Conceptually:

```
recv_table[0]   → bits for seq % 1024 ∈ {0, 8, 16, …}
recv_table[1]   → bits for seq % 1024 ∈ {1, 9, 17, …}
…
recv_table[127] → bits for seq % 1024 ∈ {127, 255, …}
```

Within each byte, bit positions 0–7 map to consecutive sequence residues.

---

## Mapping `seq` to a byte and a bit

Given `seq` (host byte order, after `ntohs`):

```
bit_index = seq % 1024          // slot on the ring (0 … 1023)
idx       = bit_index >> 3      // byte index: bit_index / 8  (0 … 127)
bit_pos   = bit_index & 0x07    // position inside byte: bit_index % 8  (0 … 7)
mask      = 1 << bit_pos        // single-bit mask
```

Why `>> 3` and `& 0x07` instead of `/ 8` and `% 8`?

- `>> 3` divides by 8 (shift right by 3 bits).
- `& 0x07` keeps the lowest 3 bits, equivalent to `% 8`.

These are the standard way to index a packed bitmap.

---

## Algorithm (`check_duplicate`)

```c
static int check_duplicate(t_ping *ping, uint16_t seq)
{
    int idx;
    int bit;

    idx = (seq % (PING_CKTAB_SZ * 8)) >> 3;
    bit = 1 << ((seq % (PING_CKTAB_SZ * 8)) & 0x07);
    if (ping->recv_table[idx] & bit)
        return (1);
    ping->recv_table[idx] |= bit;
    return (0);
}
```

| Return value | Meaning | Effect in `print_echo_reply` |
|--------------|---------|------------------------------|
| `0` | First time this slot was marked for this lap | Normal reply line |
| `1` | Bit already set — duplicate | Append `(DUP!)`, increment `num_rept` |

`num_recv` is still incremented for every echo reply; only `num_rept` tracks duplicates.

---

## Worked examples

Assume `recv_table` is all zeros.

### Example 1 — first reply, `seq = 5`

| Step | Expression | Result |
|------|------------|--------|
| `bit_index` | `5 % 1024` | `5` |
| `idx` | `5 >> 3` | `0` |
| `bit_pos` | `5 & 7` | `5` |
| `bit` | `1 << 5` | `32` (`0b00100000`) |
| Test | `recv_table[0] & 32` | `0` → not duplicate |
| Set | `recv_table[0] \|= 32` | bit 5 of byte 0 is now `1` |

Returns `0`.

### Example 2 — same `seq = 5` again

Same `idx = 0`, `bit = 32`. Now `recv_table[0] & 32` is non-zero → returns `1` (duplicate). The bit is not changed.

### Example 3 — `seq = 10` (different byte)

| Step | Result |
|------|--------|
| `bit_index` | `10` |
| `idx` | `10 >> 3` = `1` |
| `bit_pos` | `10 & 7` = `2` |
| `bit` | `1 << 2` = `4` |

Uses `recv_table[1]`, bit 2 — independent from `seq = 5` in `recv_table[0]`.

### Example 4 — wrap-around, `seq = 1025`

| Step | Result |
|------|--------|
| `bit_index` | `1025 % 1024` = `1` |
| `idx` | `0` |
| `bit` | `1 << 1` = `2` |

`seq = 1025` occupies the **same slot** as `seq = 1`. If `seq = 1` was received earlier in the session and its bit is still set, a reply with `seq = 1025` would be reported as a duplicate even though it is a different probe. That is the deliberate trade-off of a small fixed table.

---

## Visual: one byte of the ring

After receiving `seq = 0`, `5`, and `13`:

```
seq = 0  → bit_index 0  → byte 0, bit 0
seq = 5  → bit_index 5  → byte 0, bit 5
seq = 13 → bit_index 13 → byte 1, bit 5

recv_table[0]:  bit 7 … 5 4 3 2 1 0
                      … 1 0 0 0 0 1
                            ↑       ↑
                          seq=5   seq=0

recv_table[1]:  … 1 …  (seq=13, bit 5)
```

---

## Why 1024 slots is enough in practice

- Typical ping runs are short (`-c 4`, a few hundred packets). Duplicates matter for the **same** `seq` arriving twice close together in time.
- Sequence numbers increment once per sent probe; a duplicate reply usually arrives seconds later, with the **same** `seq`, not 1024 probes later.
- GNU **ping** uses the same idea (bitmap sized for a limited window of sequence numbers).

The ring is not meant to remember every `seq` forever — only to catch **immediate** duplicates within a sliding window of the last 1024 residue classes.

---

## Limitations

### 1. Slot collision after 1024 probes

`seq` and `seq + 1024` map to the same bit. If the table still has that bit set from an earlier lap, a **new** packet can be misclassified as a duplicate.

In practice this is rare for normal ping usage because:

- the table is not cleared between laps;
- very long runs sending more than 1024 packets without restarting can make this more likely.

### 2. No timestamp per entry

The bitmap only stores “seen / not seen”, not **when** it was seen. It cannot distinguish “duplicate 5 ms later” from “new probe 10 minutes later” if they share the same slot.

### 3. Memory is fixed at 128 bytes

No heap allocation, no growth — predictable and fine for a CLI tool.

---

## Where it fits in the receive path

```
recv_ping()
    → print_echo_reply()
        → dupflag = check_duplicate(ping, ntohs(ICMP_HDR_SEQ(icmp_hdr)))
        → num_recv++
        → if (dupflag) num_rept++
        → print line with optional (DUP!)
```

See also [ICMP.md](ICMP.md) (sequence numbers and duplicate example timeline) and [ARCHITECTURE.md](../ARCHITECTURE.md) (`print.c` / stats).

---

## Summary

| Term | Meaning |
|------|---------|
| **Bitmap** | One bit per “have we seen this?” flag; 8 flags per byte |
| **Circular** | `seq % 1024` wraps high sequence numbers onto a fixed ring of slots |
| **Duplicate** | Same slot visited twice while its bit is still `1` |
| **Cost** | 128 bytes RAM, O(1) time per reply |

The circular bitmap is a compact, fast structure for duplicate ICMP echo detection without storing every sequence number explicitly.
