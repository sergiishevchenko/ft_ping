# ICMP (Internet Control Message Protocol)

ICMP is a layer-3 companion to IP. It does not carry application data; it carries **control and diagnostic messages** — reachability tests, error reports, and redirects. **ft_ping** is built entirely around one ICMP message pair: **Echo Request** and **Echo Reply**.

---

## Role in the network stack

For the full OSI vs TCP/IP picture, see [OSI-TCP-IP.md](OSI-TCP-IP.md). Short version:

```
Application          ft_ping (user process)
       |
Transport            (none — ICMP is not TCP/UDP)
       |
Network              IP header  +  ICMP message
       |
Link                 Ethernet, Wi‑Fi, …
```

When you run `ft_ping 8.8.8.8`, the program:

1. Opens a **raw socket** (`SOCK_RAW`, `IPPROTO_ICMP`).
2. Builds an **ICMP Echo Request** (type 8) in user space.
3. Passes only the ICMP bytes to `sendto()`; the **kernel adds the IPv4 header**.
4. On receive, the raw socket delivers the **full IP packet** (IP + ICMP); the program parses both layers.

---

## ICMP message layout

Every ICMP message starts with a common 8-byte header:

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | **type** | 1 byte | Message kind (8 = Echo Request, 0 = Echo Reply, 3 = Dest Unreachable, …) |
| 1 | **code** | 1 byte | Sub-type (meaning depends on `type`) |
| 2–3 | **checksum** | 2 bytes | Integrity check over the whole ICMP message |
| 4–7 | **rest of header** | 4 bytes | Payload depends on `type` |

For **Echo Request / Echo Reply**, bytes 4–7 are:

| Field | Size | Description |
|-------|------|-------------|
| **identifier** | 2 bytes | Distinguishes concurrent ping processes |
| **sequence** | 2 bytes | Increments per probe; detects loss and duplicates |

After the 8-byte header comes **optional data** (payload). In `ft_ping` the default payload is **56 bytes** (plus 8-byte ICMP header → **64 bytes** shown in reply lines).

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

### ICMP header — byte map (Echo)

All echo messages share the same **8-byte** header (`PING_PKT_HDR_SZ` in `ft_ping.h`):

| Offset | Field | Size | Echo Request | Echo Reply |
|--------|-------|------|--------------|------------|
| 0 | **type** | 1 byte | `8` (`ICMP_ECHO`) | `0` (`ICMP_ECHOREPLY`) |
| 1 | **code** | 1 byte | `0` | `0` |
| 2–3 | **checksum** | 2 bytes | [RFC 1071](../rfc/rfc1071.txt) over header + data | recomputed on reply |
| 4–5 | **identifier** | 2 bytes | `getpid() & 0xFFFF` | copied from request |
| 6–7 | **sequence** | 2 bytes | 0, 1, 2, … | copied from request |

`id` and `seq` are stored in **network byte order** on the wire (`htons` / `ntohs`). See [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md).

### Complete ICMP message — header + payload

An **ICMP message** = fixed header + variable **data**:

```
┌─────────────────────────────────────────────────────────────┐
│              ICMP HEADER (8 bytes, fixed)                   │
│  ┌──────┬──────┬──────────┬────────────┬──────────────┐     │
│  │ type │ code │ checksum │ identifier │ sequence     │     │
│  │ 1 B  │ 1 B  │   2 B    │    2 B     │    2 B       │     │
│  └──────┴──────┴──────────┴────────────┴──────────────┘     │
├─────────────────────────────────────────────────────────────┤
│              DATA / PAYLOAD (-s bytes, variable)            │
│  ┌─────────────────────┬─────────────────────────────────┐  │
│  │ struct timeval      │ pattern or default fill         │  │
│  │ (send timestamp)    │ 00 01 02 … or -p hex repeat     │  │
│  └─────────────────────┴─────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

Default `-s 56` (56 data bytes):

```
ICMP message (64 bytes total = 8 header + 56 data)

┌──────── 8 B header ────────┬──────────── 56 B data ────────────┐
│ type code cksum id seq     │ timeval │ 00 01 02 03 … (rest)    │
└────────────────────────────┴───────────────────────────────────┘
                              ↑         ↑
                         sizeof(      from init_data_buffer()
                         timeval)     see FLAGS.md (-p)
