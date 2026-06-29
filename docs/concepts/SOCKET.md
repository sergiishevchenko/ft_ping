# Sockets in `ft_ping`

A **socket** is the operating-system endpoint the program uses to talk to the network. The kernel owns the actual NIC drivers, routing tables, and protocol stacks; application code opens a socket, configures it, then reads and writes through it with system calls (`sendto`, `recvmsg`, `setsockopt`, …). See **[KERNEL-NETWORKING.md](KERNEL-NETWORKING.md)** for a full explanation of that user/kernel split.

`ft_ping` uses one **raw ICMP socket** for the whole session. All of that setup lives in `srcs/socket.c`. Sending and receiving on the same fd happen in `send.c`, `recv.c`, and `main.c` (`ping_loop`).

Related: [ICMP.md](ICMP.md), [IPv4.md](IPv4.md), [ROUTING.md](ROUTING.md), [TTL.md](TTL.md), [TOS.md](TOS.md), [RECV.md](RECV.md), [FLAGS.md](../FLAGS.md).

---

## Normal socket vs raw socket

`ping` must build and inspect **ICMP** directly — see [KERNEL-NETWORKING.md](KERNEL-NETWORKING.md) for how TCP/UDP sockets hide headers and why raw ICMP is different. Summary:

| | TCP/UDP socket | Raw ICMP socket (`ft_ping`) |
|--|----------------|----------------------------|
| API | `socket(AF_INET, SOCK_STREAM/DGRAM, …)` | `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)` |
| On **send** | Payload is passed; kernel adds headers | **ICMP bytes** are passed; kernel adds **IP header** |
| On **receive** | Payload only | **Full IP datagram** (IP + ICMP) |
| Privilege | Usually none | **Root** / `CAP_NET_RAW` |
| Who builds ICMP | Kernel (for normal ping utility internally) | **Application code** in `send_ping()` |

```
  ft_ping process                         kernel                         network
  ───────────────                         ──────                         ───────

  send_ping() ──ICMP bytes──► sendto() ──► adds IP hdr ──► route ──► target
  recv_ping() ◄── IP+ICMP ─── recvmsg() ◄── demux ICMP ◄── replies/errors
```

The file descriptor is stored in `ping->sockfd` inside `t_ping`. Every probe shares that one socket until `cleanup()` calls `close()`.

---

## Lifecycle in the project

Sockets are created once at startup, used for the whole ping session, then closed on exit.

```
parse_args()          # fills ping->ttl, ping->tos, flags, destination
       │
       ▼
create_socket()       # socket() + set_sock_options()     ← socket.c
       │
       ▼
set_ip_timestamp()    # only if --ip-timestamp              ← socket.c
       │
       ▼
setuid()              # drop root; fd stays valid
       │
       ▼
ping_loop()           # select → sendto / recvmsg on sockfd
       │
       ▼
cleanup()             # close(sockfd)
```

**Why open the socket before `setuid`:** creating a raw socket needs root. After `socket()` succeeds, `main()` calls `setuid(getuid())` so the long-running loop does not keep superuser privileges — but the already-open fd remains usable.

**Why options are set in `socket.c`:** TTL, TOS, routing, and IP options are **per-socket** kernel settings. They must be applied before the first `sendto()`, using values parsed from the command line into `t_ping`.

---

## `create_socket()` — opening the socket

```c
ping->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

| Argument | Value | Meaning |
|--------|-------|---------|
| `domain` | `AF_INET` | IPv4 only |
| `type` | `SOCK_RAW` | Application supplies next-layer header (ICMP) |
| `protocol` | `IPPROTO_ICMP` | Only ICMP traffic is delivered to this socket |

On success, `set_sock_options(ping)` runs. If any option fails, the fd is **closed** and `sockfd` is set to `-1` so the caller does not leak a half-configured socket.

Failure without root:

```
ft_ping: socket: Operation not permitted
```

---

## `set_sock_options()` — kernel configuration

`setsockopt(sockfd, level, option, &value, len)` changes how the kernel handles traffic on this socket. `socket.c` sets:

### `SO_BROADCAST` (always)

Generic socket flag. Unicast ping does not need broadcast, but inetutils `ping` enables it — `ft_ping` does the same for reference compatibility.

### `IP_TTL` (always)

Sets the **Time To Live** byte on **outgoing** IPv4 headers. Value comes from `ping->ttl` (default **64**, flag `--ttl N`). Routers decrement TTL; at zero they drop the packet and may send ICMP *Time Exceeded*. See [TTL.md](TTL.md).

### `IP_TOS` (if `-T` was passed)

Sets the **Type of Service** byte when `ping->tos >= 0`. If `-T` was not used, `tos` stays `-1` and this option is skipped. See [TOS.md](TOS.md).

### `SO_DONTROUTE` (if `-r`)

When global `g_dontroute` is set in `main.c` for flag `-r`, the kernel must **not** use the routing table — only directly connected destinations. Works for `127.0.0.1`; remote hosts often fail.

### `SO_RCVTIMEO` (always, 1 second)

Limits how long `recvmsg()` may block waiting for data. The main loop also uses `select()` with a **10 ms** timeout so packets can be sent on schedule; `SO_RCVTIMEO` is an extra safety net so receive never hangs indefinitely.

| Option | CLI flag | Stored in |
|--------|----------|-----------|
| `IP_TTL` | `--ttl` | `ping->ttl` |
| `IP_TOS` | `-T` | `ping->tos` |
| `SO_DONTROUTE` | `-r` | `g_dontroute` (global in `main.c`) |

---

## `set_ip_timestamp()` — IP options on send

When the user passes `--ip-timestamp tsonly` or `tsaddr`, `main()` calls this **after** `create_socket()`.

The function builds a binary **IP Timestamp** option ([RFC 791](../rfc/rfc791.txt)) in a 40-byte buffer and passes it to the kernel:

```c
setsockopt(sockfd, IPPROTO_IP, IP_OPTIONS, rspace, length);
```

The kernel attaches that option to every outgoing IP packet on this socket. Routers along the path *may* fill timestamp slots; if replies still carry options, `print_ip_opt()` in `print.c` can print a `TS:` line.

```
  IP option bytes (simplified)
  ┌──────┬────────┬─────────┬──────────┬─────────────────┐
  │ type │ length │ pointer │ flags    │ space for TS    │
  │ 0x44 │ 40/36  │ 5       │ tsonly/  │ (routers write) │
  │      │        │         │ tsaddr   │                 │
  └──────┴────────┴─────────┴──────────┴─────────────────┘
