# `recv_ping()` — receiving and dispatching ICMP packets

`recv_ping()` in `srcs/recv.c` is called from `ping_loop()` when `select()` reports that the raw ICMP socket is readable. It reads **one** datagram, parses the **IPv4** and **ICMP** headers, and either:

- prints a normal **Echo Reply** line,
- prints an **ICMP error** (TTL exceeded, unreachable, …),
- or **ignores** the packet silently.

Related: [ICMP.md](ICMP.md), [IPv4.md](IPv4.md), [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md), [ARCHITECTURE.md](../ARCHITECTURE.md).

---

## Full source

```c
#include "ft_ping.h"

int	recv_ping(t_ping *ping)
{
	uint8_t				buf[RECV_BUFSIZE];
	struct sockaddr_in	from;
	struct iovec		iov;
	struct msghdr		msg;
	ssize_t				bytes;
	struct ip			*ip_hdr;
	t_icmphdr			*icmp_hdr;
	int					ip_hdr_len;

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	bytes = recvmsg(ping->sockfd, &msg, 0);
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return (0);
		fprintf(stderr, "ft_ping: recvmsg: %s\n", strerror(errno));
		return (-1);
	}
	ip_hdr = (struct ip *)buf;
	ip_hdr_len = ip_hdr->ip_hl << 2;
	if (bytes < ip_hdr_len + PING_PKT_HDR_SZ)
		return (0);
	icmp_hdr = (t_icmphdr *)(buf + ip_hdr_len);
	if (ICMP_HDR_TYPE(icmp_hdr) == ICMP_ECHOREPLY
		&& ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident)
	{
		print_echo_reply(ping, &msg, buf, bytes);
		return (0);
	}
	if (ICMP_HDR_TYPE(icmp_hdr) != ICMP_ECHO)
	{
		print_icmp_error(&from, ip_hdr, icmp_hdr,
			(int)bytes, ping);
	}
	return (0);
}
```

---

## Role in the program

```
ping_loop()
    │
    select(sockfd, 10ms timeout)
    │
    └─ readable? ──► recv_ping(ping)   ← one call per loop iteration
                          │
              ┌───────────┼───────────┐
              ▼           ▼           ▼
      print_echo_reply  print_icmp_error  (silent)
```

`recv_ping` does **not** loop until the socket queue is empty. The outer `ping_loop` calls it at most once per iteration.

---

## What arrives on a raw ICMP socket

On **send**, `ft_ping` passes only ICMP bytes to `sendto()`; the kernel adds the IP header.

On **receive**, `recvmsg()` returns **IP header + ICMP message** in one buffer:

```
buf[0 … bytes-1]
┌──────────────────────┬─────────────────────┬──────────────────┐
│ IPv4 header          │ ICMP header (8 B)   │ ICMP payload     │
│ ip_hdr_len bytes     │ PING_PKT_HDR_SZ     │ data (e.g. 56 B) │
└──────────────────────┴─────────────────────┴──────────────────┘
 ↑                      ↑
 struct ip *            t_icmphdr * = buf + ip_hdr_len
```

The **sender’s IPv4 address** is not taken from the IP header alone for display — it comes from `msg.msg_name` (`struct sockaddr_in from`), filled by the kernel alongside the datagram.

---

## Every local variable (type and purpose)

| Line | Declaration | C type | Purpose |
|------|-------------|--------|---------|
| 3 | `ping` (parameter) | `t_ping *` | Session state: `sockfd`, `ident`, counters, options |
| 5 | `buf` | `uint8_t[RECV_BUFSIZE]` | Raw receive buffer on the stack (65536 bytes) |
| 6 | `from` | `struct sockaddr_in` | **Source address** of the datagram (who sent it) |
| 7 | `iov` | `struct iovec` | Describes one memory slice for `recvmsg` (base + length) |
| 8 | `msg` | `struct msghdr` | Full receive control block: source address + iovec list |
| 9 | `bytes` | `ssize_t` | Return value of `recvmsg`: bytes read, or `-1` on error |
| 10 | `ip_hdr` | `struct ip *` | Pointer to IPv4 header at start of `buf` |
| 11 | `icmp_hdr` | `t_icmphdr *` | Pointer to ICMP header after the IP header |
| 12 | `ip_hdr_len` | `int` | Length of IP header in **bytes** |

