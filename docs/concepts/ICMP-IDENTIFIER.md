# ICMP identifier and PID

The ICMP **identifier** (`id`) is a 16-bit field in Echo Request / Echo Reply headers. It answers one question on the receiving host: *which local ping client sent this probe?*

`ft_ping` sets it once at startup:

```c
ping->ident = (uint16_t)(getpid() & 0xFFFF);
```

This page explains what that line means, why PID is used, and how the value flows through send and receive.

For the full ICMP header layout, see [ICMP.md](ICMP.md). For program structure, see [ARCHITECTURE.md](../ARCHITECTURE.md).

---

## Where the identifier lives

Every ICMP message has an 8-byte header. For Echo Request (type 8) and Echo Reply (type 0), bytes 4–7 are:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     type      |     code      |          checksum             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           identifier          |        sequence number        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         data (payload)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Size | Role |
|-------|------|------|
| **identifier** | 16 bits (2 bytes) | Tags the **process** (ping client) on the sender |
| **sequence** | 16 bits (2 bytes) | Tags the **probe number** within that process |

Both fields are stored in **network byte order** on the wire. Before send use `htons`; after receive use `ntohs` — see [Network byte order: `htons` and `ntohs`](#network-byte-order-htons-and-ntohs).

---

## What is PID?

**PID** (Process ID) is a number the operating system assigns to each running process.

```c
pid_t pid = getpid();   /* e.g. 42857 on this machine right now */
```

Properties that matter for ping:

| Property | Detail |
|----------|--------|
| Uniqueness | While a process is alive, its PID is unique on that machine |
| Stability | PID does **not** change during the lifetime of one `ft_ping` run |
| Size | On typical Unix systems PID fits in 32 bits; the field in ICMP fits in **16** |

The identifier must stay **constant** for the whole run. Sequence changes every packet; identifier does not.

---

## Why `getpid() & 0xFFFF` — lower 16 bits

### The field is only 16 bits wide

ICMP identifier is exactly 16 bits. Values range from `0` to `65535` (`0xFFFF`).

PID can be larger. Example on a 64-bit macOS/Linux host:

```
PID from getpid():  42857  =  0x0000A799   (32-bit view)
ICMP identifier:              0xA799       (16-bit field)
```

You cannot store the full PID in the packet. The usual compromise: take the **least significant 16 bits**.

### What `& 0xFFFF` does

`0xFFFF` is a bit mask — sixteen `1` bits:

```
0xFFFF  =  00000000 00000000 11111111 11111111
                              └─ keep ─┘
```

The expression `getpid() & 0xFFFF` **clears** all bits above bit 15:

```
PID (example):     00000000 00000000 10100111 10011001
Mask 0xFFFF:     & 00000000 00000000 11111111 11111111
                   ─────────────────────────────────
Result (ident):    00000000 00000000 10100111 10011001  →  0xA799
```

Cast to `(uint16_t)` makes the type match `ping->ident` in `t_ping`.

### Why not a random number or a counter?

| Approach | Problem |
|----------|---------|
| Fixed constant (e.g. always `1`) | Every ping on the machine would share the same `id` |
| Random each run | Works, but you must store it; PID is already unique and free |
| Global counter | Needs shared state across processes; PID is per-process by design |

**PID is the de facto standard** — system `ping` on Linux, BSD, and macOS does the same. It is simple, needs no extra configuration, and is usually unique enough.

---

## Identifier vs sequence

Easy to confuse; they solve different problems:

| | **identifier** | **sequence** |
|---|----------------|--------------|
| **Question** | Which **process** sent this? | Which **packet** from that process? |
| **Set when** | Once at startup (`init_ping`) | Before each `sendto` (`ping->seq++`) |
| **Changes during run** | No | Yes (0, 1, 2, …) |
| **In ft_ping** | `ping->ident` | `ping->seq` |

Analogy: **identifier** = apartment number, **sequence** = letter number sent from that apartment.

Example timeline for one `ft_ping` process with `ident = 42857`:

```
startup:  ident = 42857 (fixed)
send #1:  id=42857, seq=0
send #2:  id=42857, seq=1
send #3:  id=42857, seq=2
```

Every reply must echo back the same `id` and the matching `seq`.

---

## Network byte order: `htons` and `ntohs`

`htons` and `ntohs` are conversion helpers from `<arpa/inet.h>`. They translate **16-bit** integers between:

| Order | Meaning |
|-------|---------|
| **Host byte order** | How the CPU stores multi-byte values in RAM |
| **Network byte order** | How multi-byte fields are laid out **in packets** (always big-endian) |

| Function | Expands to | Direction | When |
|----------|------------|-----------|------|
| `htons(x)` | **h**ost **to** **n**etwork **s**hort | host → wire | writing `id` / `seq` into the ICMP header |
| `ntohs(x)` | **n**etwork **to** **h**ost **s**hort | wire → host | reading `id` / `seq` from a received packet |

**Short** = 16 bits (2 bytes). For 32-bit fields (e.g. IPv4 addresses) the same idea uses `htonl` / `ntohl` (**l**ong).

### Why the internet cares about byte order

CPUs disagree on which byte comes first for a multi-byte number:

| Endianness | Example: value `0x1234` stored as two bytes |
|------------|-----------------------------------------------|
| **Little-endian** (x86, Apple Silicon) | `[34] [12]` — least significant byte first |
| **Big-endian** (network / “wire”) | `[12] [34]` — most significant byte first |

IP, ICMP, TCP, and UDP specify: **multi-byte header fields on the wire are big-endian**. Every host must convert on send and receive so that `ident = 1` always appears as bytes `00 01` in the packet, regardless of local CPU.

Without conversion, a little-endian machine could put `ident = 1` into the packet as `01 00` — which other hosts would read as `256`.

### In memory vs on the wire in `ft_ping`

Inside the program, `ping->ident` and `ping->seq` are normal host-order `uint16_t` values. They are **never** written to the ICMP header directly.

**Send** (`srcs/send.c`) — convert before storing in the packet:

```c
ICMP_HDR_ID(icmp_hdr) = htons(ping->ident);
ICMP_HDR_SEQ(icmp_hdr) = htons(ping->seq);
```

**Receive** (`srcs/recv.c`) — convert before comparing or printing:

```c
ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident
```

**Print** (`srcs/print.c`) — sequence in the reply line also comes from the wire:

```c
ntohs(ICMP_HDR_SEQ(icmp_hdr))   /* → icmp_seq=0, icmp_seq=1, … */
```

Never compare `ping->ident` to `ICMP_HDR_ID(icmp_hdr)` without `ntohs` on a little-endian host: the raw bytes in the buffer are network order; `ping->ident` is host order.

### Worked example: `ident = 1`

```
Logical value:     1  =  0x0001

In RAM (little-endian host):     01 00
In ICMP packet (network):        00 01   ← htons() produces this layout

After recv:
  ICMP_HDR_ID in buffer  →  bytes 00 01
  ntohs(...)             →  1
  compare to ping->ident →  match
```

### On big-endian machines

If host order already matches network order, `htons` and `ntohs` are often no-ops (they return the value unchanged). You still call them so the same source compiles and behaves correctly everywhere.

### Rule of thumb for `ft_ping`

| Location | Byte order | Action |
|----------|------------|--------|
| `ping->ident`, `ping->seq` | host | use as-is in C logic |
| ICMP header in send buffer | network | `htons` on write |
| ICMP header in recv buffer | network | `ntohs` on read |
| `-v` banner `id 0x….` | host | `ping->ident` printed directly |

---

## Why the identifier exists — multiple ping clients

On Unix, a raw ICMP socket (`SOCK_RAW`, `IPPROTO_ICMP`) delivers **all** ICMP traffic the kernel considers relevant to that socket type — not only packets for one destination or one process.

Imagine two terminals:

```bash
# Terminal 1
sudo ./ft_ping 8.8.8.8

# Terminal 2
sudo ./ft_ping 1.1.1.1
```

Both processes listen for ICMP Echo Replies. Replies from 8.8.8.8 and 1.1.1.1 can arrive at **both** sockets. Without `id`, each process would mis-attribute foreign replies as its own.

With distinct identifiers:

```
Process A (PID 1000)  →  ident = 1000  →  accepts replies where id == 1000
Process B (PID 1001)  →  ident = 1001  →  accepts replies where id == 1001
```

Reply with `id = 1001` arriving at process A is **ignored** — not an error, just not ours.

---

## End-to-end flow in ft_ping

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. init_ping() — srcs/main.c                                    │
│    ping->ident = (uint16_t)(getpid() & 0xFFFF);                 │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. send_ping() — srcs/send.c                                    │
│    ICMP_HDR_ID(icmp_hdr) = htons(ping->ident);                  │
│    ICMP_HDR_SEQ(icmp_hdr) = htons(ping->seq);                   │
│    sendto(...) → kernel adds IP header → wire                   │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
                    Remote host (kernel)
                    Echo Reply: same id, same seq, same payload
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. recv_ping() — srcs/recv.c                                    │
│    if (type == ECHOREPLY && ntohs(id) == ping->ident)           │
│        print_echo_reply(...)    ← ours                          │
│    else if (type != ECHO)                                       │
│        print_icmp_error(...)    ← TTL exceeded, unreachable, …  │
│    (foreign ECHOREPLY with wrong id → silently dropped)         │
└─────────────────────────────────────────────────────────────────┘
```

### Source references

**Initialization** (`srcs/main.c`):

```c
ping->ident = (uint16_t)(getpid() & 0xFFFF);
```

**Send** (`srcs/send.c`):

```c
ICMP_HDR_ID(icmp_hdr) = htons(ping->ident);
ICMP_HDR_SEQ(icmp_hdr) = htons(ping->seq);
```

**Receive filter** (`srcs/recv.c`):

```c
if (ICMP_HDR_TYPE(icmp_hdr) == ICMP_ECHOREPLY
    && ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident)
{
    print_echo_reply(ping, &msg, buf, bytes);
    return (0);
}
```

**Storage** (`includes/ft_ping.h`):

```c
uint16_t    ident;
uint16_t    seq;
```

### Verbose header line

With `-v`, the startup banner shows the identifier (`srcs/stats.c`):

```
PING 8.8.8.8 (8.8.8.8): 56 data bytes, id 0xa799 = 42857
```

Hex `0xa799` and decimal `42857` are the same 16-bit value.

---

## What the remote host does with `id`

The replying host (almost always its **kernel**, not a user program) does **not** interpret `id` as a PID. It simply **copies** identifier and sequence from the Echo Request into the Echo Reply unchanged, along with the payload.

| Field | In Echo Request | In Echo Reply |
|-------|-----------------|---------------|
| type | 8 | 0 |
| identifier | set by sender | **copied unchanged** |
| sequence | set by sender | **copied unchanged** |
| data | sender payload | **copied unchanged** |

So `id` is a **local convention** on the sending machine: “this value tags my process.” The internet treats it as opaque bits that must round-trip.

---

## PID collisions and edge cases

Taking only 16 bits is a trade-off. It works in practice but is not mathematically perfect.

### Two PIDs with the same lower 16 bits

```
PID  1000  =  0x000003E8  →  ident = 0x03E8 = 1000
PID 75036  =  0x0001253C  →  ident = 0x253C = 9532   (different — OK)

PID  1000  =  0x000003E8  →  ident = 0x03E8
PID 65536 + 1000 = 66536  →  ident = 0x03E8  (collision!)
```

If two live `ft_ping` processes collide on `ident`, they may each accept the other’s Echo Replies. Rare on a typical laptop; more plausible on busy servers with many short-lived processes.

### PID reuse

When a process exits, the OS may later assign the same PID to a new process. A very delayed reply to an old probe could theoretically match a new process with the same PID and `ident`. Again uncommon for ping.

### Why it is still good enough

- Same approach as **inetutils ping** / **iputils ping**
- Collisions require PIDs differing by a multiple of 65536 **at the same time**
- `seq` and destination still narrow matching; `id` is the main guard against **cross-process** mix-ups on one host

---

## Common misconceptions

### “The remote host knows my PID”

No. It only echoes the 16-bit `id` field. It has no access to your process table.

### “Identifier and sequence are the same thing”

No. `id` = which client; `seq` = which packet from that client. See [Identifier vs sequence](#identifier-vs-sequence).

### “We need `id` because packets can arrive out of order”

Partially related. `seq` handles ordering and duplicates within one client. `id` handles **multiple clients** on one machine. Both are needed.

### “`& 0xFFFF` means the handler cooperates with a partial write”

Unrelated topic (that is about signals and `sig_atomic_t`). Here `& 0xFFFF` is simple **bit masking** to fit a 32-bit PID into a 16-bit protocol field.

### “I can skip `htons` / `ntohs` on my Mac”

On some builds they appear to do nothing, but the ICMP header in the buffer is still defined as network byte order. Skipping conversion breaks on little-endian hosts when values are not symmetric (e.g. `ident = 0x0100`). Always convert at the host/wire boundary.

---

## Quick reference

| Item | Value in ft_ping |
|------|------------------|
| Default `ident` | `getpid() & 0xFFFF` |
| Set in | `init_ping()` — `srcs/main.c` |
| Written to packet | `send_ping()` — `srcs/send.c` |
| Checked on receive | `recv_ping()` — `srcs/recv.c` |
| Shown with `-v` | `print_header()` — `srcs/stats.c` |
| Wire format | Big-endian — see [`htons` / `ntohs`](#network-byte-order-htons-and-ntohs) |
| Field width | 16 bits (`uint16_t`) |