```

The reply line `64 bytes from …` counts **ICMP header + data**, not the IP header.

### On the wire: IPv4 packet wrapping ICMP

`ft_ping` builds only the ICMP portion. The **kernel** prepends the IPv4 header on send; on receive, `recvmsg()` returns **IP + ICMP**:

```
┌─────────────────────────────────────────────────────────────┐
│           IPv4 HEADER (~20 bytes, added by kernel)          │
│  version, IHL, TTL, protocol=ICMP(1), src IP, dst IP, …     │
├─────────────────────────────────────────────────────────────┤
│                    ICMP MESSAGE (user/build)                │
│  ┌──────────────── 8 B ICMP header ────────────────┐        │
│  │ type | code | checksum | id | seq               │        │
│  ├─────────────────────────────────────────────────┤        │
│  │ data: [timeval][pattern or 00 01 02 …]  (-s)    │        │
│  └─────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────┘
         ↑ recv_ping skips ip_hl×4 bytes to reach ICMP
```

See [IPv4.md](IPv4.md) for IP header fields; [TTL.md](TTL.md) for `--ttl`.

### Echo Request vs Echo Reply

```
  YOU (ft_ping)                              TARGET (kernel)
       |                                           |
       |  Echo Request                             |
       |  type=8  id=X  seq=N                      |
       |  data=[timeval|padding...]                |
       |------------------------------------------>|
       |                                           |
       |  Echo Reply                               |
       |  type=0  id=X  seq=N   ← same id, seq     |
       |  data=[same bytes back]                   |
       |<------------------------------------------|
```

| Field | Request (type 8) | Reply (type 0) |
|-------|------------------|----------------|
| type | `8` | `0` |
| code | `0` | `0` |
| identifier | set by sender | **unchanged** |
| sequence | per probe | **unchanged** |
| data | timeval + fill | **byte-for-byte copy** |
| checksum | over full message | recomputed |

### Size reference (`ft_ping`)

| Piece | Default | Controlled by |
|-------|---------|---------------|
| ICMP header | 8 bytes | fixed (`PING_PKT_HDR_SZ`) |
| ICMP data (payload) | 56 bytes | `-s` (`data_length`) |
| **ICMP message** | **64 bytes** | 8 + 56 → shown in reply line |
| IPv4 header | ~20 bytes | kernel; `ttl` from `--ttl` |
| **IPv4 packet on wire** | ~84 bytes | IP + ICMP (approx.) |

With `-s 0`: ICMP message is header only (8 bytes); no timeval, reply line shows `8 bytes` and no `time=…`.

### Common ICMP types relevant to ping

| Type | Name | Code (examples) | Meaning |
|------|------|-----------------|--------|
| **0** | Echo Reply | 0 | Response to your probe |
| **8** | Echo Request | 0 | Your outgoing probe |
| **3** | Destination Unreachable | 0–15 | Host/net/port unreachable, filtered, … |
| **5** | Redirect | 0–3 | Router suggests a better path |
| **11** | Time Exceeded | 0 = TTL in transit, 1 = frag reassembly | Packet died in transit or reassembly timed out |
| **12** | Parameter Problem | — | Bad IP/ICMP header field |
| **4** | Source Quench | — | Congestion signal (legacy) |

---

## ICMP Echo Request and Echo Reply (detailed)

Ping is not a separate protocol. It is the common name for the **Echo Request / Echo Reply** exchange defined in ICMP. One side sends type **8**; if the destination is reachable and allowed to respond, it answers with type **0** carrying the same identifying fields and (usually) the same data.

### Purpose

| Goal | How echo helps |
|------|----------------|
| **Reachability** | A reply proves there is a working path to the host and back |
| **Latency (RTT)** | Timestamp in the payload is echoed; compare send vs receive time |
| **Loss** | Missing replies for a given `seq` mean loss or filtering on the path |
| **Duplicates** | Same `seq` twice → replayed or mis-routed packet |

Echo messages are **not errors**. They are a voluntary request/response pair. Routers forward them like any other IP packet (unless firewalls block ICMP).

### End-to-end flow

```
  ft_ping (source)                network                    target host
        |                            |                              |
        |  1. Build ICMP Echo Req    |                              |
        |     type=8, id, seq, data  |                              |
        |--------------------------->|  IP forward (TTL decrements) |
        |                            |----------------------------->|
        |                            |                              |
        |                            |     2. Kernel ICMP module    |
        |                            |        receives type 8       |
        |                            |        builds type 0 reply   |
        |                            |        swaps IP src/dst      |
        |                            |        copies id, seq, data  |
        |                            |                              |
        |                            |  3. Echo Reply (type 0)      |
        |                            |<-----------------------------|
        |  4. recvmsg on raw socket  |                              |
        |<---------------------------|                              |
        |  5. Match id, print RTT    |                              |
