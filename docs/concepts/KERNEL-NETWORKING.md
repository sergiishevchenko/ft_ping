# Kernel networking and system calls

The `ft_ping` process does **not** talk to the network card directly. It asks the **kernel** to do that on its behalf. The kernel owns drivers, routing, and most of the protocol stack; the program receives a **socket** (a file descriptor) and uses **system calls** to configure it and move data.

This page explains that split: what runs in kernel space, what runs in user space, and how each network syscall used by `ft_ping` fits in.

Related: [OSI-TCP-IP.md](OSI-TCP-IP.md) (layer model), [SOCKET.md](SOCKET.md) (raw socket setup), [RECV.md](RECV.md) (`recvmsg` details).

---

## User space vs kernel space

Modern Unix (Linux, macOS, BSD) separates memory and privileges into two worlds:

| | **User space** | **Kernel space** |
|--|----------------|------------------|
| Who runs there | User-space code (`ft_ping`, `libc`) | Operating system core |
| Privilege | Limited — cannot touch hardware directly | Full — drivers, interrupts, all RAM |
| Crash effect | Usually kills one process | Can panic the whole machine |
| Network access | Only through **system calls** | Direct hardware + internal APIs |

```
┌─────────────────────────────────────────────────────────────────┐
│  USER SPACE                                                     │
│  ┌──────────────┐   syscall    ┌─────────────────────────────┐ │
│  │   ft_ping    │ ───────────► │  Kernel network stack       │ │
│  │  main.c      │ ◄─────────── │  (sockets, IP, routing, …)  │ │
│  │  send.c      │              └──────────────┬──────────────┘ │
│  │  recv.c      │                             │                │
│  └──────────────┘                             ▼                │
└───────────────────────────────────────────────│────────────────┘
                                                │ driver API
┌───────────────────────────────────────────────│────────────────┐
│  KERNEL SPACE                                 ▼                │
│  ┌─────────────┐    DMA/interrupts    ┌─────────────┐          │
│  │ NIC driver  │ ◄──────────────────► │  Ethernet   │          │
│  │ (e1000, …)  │                      │  hardware   │          │
│  └─────────────┘                      └─────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

When `sendto()` returns success, it only means the kernel **accepted** the send buffer — not necessarily that the packet already left the machine. The driver may queue it for transmission milliseconds later.

---

## What the kernel owns

### Network interface (NIC) drivers

The **Network Interface Card** (Wi‑Fi or Ethernet) speaks electrical or radio signals. Only a **driver** in the kernel knows how to program that chip: set MAC address, push frames out, handle interrupts when frames arrive.

`ft_ping` never calls driver functions. It never sees Ethernet headers on send (the kernel builds them after routing decides *which* interface to use).

### Routing tables

To reach `8.8.8.8`, the kernel looks up a **routing table**:

- Is the destination on the local subnet? → send via ARP to that host.
- Otherwise → forward to the **default gateway** (router IP on the LAN).

Flag **`-r`** (`SO_DONTROUTE`) tells the kernel: *skip this lookup* and only send to directly connected networks.

`ft_ping` stores the destination in `ping->dest_addr` (from `getaddrinfo()` in `dns.c`). The kernel uses that address on every `sendto()`; the program does not implement routing logic.

### Protocol stack

Inside the kernel, layered software handles protocols. For `ft_ping` the important parts are:

| Layer (conceptual) | Kernel responsibility | `ft_ping` involvement |
|--------------------|----------------------|------------------------|
| **ICMP** | Validate, demux, generate some replies | **Build** Echo Request in user space; **parse** replies in `recv.c` |
| **IPv4** | Add/strip IP header, decrement TTL, fragment if needed | Set TTL/TOS/options via `setsockopt`; read IP header from raw recv buffer |
| **Link (L2)** | Ethernet/Wi‑Fi framing, ARP | None |
| **Driver** | DMA to/from NIC | None |

ICMP is special: it is not TCP or UDP. It rides **directly on IP** (protocol field = 1). See [ICMP.md](ICMP.md) and [OSI-TCP-IP.md](OSI-TCP-IP.md).

### Socket layer

The **socket API** is the kernel’s uniform door into networking. A socket is:

1. An **object** inside the kernel (buffers, state, protocol hooks).
2. Exposed to the process as a small integer **file descriptor** (`ping->sockfd`), like a file open for read/write.

Opening, configuring, and closing sockets are all syscalls. Data transfer too.

### TCP and UDP: what the kernel hides

Most programs use **connected** sockets (**TCP**, `SOCK_STREAM`) or **datagram** sockets (**UDP**, `SOCK_DGRAM`). In both cases the kernel presents a simple contract: **the application sends and receives payload bytes**. IP and transport headers stay inside the kernel — the application does not assemble or parse them in normal app code.

**TCP (stream socket)**

```
Application send buffer          Kernel builds on the wire
┌─────────────────┐         ┌──────┬─────┬──────────────┬──────────┐
│ "GET / HTTP..." │  ───►   │ Eth  │ IP  │ TCP hdr+ports│ application data│
└─────────────────┘         └──────┴─────┴──────────────┴──────────┘
                              ▲ hidden    ▲ hidden