### `t_ping *ping` — fields used in `recv_ping`

| Field | Type | Used for |
|-------|------|----------|
| `sockfd` | `int` | File descriptor passed to `recvmsg` |
| `ident` | `uint16_t` | Must match ICMP `id` in Echo Reply |
| `options` | `unsigned int` | Passed through to `print_icmp_error` (`OPT_VERBOSE`) |
| `dest_addr` | `struct sockaddr_in` | Used inside `print_icmp_error` to filter unrelated errors |

Other `t_ping` fields (`num_recv`, stats, …) are updated inside `print_echo_reply`, not in `recv_ping` itself.

---

## Every constant and macro used

### Project defines (`includes/ft_ping.h`)

| Name | Value | Role in `recv_ping` |
|------|-------|---------------------|
| `RECV_BUFSIZE` | `65536` | Size of `buf` — fits max IPv4 datagram |
| `PING_PKT_HDR_SZ` | `8` | Minimum ICMP header size; used to reject truncated packets |

### ICMP types (RFC 792 / system headers)

| Name | Value | Meaning |
|------|-------|---------|
| `ICMP_ECHOREPLY` | `0` | Echo Reply — success path when `id` matches |
| `ICMP_ECHO` | `8` | Echo Request — **ignored** if not our reply (see dispatch below) |

Other ICMP types (3 unreachable, 11 time exceeded, …) are handled via `print_icmp_error`.

### Portable ICMP header macros

` t_icmphdr` and `ICMP_HDR_*` hide Linux vs macOS struct layout differences:

| Macro | Accesses | Use in `recv_ping` |
|-------|----------|-------------------|
| `ICMP_HDR_TYPE(p)` | ICMP type byte | Compare to `ICMP_ECHOREPLY` / `ICMP_ECHO` |
| `ICMP_HDR_ID(p)` | Echo id field | Compare to `ping->ident` (with `ntohs`) |

On Linux: `struct icmphdr` with `type`, `un.echo.id`.  
On macOS: `struct icmp` with `icmp_type`, `icmp_hun.ih_idseq.icd_id`.

### `errno` values handled

| Constant | When | Action |
|----------|------|--------|
| `EAGAIN` | `recvmsg` would block (socket timeout) | `return 0` — normal |
| `EWOULDBLOCK` | Same as `EAGAIN` on some platforms | `return 0` |
| `EINTR` | Interrupted by signal (e.g. SIGINT) | `return 0` — loop continues, checks `g_stop` |

Socket receive timeout is set in `socket.c`: `SO_RCVTIMEO` = 1 second. `ping_loop` also uses `select` with 10 ms; either path can surface `EAGAIN`.

### Standard library / kernel types (not `#define` in `ft_ping.h`)

| Type | Header | Role |
|------|--------|------|
| `struct ip` | `<netinet/ip.h>` | IPv4 header layout; `ip_hl`, `ip_ttl`, … |
| `struct sockaddr_in` | `<netinet/in.h>` | IPv4 socket address (`sin_addr`, …) |
| `struct iovec` | `<sys/socket.h>` | `{ iov_base, iov_len }` scatter/gather slice |
| `struct msghdr` | `<sys/socket.h>` | `{ msg_name, msg_iov, … }` for `recvmsg` |
| `uint8_t` | `<stdint.h>` via sys types | Byte buffer |
| `ssize_t` | `<sys/types.h>` | Signed size for syscall return |

---

## Receive buffers: `buf`, `from`, `iovec`, `msghdr`