```

Many networks **drop** packets with IP options. Packet loss is normal; the program must not crash.

---

## How the socket is used after setup

Once `ping->sockfd` is ready, the rest of the program treats it as a bidirectional ICMP channel.

### Sending (`send.c`)

```c
sendto(ping->sockfd, packet, pkt_sz, 0,
    (struct sockaddr *)&ping->dest_addr, sizeof(ping->dest_addr));
```

- `packet` = ICMP header (8 bytes) + payload (timestamp + pattern data).
- Destination = `ping->dest_addr` from DNS resolution in `dns.c`.
- The kernel wraps the buffer in an IPv4 header (using TTL, TOS, and IP options from `setsockopt`).

### Receiving (`recv.c`)

```c
recvmsg(ping->sockfd, &msg, 0);
```

- Buffer contains **IP header + ICMP message**.
- Sender address is in `msg.msg_name` (`struct sockaddr_in`).
- See [RECV.md](RECV.md) for parsing and filtering by `ping->ident`.

### Waiting (`main.c`)

```c
select(ping->sockfd + 1, &readfds, NULL, NULL, &tv);  // tv = 10 ms
```

`select` watches whether the socket has incoming data without blocking the send timer for the full second.

---

## Data flow (one round trip)

```
  ping_loop
      │
      ├─ send_ping()
      │     build ICMP Echo Request (type 8)
      │     checksum()
      │     sendto(sockfd) ──────────────► kernel ──► wire
      │
      ├─ select(sockfd, 10ms)
      │
      └─ recv_ping()
            recvmsg(sockfd) ◄──────────── kernel ◄── wire
            parse IP + ICMP
            print_echo_reply() or print_icmp_error()
```

Echo **Reply** (type 0) is accepted only if ICMP **id** matches `ping->ident` (`getpid() & 0xFFFF`). Other ICMP types may be shown as errors. See [ICMP-IDENTIFIER.md](ICMP-IDENTIFIER.md).

---

## Functions in `socket.c` (summary)

| Function | Role |
|----------|------|
| `create_socket` | `socket(SOCK_RAW)` + `set_sock_options`; returns `-1` on failure |
| `set_sock_options` | `static` helper: broadcast, TTL, TOS, `-r`, receive timeout |
| `set_ip_timestamp` | Optional `IP_OPTIONS` for `--ip-timestamp` |

`g_dontroute` is declared `extern` in `socket.c` and defined in `main.c` when `-r` is parsed — a simple way to pass one flag without extending `t_ping`.

---

## Common errors

| Message | Cause |
|---------|--------|
| `ft_ping: socket: Operation not permitted` | Program not started as root |
| `ft_ping: setsockopt(IP_OPTIONS): …` | Host/kernel rejects IP options |
| `ft_ping: setsockopt(SO_DONTROUTE): …` | `-r` not supported for this target |
| `ft_ping: recvmsg: …` | Rare socket error (not `EAGAIN` / `EINTR`) |

---

## Related docs

| Document | Content |
|----------|---------|
| [ARCHITECTURE.md](../ARCHITECTURE.md) | Module map, `setuid` after socket |
| [COMMAND_FLOW.md](../COMMAND_FLOW.md) | Startup order in `main()` |
| [RECV.md](RECV.md) | Reading on `ping->sockfd` |
| [FLAGS.md](../FLAGS.md) | Flags that affect socket options |