```

- Kernel adds **TCP header** (ports, sequence, checksum) and **IP header** (src/dst, TTL).
- Kernel handles retransmits, ordering, connection state (`connect`, `listen`, `accept`).
- A `read()` / `write()` call see a **byte stream**, not individual packets.

**UDP (datagram socket)**

```
Application sendto() buffer        Kernel builds on the wire
┌─────────────────┐         ┌──────┬─────┬──────────────┬──────────┐
│ DNS query bytes │  ───►   │ Eth  │ IP  │ UDP hdr+ports│ payload  │
└─────────────────┘         └──────┴─────┴──────────────┴──────────┘
```

- One `sendto()` → one UDP datagram (up to MTU limits).
- Kernel still hides IP and UDP headers; only the payload is passed + destination address/port.
- `recvfrom()` returns payload + peer address/port — not raw IP.

| | TCP (`SOCK_STREAM`) | UDP (`SOCK_DGRAM`) |
|--|---------------------|---------------------|
| Abstraction | Reliable byte stream | Unreliable messages |
| Headers built by | Kernel | Kernel |
| What `send` delivers | App data only | App data only |
| Addressing | `connect()` or implicit after connect | `sendto()` / `recvfrom()` with port |
| Typical use | HTTP, SSH | DNS, video, games |

**Why `ft_ping` is different**

ICMP is **not** TCP or UDP. There is no port, no stream, and no kernel wrapper that builds Echo Request on a raw socket. `ft_ping` uses `SOCK_RAW` + `IPPROTO_ICMP`:

| | TCP/UDP socket | Raw ICMP (`ft_ping`) |
|--|----------------|----------------------|
| Syscall API | `send` / `sendto` on payload | `sendto` on **ICMP message** |
| Kernel adds | TCP or UDP + IP + L2 | **IP** + L2 only |
| On receive | Payload (and maybe peer port) | **Full IP datagram** + ICMP |
| Who builds ICMP | N/A (different protocol) | **Application code** in `send_ping()` |

So the same socket API (`socket`, `sendto`, `recvmsg`) sits on top of very different kernel behavior. See [SOCKET.md](SOCKET.md) for how `ft_ping` configures and uses its raw socket.

---

## What the application implements

`ft_ping` is responsible for everything the kernel does **not** do for a raw ICMP socket:

| Task | Where |
|------|--------|
| Parse CLI flags | `main.c` → `parse_args()` |
| Resolve hostname to IPv4 | `dns.c` → `getaddrinfo()` |
| Open and configure socket | `socket.c` |
| Build ICMP header + payload | `send.c` → `send_ping()` |
| Compute ICMP checksum | `checksum.c` |
| Main loop timing (`select`, interval) | `main.c` → `ping_loop()` |
| Parse IP + ICMP on receive | `recv.c` → `recv_ping()` |
| Print lines and statistics | `print.c`, `stats.c` |

The kernel adds the **IPv4 header** on send and delivers **IP + ICMP** on receive. The application adds the **ICMP** body.

---

## System calls: the boundary

A **system call** (syscall) is how user code enters the kernel safely. In C, code typically calls libc wrappers (`sendto`, not `syscall(SYS_sendto, …)` directly).

Typical flow:

1. The caller invokes `sendto(sockfd, buf, len, …)`.
2. libc puts arguments in registers / on stack and executes a **trap** instruction.
3. CPU switches to kernel mode; kernel validates `sockfd`, copies data from the send buffer if needed.
4. Kernel runs network stack logic.
5. Kernel returns a result (byte count or `-1` + `errno`) to the calling process.

```
  ft_ping                    libc                 kernel
     │                         │                     │
     │  sendto(fd, buf, …)     │                     │
     ├────────────────────────►│  trap + args        │
     │                         ├────────────────────►│ validate fd
     │                         │                     │ build/route packet
     │                         │◄────────────────────┤ return n or -1
     │◄────────────────────────┤                     │
     │  n bytes or -1, errno   │                     │
