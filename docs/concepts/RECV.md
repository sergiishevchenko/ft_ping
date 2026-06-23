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

## `struct msghdr` and `recvmsg` setup (lines 14–21)

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

| Field | Value | Effect |
|-------|-------|--------|
| `iov.iov_base` | `buf` | Packet bytes written here |
| `iov.iov_len` | `65536` | Max bytes to accept |
| `msg.msg_name` | `&from` | Kernel fills sender’s `sockaddr_in` |
| `msg.msg_namelen` | `sizeof(from)` | In/out length of address buffer |
| `msg.msg_iov` | `&iov` | Array of one iovec |
| `msg.msg_iovlen` | `1` | One buffer segment |
| flags | `0` | No `MSG_DONTWAIT` — blocking read |

**Why `recvmsg` instead of `recv`:** you get both the datagram **and** the source address in one syscall (`msg_name`).

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