`recv_ping` does not call plain `recv()`. It uses **`recvmsg()`**, which needs two kinds of output from the kernel in one syscall:

1. **Packet bytes** → written into `buf` (described by `struct iovec`)
2. **Sender IPv4 address** → written into `from` (described by `msg.msg_name` inside `struct msghdr`)

### `uint8_t buf[RECV_BUFSIZE]`

A **byte array** on the stack — the raw datagram as it arrived on the wire (IP + ICMP + payload).

- `uint8_t` = one unsigned byte (0–255), exactly one octet of the packet.
- `RECV_BUFSIZE` = `65536` = max IPv4 datagram size.

`buf` holds the **contents** of the packet. It does **not** tell you who sent it — that is `from`.

### `struct sockaddr_in from`

Filled by the kernel when the packet arrives. Holds the **source IPv4 address** (and port field, unused for ICMP).

Used when printing:

```
64 bytes from 127.0.0.1: icmp_seq=0 ...
              ↑
         from.sin_addr
```

---

### `struct iovec` — name, layout, why it exists

**Name:** **io** (input/output) + **vec** (vector) — one entry in a list of `{pointer, length}` pairs telling the kernel **where to put incoming data**.

**Definition** (`<sys/socket.h>`):

```c
struct iovec {
    void  *iov_base;   /* start of buffer */
    size_t iov_len;    /* max bytes to write there */
};
```

| Field | In `recv_ping` | Meaning |
|-------|----------------|---------|
| `iov_base` | `buf` | “Write packet bytes starting here.” |
| `iov_len` | `sizeof(buf)` (= 65536) | “Do not write more than this many bytes.” |

`iovec` does **not** store the packet. It only **points at** `buf` and gives its size. Think of it as one line on a form: *put the parcel contents in this box, max 65536 bytes.*

**Why not pass `buf` directly to `recvmsg`?** The POSIX API is built for **scatter/gather**: one syscall can fill **several** memory regions. `ft_ping` uses only **one** region (`msg_iovlen = 1`), but the API shape is the same.

**Example with two buffers** (not used in `ft_ping`, but shows the name “vector”):

```c
uint8_t ip_part[60];
uint8_t icmp_part[4096];
struct iovec iov[2];

iov[0].iov_base = ip_part;
iov[0].iov_len  = sizeof(ip_part);
iov[1].iov_base = icmp_part;
iov[1].iov_len  = sizeof(icmp_part);

msg.msg_iov    = iov;
msg.msg_iovlen = 2;   /* kernel may split data across both */
```

`ft_ping` keeps it simple: one `buf`, one `iovec`.

---

### `struct msghdr` — name, layout, why it exists

**Name:** **msg** (message) + **hdr** (header) — a **control block** describing one receive operation for `recvmsg(2)`. It is **not** part of the packet on the wire; it is a form you fill in before calling the kernel.

**Plain idea:** `msghdr` answers two questions for the kernel in one syscall:

1. **Where do I put the packet bytes?** → via `msg_iov` → `iovec` → `buf`
2. **Where do I put the sender’s address?** → via `msg_name` → `from`

`msghdr` itself stores **no packet data** and **no IP address** — only pointers and sizes.

#### Layout (fields used in `recv_ping`)

```c
struct msghdr {
    void         *msg_name;       /* buffer for source address */
    socklen_t     msg_namelen;    /* size of that buffer (in/out) */
    struct iovec *msg_iov;        /* array of {pointer, length} for data */
    size_t        msg_iovlen;     /* number of iovec entries */
    /* msg_control, msg_flags, … — not used in ft_ping */
};
```

| Field | Set to in `recv_ping` | Role |
|-------|----------------------|------|
| `msg_name` | `&from` | Kernel writes **who sent** the datagram (`sockaddr_in`) |
| `msg_namelen` | `sizeof(from)` | Input: max size of `from`; output: actual size written |
| `msg_iov` | `&iov` | Points at one `iovec` that describes `buf` |
| `msg_iovlen` | `1` | Only one data buffer (unlike multi-buffer scatter/gather) |