```

Every syscall has a cost (mode switch). `ping` sends at most one packet per second (or faster in flood mode), so this is negligible.

---

## Syscalls used by `ft_ping`

### Overview

| Syscall | File | When | Purpose |
|---------|------|------|---------|
| `socket()` | `socket.c` | Startup | Create raw ICMP endpoint |
| `setsockopt()` | `socket.c` | Startup | TTL, TOS, `-r`, timeouts, IP options |
| `sendto()` | `send.c` | Each probe | Hand ICMP message to kernel for send |
| `select()` | `main.c` | Each loop iter | Wait for readable socket without long block |
| `recvmsg()` | `recv.c` | When data ready | Read one incoming datagram + peer address |
| `close()` | `main.c` | Exit | Release socket |
| `setuid()` | `main.c` | After `socket()` | Drop root; keep fd open |

DNS uses `getaddrinfo()` (libc, may use other syscalls internally) — see [GETADDRINFO.md](GETADDRINFO.md).

---

### `socket()` — create endpoint

```c
ping->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

**Kernel creates:**

- A new socket data structure.
- An fd in the process file table (`0`, `1`, `2` are stdin/stdout/stderr; the socket receives the next free number).

**Requires root** on most systems because `SOCK_RAW` allows crafting low-level packets. Without privilege: `EPERM` → `Operation not permitted`.

See [SOCKET.md](SOCKET.md) for argument meanings.

---

### `setsockopt()` — configure socket

```c
setsockopt(sockfd, level, option_name, &value, sizeof(value));
```

**Levels used in `ft_ping`:**

| `level` | Meaning |
|---------|---------|
| `SOL_SOCKET` | Generic socket options (`SO_BROADCAST`, `SO_DONTROUTE`, `SO_RCVTIMEO`) |
| `IPPROTO_IP` | IPv4-specific (`IP_TTL`, `IP_TOS`, `IP_OPTIONS`) |

The kernel stores these on the **socket object**. They apply to every packet sent on that fd until changed.

Examples:

- `IP_TTL` → copied into outgoing IP header **TTL** field ([TTL.md](TTL.md)).
- `IP_TOS` → **Type of Service** byte ([TOS.md](TOS.md)).
- `IP_OPTIONS` → raw option bytes prepended to IP header (`--ip-timestamp`).

Failure returns `-1`; `ft_ping` prints `ft_ping: setsockopt(...): <reason>` and aborts startup.

---

### `sendto()` — send one datagram

```c
sendto(ping->sockfd, packet, pkt_sz, 0,
    (struct sockaddr *)&ping->dest_addr, sizeof(ping->dest_addr));
```

**Arguments:**

| Arg | Role |
|-----|------|
| `sockfd` | Which socket |
| `packet` | Buffer: ICMP type/code/checksum/id/seq + payload |
| `pkt_sz` | Length of ICMP message (not including IP header) |
| `flags` | `0` in `ft_ping` |
| `dest_addr` | IPv4 destination (`sockaddr_in` from DNS) |

**Kernel path (simplified):**

```
sendto()
  ├─ Copy ICMP bytes from user buffer
  ├─ Build IPv4 header (src IP from interface, dst from dest_addr)
  ├─ Apply socket options (TTL, TOS, IP options)
  ├─ Route: which interface? next-hop MAC? (routing table)
  ├─ Wrap in Ethernet frame (ARP if needed)
  └─ Queue to NIC driver
```

Return value: number of bytes **accepted** for sending (often equals `pkt_sz`), or `-1` on error.

`ft_ping` increments `num_xmit` and `seq` only after successful `sendto()`.

---

### `select()` — wait without blocking the whole second

```c
FD_ZERO(&readfds);
FD_SET(ping->sockfd, &readfds);
tv.tv_sec = 0;
tv.tv_usec = 10000;   /* 10 ms */
select(ping->sockfd + 1, &readfds, NULL, NULL, &tv);
```

`select` asks the kernel: *among these fds, is any readable within 10 ms?*

- Returns `> 0` if `sockfd` has data → call `recv_ping()`.
- Returns `0` on timeout → loop continues, may send next probe if interval elapsed.
- Returns `-1` on error; `EINTR` if signal arrived (e.g. Ctrl+C) — loop retries.