```

On **127.0.0.1** (loopback), the same machine is both sender and responder: the kernel receives the request and generates the reply without leaving the host.

### Echo Request (type 8, code 0)

| Field | Sender sets | Notes |
|-------|-------------|-------|
| **type** | `8` (`ICMP_ECHO`) | Fixed for all ping probes |
| **code** | `0` | Must be zero for echo |
| **checksum** | [RFC 1071](../rfc/rfc1071.txt) | Over header + entire payload |
| **identifier** | Process-chosen 16-bit value | Stays constant for one `ft_ping` run |
| **sequence** | Per-probe counter | Usually starts at 0 and increments |
| **data** | Arbitrary bytes | Often includes a send timestamp |

The request does **not** ask a question in text. The “question” is only: *can you reach me, and can you give my bits back unchanged?*

### Echo Reply (type 0, code 0)

The responder (almost always the **target OS kernel**, not a user program) does the following:

1. **Validate** the incoming ICMP echo (checksum, not broadcast-only rules, etc.).
2. **Allocate** a new ICMP message.
3. Set **type = 0** (`ICMP_ECHOREPLY`), **code = 0**.
4. Copy **identifier** and **sequence** unchanged from the request.
5. Copy **payload** byte-for-byte (so the sender’s timestamp comes back).
6. Recompute **checksum**.
7. Send inside a new IP packet: **source = former destination**, **destination = former source**.

| Field | In reply | Must match request? |
|-------|----------|---------------------|
| type | `0` | — (different from request) |
| code | `0` | yes (zero) |
| identifier | same | **yes** — used to pair reply with process |
| sequence | same | **yes** — used to pair reply with probe |
| data | same bytes | **yes** for RTT via embedded timestamp |

If `id` or `seq` differ from what you sent, the packet is not the answer to that probe (or is corrupted).

### Identifier and sequence in practice

**Identifier (`id`)**

- Distinguishes **multiple ping clients** on one machine.
- `ft_ping` uses `getpid() & 0xFFFF` so two simultaneous `ft_ping` processes rarely share the same `id`.
- Replies with a different `id` are **ignored** in `recv_ping()`.
- Full walkthrough: [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md).

**Sequence (`seq`)**

- Increments after each **successful** `sendto` in `ft_ping` (`ping->seq++` in `send_ping()`).
- Lets you spot **gaps** (lost probes) and **repeats** (duplicate replies).
- Stored in the ICMP header in **network byte order** (`htons` on send, `ntohs` on receive).

Example timeline for `-c 3`:

```
send: seq=0  →  reply: seq=0  →  print icmp_seq=0
send: seq=1  →  (timeout)     →  no line
send: seq=2  →  reply: seq=2  →  print icmp_seq=2
send: seq=2  →  reply: seq=2  →  print icmp_seq=2 (DUP!)
```

Duplicate detection uses a bitmap (`recv_table` in `t_ping`): the same `seq` seen twice prints `(DUP!)` and increments `num_rept`.

### Payload and round-trip time (RTT)

Default data size is **56 bytes**. Layout in `ft_ping` (see [Complete ICMP message — header + payload](#complete-icmp-message--header--payload) above):

```
┌──────────────────────────────────────────────────────────┐
│  ICMP header (8 bytes)                                   │
├──────────────────────────────────────────────────────────┤
│  first sizeof(struct timeval) bytes: send timestamp      │  ← gettimeofday() in send_ping()
├──────────────────────────────────────────────────────────┤
│  rest: pattern (-p) or 0x00, 0x01, 0x02, …             │  ← init_data_buffer()
└──────────────────────────────────────────────────────────┘
```

**RTT algorithm** (`update_timing()` in `srcs/print.c`):

1. On **send**: `gettimeofday(&tv, NULL)` → copy into start of ICMP data.
2. On **reply**: read the same `struct timeval` from echoed data.
3. `gettimeofday` again at receive → `triptime = tv_recv - tv_send` in milliseconds.

The network does not compute RTT; it only **returns your timestamp**. Clock on the remote host is irrelevant because the echoed timeval was generated locally before send.

If `-s 0` (zero data bytes), there is no room for a timestamp and the reply line omits `time=...`.

### Full packet: what leaves and what returns

**Outbound** (logical view):

```
┌─────────────────────────────────────────────────────────────┐
│ IP (kernel): src=you, dst=target, TTL=--ttl, proto=ICMP(1)  │
├─────────────────────────────────────────────────────────────┤
│ ICMP Echo Request                                           │
│   type=8  code=0  checksum  id=0xABCD  seq=0                │
│   data: [tv_sec/tv_usec | ... padding ...]                  │
└─────────────────────────────────────────────────────────────┘
```

**Inbound Echo Reply**:

```
┌─────────────────────────────────────────────────────────────┐
│ IP (kernel): src=target, dst=you, TTL=peer_remaining        │
├─────────────────────────────────────────────────────────────┤
│ ICMP Echo Reply                                             │
│   type=0  code=0  checksum  id=0xABCD  seq=0   ← same id/seq│
│   data: [same tv_sec/tv_usec | ... same padding ...]        │
└─────────────────────────────────────────────────────────────┘
```

`ft_ping` passes **only the ICMP portion** to `sendto()`. On `recvmsg()`, the buffer contains **IP header + ICMP**; the program skips `ip_hl * 4` bytes to reach the ICMP header.

### Matching logic in `recv_ping()`

After reading a packet:

```
if type == ECHOREPLY (0) and id == ping->ident:
    → success: print_echo_reply()