#### How `recvmsg` uses `msg`

```c
bytes = recvmsg(ping->sockfd, &msg, 0);
```

The kernel reads your `msg`, receives one datagram from `sockfd`, then:

- fills **`from`** through `msg_name` (source IPv4 for `bytes from …`)
- fills **`buf`** through `msg_iov[0]` (raw bytes: IP + ICMP + payload)

Return value `bytes` = number of bytes written into `buf` (not including `from`).

#### Diagram: what points where

```
              struct msghdr msg
              ┌─────────────────────────────┐
              │ msg_name    ──────► &from    │  → 127.0.0.1 (sender)
              │ msg_namelen = sizeof(from)   │
              │ msg_iov     ──────► &iov     │
              │ msg_iovlen  = 1              │
              └─────────────────────────────┘
                                    │
                                    ▼
                            struct iovec iov
                            ┌──────────────────┐
                            │ iov_base ──► buf │  → packet bytes
                            │ iov_len  = 65536 │
                            └──────────────────┘
```

**Who / what:**

| Variable | Holds | Used for |
|----------|-------|----------|
| `from` | Sender IP | `64 bytes from **127.0.0.1**: …` |
| `buf` | Packet body | Parse IP header, ICMP, compute RTT |

#### Why not plain `recv()`?

```c
recv(sockfd, buf, len, 0);   /* only gives you buf — no sender address */
```

`ft_ping` prints the source IP on every reply line. `print_echo_reply()` reads:

```c
inet_ntoa(((struct sockaddr_in *)msg->msg_name)->sin_addr);
```

That requires `recvmsg` + `msghdr` + `msg_name` → `from`. You cannot get `from` from `buf` alone for display (you could parse the IP header’s source field, but the API already gives you `msg_name`).

#### `msghdr` vs `iovec` — division of labour

| Structure | Job |
|-----------|-----|
| **`iovec`** | One slice: “write **data** here, max N bytes.” |
| **`msghdr`** | Whole job: “write **data** (via `iovec` list) **and** **sender address** (via `msg_name`).” |

One `iovec` inside one `msghdr` is the minimal setup for `recv_ping`.

#### Setup in code (lines 14–20)

```c
memset(&msg, 0, sizeof(msg));   /* clear unused msghdr fields */

msg.msg_name    = &from;
msg.msg_namelen = sizeof(from);

msg.msg_iov     = &iov;          /* iov must already point at buf */
msg.msg_iovlen  = 1;
```

`memset` on `msg` is important: fields like `msg_control` stay NULL so the kernel does not write ancillary data.

#### After a successful call (example)

Ping reply from loopback, 84 bytes in `buf`:

```
msg.msg_name  →  from.sin_addr = 127.0.0.1
msg.msg_iov   →  buf[0..83]    = [ IP 20B | ICMP 8B | data 56B ]
bytes         =  84
```

`msg` is stack-local and reused on the next `recv_ping` call; only `buf` and `from` contents change.

---

### How the four pieces connect (`recv_ping` example)

**Before `recvmsg`:**

```
buf[65536]     ← empty byte array (packet will land here)
from           ← uninitialized sockaddr_in

iov.iov_base ──────────► buf[0]
iov.iov_len  = 65536

msg.msg_name    ───────► &from
msg.msg_namelen = sizeof(from)
msg.msg_iov     ───────► &iov
msg.msg_iovlen  = 1
```

**After `recvmsg` returns 84** (example: 20-byte IP + 8-byte ICMP + 56-byte data):

```
buf[0 … 83]    ← 84 bytes of packet (IP + ICMP + payload)
from.sin_addr  ← e.g. 127.0.0.1 (who replied)

bytes = 84
```

**Memory sketch:**