**Why not block only in `recvmsg`?** The ping loop must also **send** on a timer (1 s default). Short `select` timeouts let the loop wake regularly. `SO_RCVTIMEO` (1 s) is a separate backstop on the socket itself.

---

### `recvmsg()` — receive one datagram + metadata

```c
recvmsg(ping->sockfd, &msg, 0);
```

Unlike plain `recv()`, `recvmsg` uses `struct msghdr` to get:

- **Data** — via `iovec` pointing at `buf` (IP + ICMP in raw socket).
- **Peer address** — via `msg.msg_name` → `struct sockaddr_in from` (who sent the reply).

**Kernel path on receive:**

```
NIC interrupt → driver → kernel queues packet
  ├─ Demux: IPv4, protocol ICMP
  ├─ Match raw socket (type, protocol)
  └─ Buffer until recvmsg() copies to user space
```

`recv_ping()` ignores short packets, filters Echo Reply by `ping->ident`, dispatches errors to `print_icmp_error()`. Full walkthrough: [RECV.md](RECV.md).

Non-fatal errors: `EAGAIN` / `EWOULDBLOCK` (timeout), `EINTR` (signal) → return 0 and continue.

---

### `close()` — release resources

```c
close(ping->sockfd);
```

Kernel frees socket buffers and removes the fd from the process. Called from `cleanup()` at exit.

---

### `setuid()` — privilege drop (not a network syscall, but related)

```c
setuid(getuid());
```

After `socket()` succeeds as root, `main()` drops effective UID to the real user. The **open socket stays valid** — capability was needed to *create* raw socket, not to use it on every packet on most Unix systems.

---

## End-to-end: one Echo Request and Reply

```
USER SPACE                          KERNEL                           NETWORK
──────────                          ──────                           ───────

send_ping()
  build ICMP type 8
  checksum()
       │
       ▼
sendto(ICMP) ───────────────────►  append IP hdr
                                   route lookup
                                   L2 frame ─────────────────────►  target host
                                                                        │
                                                                        ▼
                                                                   ICMP Echo Reply
recvmsg() ◄──────────────────────  queue on raw socket ◄──────────────┘
       │
       ▼
recv_ping()
  parse IP + ICMP
  print_echo_reply()
```

---

## File descriptor overview

Treat `ping->sockfd` like an open file:

| File I/O | Socket I/O (`ft_ping`) |
|----------|-------------------------|
| `open()` | `socket()` |
| `fcntl` / ioctl-like setup | `setsockopt()` |
| `write()` | `sendto()` |
| `read()` | `recvmsg()` |
| `poll` / `select` | `select()` on sockfd |
| `close()` | `close()` |

Data is not stored in the process until `recvmsg` copies it. The kernel may buffer several incoming packets if packets are not read promptly (unusual for ping rates).

---

## Errors and `errno`

When a syscall returns `-1`, the thread-local **`errno`** explains why:

| `errno` | Typical syscall | Meaning for `ft_ping` |
|---------|-----------------|------------------------|
| `EPERM` | `socket()` | Not root |
| `EINVAL` | `setsockopt()` | Bad option/value |
| `EAGAIN` / `EWOULDBLOCK` | `recvmsg()` | No data within timeout |
| `EINTR` | `select()`, `recvmsg()` | Interrupted by signal |
| `ENETUNREACH` | `sendto()` | No route to host |

Messages are printed as `ft_ping: <call>: <strerror(errno)>`.

---

## How this differs from a normal `ping` binary

The system `/usr/bin/ping` also uses the kernel stack, but implementation may differ:

- May use `SOCK_DGRAM` ICMP socket on some OSes (kernel builds ICMP header).
- `ft_ping` uses **`SOCK_RAW`** and builds ICMP itself — closer to inetutils behavior on Linux.

In **all** cases the NIC driver and routing table remain kernel-owned. Only the division of labor between libc/app and kernel changes.

---

## Related docs

| Document | Topic |
|----------|--------|
| [SOCKET.md](SOCKET.md) | Raw socket creation and options in `socket.c` |
| [RECV.md](RECV.md) | `recvmsg`, `msghdr`, parsing |
| [OSI-TCP-IP.md](OSI-TCP-IP.md) | Where ICMP and IP sit in the stack |
| [ARCHITECTURE.md](../ARCHITECTURE.md) | Module map and startup order |
| [COMMAND_FLOW.md](../COMMAND_FLOW.md) | Per-flag execution path |