else if type == ECHO (8):
    → ignore (someone else's ping to us)
else:
    → ICMP error path: print_icmp_error()
```

This is why you never print lines for other users’ pings on the same host: their `id` differs.

### What can go wrong (no Echo Reply)

| Situation | Typical result |
|-----------|----------------|
| Target down or unroutable | ICMP **Destination Unreachable** (type 3) or silence |
| TTL too low | **Time Exceeded** (type 11) from a router — see [TTL.md](TTL.md) |
| Firewall drops ICMP echo | Timeout, no reply line |
| Reply arrives after timeout | May appear late or be ignored by higher-level logic |
| Wrong `id` | Ignored — not counted as your reply |

Echo Request failure does **not** always produce an ICMP error; firewalls often **drop** without telling you.

### Byte order reminder

| Field | On wire | In `ft_ping` |
|-------|---------|--------------|
| `id`, `seq` | big-endian (network) | `htons()` before send, `ntohs()` after receive |
| `checksum` | big-endian | `checksum()` returns value in host layout; stored in header field per OS struct |
| Payload `timeval` | opaque copy | `memcpy` — no byte swap |

### Step-by-step: one probe in `ft_ping`

1. **Main loop** calls `send_ping(ping)`.
2. Allocate `8 + data_length` bytes; zero-fill.
3. Fill ICMP: type 8, code 0, id, seq.
4. Write `gettimeofday` at data offset 0; fill rest from `data_buffer`.
5. Checksum entire ICMP message; `sendto` to `dest_addr`.
6. `num_xmit++`, `seq++`.
7. **Main loop** calls `recv_ping(ping)` (possibly many times per send).
8. `recvmsg` → parse IP header length → point to ICMP.
9. If type 0 and id matches → `print_echo_reply`: compute RTT, print `bytes`, `icmp_seq`, `ttl`, `time`.
10. Statistics updated (`num_recv`, min/max/avg/stddev).

### Echo vs other ICMP traffic

| Message | Direction | Role in ping |
|---------|-----------|--------------|
| Echo Request (8) | You → target | Your probe |
| Echo Reply (0) | Target → you | Success |
| Time Exceeded (11) | Router → you | TTL expired — not a reply |
| Dest Unreachable (3) | Router/host → you | Delivery failed — not a reply |

Only type **0** with matching **id** counts as a successful ping reply for statistics and `-c` count.

---

## Checksum ([RFC 1071](../rfc/rfc1071.txt))

The ICMP checksum covers the **entire ICMP message** (header + data). Algorithm:

1. Set checksum field to **0**.
2. Sum all 16-bit words, adding carry into the low 16 bits.
3. Store **one’s complement** of the sum.

In `ft_ping`, `checksum()` in `srcs/checksum.c` implements this. `send_ping()` zeroes the field, computes the checksum, then writes it before `sendto()`.

---

## ICMP error messages and the “quoted packet”

When a router or host cannot forward/deliver a packet, it often sends an ICMP **error** back to the original sender. The error carries a copy of the **original IP packet** (or the start of it) so the sender knows which probe failed:

```
┌─────────────────────────────────────┐
│  Outer IP header (from router)      │
├─────────────────────────────────────┤
│  ICMP error (type 3, 11, …)         │
│    type, code, checksum             │
│    (4 unused bytes)                 │
├─────────────────────────────────────┤
│  Quoted: inner IP + start of ICMP   │  ← your original Echo Request
└─────────────────────────────────────┘
```

`ft_ping` uses the **inner IP destination** (`inner_ip->ip_dst`) to decide whether the error relates to the current target. Without `-v`, unrelated errors are filtered out.

---

## How ft_ping uses ICMP

The sections above describe the **Echo Request / Reply protocol** in general. Below is a compact map to the implementation. For field-by-field behavior, RTT, and matching rules, see [ICMP Echo Request and Echo Reply (detailed)](#icmp-echo-request-and-echo-reply-detailed).

### Socket creation

```c
socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

Requires **root** (`sudo`): only privileged processes may open raw ICMP sockets on most systems.

### Sending (`srcs/send.c`)

| Step | Action |
|------|--------|
| Allocate | `8 + data_length` bytes |
| Header | `type = ICMP_ECHO (8)`, `code = 0` |
| Identity | `id = htons(getpid() & 0xFFFF)`, `seq = htons(ping->seq)` |
| Payload | `gettimeofday()` at start of data (for RTT), then pattern or default fill |
| Checksum | [RFC 1071](../rfc/rfc1071.txt) over full message |
| Send | `sendto(sockfd, packet, …)` — **ICMP only**, kernel adds IP |

After a successful send: `num_xmit++`, `seq++`.

### Receiving (`srcs/recv.c`)

1. `recvmsg()` into a buffer; first bytes are the **IP header**.
2. `ip_hdr_len = ip_hdr->ip_hl << 2` (header length in 32-bit words × 4).
3. ICMP starts at `buf + ip_hdr_len`.
4. **Echo Reply** (`type 0`) with `id == ping->ident` → `print_echo_reply()`.
5. **Foreign Echo Request** (`type 8`) → ignored.
6. Any other type → `print_icmp_error()` (subject to verbose filtering).

### Printing (`srcs/print.c`)

**Successful reply:**

```
64 bytes from 8.8.8.8: icmp_seq=0 ttl=118 time=1.234 ms
```

- `64` = ICMP header (8) + data (56), not full IP size.
- `icmp_seq` from the ICMP header (host byte order after `ntohs`).
- `ttl` from the **outer IP header** of the reply (see [TTL.md](TTL.md)).
- `time` = difference between receive time and timestamp embedded in echoed payload.

**ICMP error** (example with `--ttl 1` to a remote host):

```
From 192.168.1.1 icmp_seq=0 Time to live exceeded
```

With `-v`, also prints `IP Hdr Dump:` and inner ICMP details (`print_inner_ip_data()`).

### Cross-platform ICMP structures

Linux uses `struct icmphdr`; macOS uses `struct icmp`. `includes/ft_ping.h` defines macros (`ICMP_HDR_TYPE`, `ICMP_HDR_ID`, …) so the rest of the code stays portable.

### ICMP identifier

Default: `ident = getpid() & 0xFFFF`. Replies are accepted only when `ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident`, so multiple `ping` processes on one machine do not cross-match replies. See [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md) for a detailed breakdown (PID, bit mask, send/recv flow, collisions).

## Further reading

- [RFC 792](../rfc/rfc792.txt) — ICMP message formats, echo, errors
- [RFC 1071](../rfc/rfc1071.txt) — checksum algorithm
- [RFC 1122](../rfc/rfc1122.txt) — host echo reply requirements