```
from                          buf
┌─────────────────┐          ┌────┬────┬────────────┐
│ sin_addr:       │          │ IP │ICMP│ payload    │
│  127.0.0.1      │          │20 B│ 8B │ 56 B       │
└─────────────────┘          └────┴────┴────────────┘
     ▲                              ▲
     │                              │
 msg.msg_name                   iov.iov_base
 (who sent)                     (what they sent)
```

---

### `recv()` vs `recvmsg()` — why `ft_ping` uses the latter

| Call | Gets packet bytes | Gets sender address | Needs `iovec` |
|------|-------------------|---------------------|---------------|
| `recv(sockfd, buf, len, 0)` | yes | no (not in one call) | no |
| `recvmsg(sockfd, &msg, 0)` | yes (via `msg_iov`) | yes (via `msg_name`) | yes |

`print_echo_reply` uses `msg->msg_name` for the `bytes from …` line, so `recvmsg` + `from` + `iovec` + `buf` are required together.

---

### Setup code in `recv_ping` (lines 14–21)

```c
memset(&msg, 0, sizeof(msg));
iov.iov_base = buf;
iov.iov_len = sizeof(buf);
msg.msg_name = &from;
msg.msg_namelen = sizeof(from);
msg.msg_iov = &iov;
msg.msg_iovlen = 1;
bytes = recvmsg(ping->sockfd, &msg, 0);
```

| Line | What it does |
|------|----------------|
| `memset(&msg, 0, …)` | Clear `msghdr`; unused fields default to zero |
| `iov.iov_base / iov_len` | “Data goes into `buf`, max 65536 bytes” |
| `msg.msg_name / msg_namelen` | “Sender address goes into `from`” |
| `msg.msg_iov / msg_iovlen` | “One `iovec` entry: `&iov`” |
| `recvmsg(…, 0)` | Block until one datagram (or error / timeout) |

Flags `0` = normal blocking read (no `MSG_DONTWAIT`).

---

## Error handling (lines 22–28)

```c
if (bytes < 0) { ... }
```

| Return | Meaning |
|--------|---------|
| `0` | No datagram processed (timeout, interrupt, or handled below) |
| `-1` | Hard `recvmsg` failure printed to stderr |

`recv_ping` almost always returns `0` so `ping_loop` keeps running. Only unexpected errors return `-1`.

---

## Parsing the IP header (lines 29–32)

```c
ip_hdr = (struct ip *)buf;
ip_hdr_len = ip_hdr->ip_hl << 2;
if (bytes < ip_hdr_len + PING_PKT_HDR_SZ)
    return (0);
```

### `ip_hl` (Internet Header Length)

- Stored in **4-bit units** (32-bit words).
- `ip_hl << 2` multiplies by 4 → length in bytes.
- Default: `ip_hl == 5` → `20` bytes.
- With IP options: `ip_hl` can be up to `15` → `60` bytes.

If the datagram is shorter than IP header + 8-byte ICMP header, the packet is dropped silently (corrupt, fragment, or unrelated traffic).

---

## Locating ICMP (line 33)

```c
icmp_hdr = (t_icmphdr *)(buf + ip_hdr_len);
```

ICMP starts immediately after the IP header. No guessing — offset comes from `ip_hl`.

---

## Dispatch logic (lines 34–45)

### Branch 1 — our Echo Reply (lines 34–38)

```c
if (ICMP_HDR_TYPE(icmp_hdr) == ICMP_ECHOREPLY
    && ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident)
    print_echo_reply(...);
```

Both conditions required:

| Check | Reason |
|-------|--------|
| Type `0` (`ICMP_ECHOREPLY`) | This is a reply to an echo, not an error |
| `id == ping->ident` | This reply belongs to **this** `ft_ping` process |

`ident` is set in `init_ping()` as `getpid() & 0xFFFF`.  
`ntohs()` converts the 16-bit `id` from **network byte order** to host order.

`print_echo_reply()` (in `print.c`) then:

- Computes RTT from embedded `timeval` in payload
- Updates `num_recv`, duplicate bitmap, min/max/avg stats
- Prints: `64 bytes from <ip>: icmp_seq=… ttl=… time=… ms`

### Branch 2 — ICMP errors (lines 40–44)

```c
if (ICMP_HDR_TYPE(icmp_hdr) != ICMP_ECHO)
    print_icmp_error(...);
```

| ICMP type | Branch 1 | Branch 2 | Visible result |
|-----------|----------|----------|----------------|
| `0` ECHOREPLY, **our id** | yes | — | Normal reply line |
| `0` ECHOREPLY, **wrong id** | no | yes (`0 != 8`) | `print_icmp_error` (may filter / print "Bad ICMP type: 0") |
| `8` ECHO (request) | no | **no** (`8 == 8`) | **Silent ignore** |
| `3`, `11`, … errors | no | yes | Error message (e.g. Time to live exceeded) |

So **incoming Echo Requests** (type 8) are never printed — another host pinging you is ignored.

`print_icmp_error()` may still **filter** errors: without `-v`, errors whose inner IP destination is not your target are hidden.

### Branch 3 — implicit (line 45)

`return (0);` — end of function. Covers:

- Echo Request ignored after branch 2 skipped
- Anything `print_icmp_error` chose not to print

---

## Decision flowchart

```
                    recvmsg → bytes
                         │
                    bytes < 0?
                    ├─ EAGAIN/EINTR → return 0
                    └─ other → return -1
                         │
                    bytes < ip_hdr_len + 8?
                    └─ yes → return 0 (too short)
                         │
              type == ECHOREPLY (0)?
                    └─ yes → id == ping->ident?
                              ├─ yes → print_echo_reply
                              └─ no  → fall through
                         │
              type != ECHO (8)?
                    ├─ yes → print_icmp_error
                    └─ no  → (silent)
                         │
                    return 0
```

---

## Return value summary

| Value | When |
|-------|------|
| `0` | Normal: processed, ignored, timeout, or interrupt |
| `-1` | `recvmsg` failed with unexpected `errno` |

`ping_loop` does not treat `-1` specially today; the loop may exit on the next `select` error path.

---

## Byte order

| Field | On wire | In code |
|-------|---------|---------|
| ICMP `id`, `seq` | Big-endian | `ntohs()` when comparing or printing |
| IP addresses in headers | Network order | `struct in_addr` / `sockaddr_in` |

Payload `timeval` is copied with `memcpy` — no conversion.

---

## Example walkthrough

**Command:** `sudo ./ft_ping -c 1 127.0.0.1`

1. `send_ping` sends Echo Request: type `8`, `id = 0x3a2f`, `seq = 0`.
2. `select` → readable.
3. `recv_ping`:
   - `recvmsg` → ~84 bytes (20 IP + 8 ICMP + 56 data) typical.
   - `ip_hdr_len = 20`.
   - `icmp_hdr` at `buf + 20`.
   - Type `0`, id `0x3a2f` matches → `print_echo_reply`.
4. Output: `64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.042 ms`
5. `ping_loop` sees `num_recv >= count` → exits.

**Command:** `sudo ./ft_ping --ttl 1 -v -c 1 8.8.8.8`

1. First router returns **Time Exceeded** (type `11`), not Echo Reply.
2. Branch 1 fails (type ≠ 0).
3. Branch 2: type `11` ≠ `8` → `print_icmp_error` → `Time to live exceeded` (+ verbose dump).

---

## Functions called from `recv_ping`

| Function | File | When |
|----------|------|------|
| `recvmsg` | libc | Read datagram |
| `print_echo_reply` | `print.c` | Echo Reply with matching `id` |
| `print_icmp_error` | `print.c` | Non–Echo-Request types (except silent cases) |

---

## Manual pages

- `recvmsg(2)` — receive message from socket
- `ip(4)` / IPv4 header — `ip_hl`
- `icmp(7)` — ICMP types and codes
