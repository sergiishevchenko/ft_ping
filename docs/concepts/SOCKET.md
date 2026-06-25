# Raw sockets and `socket.c`

`socket.c` opens the **raw ICMP socket** and applies kernel-level options before the ping loop runs. Without a successful `create_socket()`, `ft_ping` cannot send or receive probes.

Related pages: [IPv4.md](IPv4.md) (TTL, TOS, IP options), [TTL.md](TTL.md), [TOS.md](TOS.md), [FLAGS.md](../FLAGS.md) (`--ttl`, `-T`, `-r`, `--ip-timestamp`).

---

## Role in the program

```
main()
  ├─ parse_args()           # fills ping->ttl, ping->tos, g_dontroute, ip_ts_type, …
  ├─ create_socket(&ping)   # socket.c — this file
  ├─ set_ip_timestamp()     # socket.c — only if --ip-timestamp
  ├─ setuid(getuid())       # drop root; socket stays open
  └─ ping_loop()            # sendto / recvmsg on ping->sockfd
```

| Function | Visibility | Called from |
|----------|------------|-------------|
| `set_sock_options` | `static` | `create_socket()` only |
| `set_ip_timestamp` | global | `main()` when `OPT_IPTIMESTAMP` |
| `create_socket` | global | `main()` after argument parsing |

---

## Full source (`srcs/socket.c`)

```c
#include "ft_ping.h"

extern int	g_dontroute;

static int	set_sock_options(t_ping *ping)
{
	int				one;
	struct timeval	tv;

	one = 1;
	if (setsockopt(ping->sockfd, SOL_SOCKET, SO_BROADCAST,
			&one, sizeof(one)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(SO_BROADCAST): %s\n",
			strerror(errno));
		return (-1);
	}
	if (setsockopt(ping->sockfd, IPPROTO_IP, IP_TTL,
			&ping->ttl, sizeof(ping->ttl)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(IP_TTL): %s\n",
			strerror(errno));
		return (-1);
	}
	if (ping->tos >= 0)
	{
		if (setsockopt(ping->sockfd, IPPROTO_IP, IP_TOS,
				&ping->tos, sizeof(ping->tos)) < 0)
		{
			fprintf(stderr, "ft_ping: setsockopt(IP_TOS): %s\n",
				strerror(errno));
			return (-1);
		}
	}
	if (g_dontroute)
	{
		one = 1;
		if (setsockopt(ping->sockfd, SOL_SOCKET, SO_DONTROUTE,
				&one, sizeof(one)) < 0)
		{
			fprintf(stderr, "ft_ping: setsockopt(SO_DONTROUTE): %s\n",
				strerror(errno));
			return (-1);
		}
	}
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (setsockopt(ping->sockfd, SOL_SOCKET, SO_RCVTIMEO,
			&tv, sizeof(tv)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(SO_RCVTIMEO): %s\n",
			strerror(errno));
		return (-1);
	}
	return (0);
}

int	set_ip_timestamp(t_ping *ping)
{
	unsigned char	rspace[MAX_IPOPTLEN];
	int				type;

	if (ping->ip_ts_type & SOPT_TSADDR)
		type = IPOPT_TS_TSANDADDR;
	else
		type = IPOPT_TS_TSONLY;
	memset(rspace, 0, sizeof(rspace));
	rspace[0] = IPOPT_TS;
	rspace[1] = sizeof(rspace);
	if (type != IPOPT_TS_TSONLY)
		rspace[1] -= sizeof(uint32_t);
	rspace[2] = 5;
	rspace[3] = type;
	if (setsockopt(ping->sockfd, IPPROTO_IP,
			IP_OPTIONS, rspace, rspace[1]) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(IP_OPTIONS): %s\n",
			strerror(errno));
		return (-1);
	}
	return (0);
}

int	create_socket(t_ping *ping)
{
	ping->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping->sockfd < 0)
	{
		fprintf(stderr, "ft_ping: socket: %s\n", strerror(errno));
		return (-1);
	}
	if (set_sock_options(ping) != 0)
	{
		close(ping->sockfd);
		ping->sockfd = -1;
		return (-1);
	}
	return (0);
}
```

---

## Line-by-line: preamble and `g_dontroute`

| Line | Code | Explanation |
|------|------|-------------|
| 1 | `#include "ft_ping.h"` | Pulls in `t_ping`, socket headers (`<sys/socket.h>`, `<netinet/in.h>`), and constants (`MAX_IPOPTLEN`, `SOPT_*`, `IPOPT_*`). |
| 3 | `extern int g_dontroute;` | **Declaration** of a global defined in `main.c`. Set to `1` when the user passes `-r`. `socket.c` only reads it to decide whether to set `SO_DONTROUTE`. Using a global avoids adding another field to `t_ping` for one flag. |

---

## Line-by-line: `set_sock_options()`

Applies standard socket options immediately after the raw socket is created. Returns `0` on success, `-1` on any `setsockopt` failure.

| Line | Code | Explanation |
|------|------|-------------|
| 5 | `static int set_sock_options(t_ping *ping)` | `static` — not visible outside this file. Takes session state; needs `ping->sockfd`, `ping->ttl`, `ping->tos`. |
| 7 | `int one;` | Integer flag used as “enable” (`1`) for boolean socket options. |
| 8 | `struct timeval tv;` | Holds receive timeout for `SO_RCVTIMEO`. |
| 10 | `one = 1;` | Prepare enable value for options that take `int` on/off. |
| 11–12 | `setsockopt(..., SO_BROADCAST, &one, ...)` | **Level** `SOL_SOCKET` (generic socket). **Option** `SO_BROADCAST` — allow broadcast traffic on this socket. Unicast ping does not need it; inetutils `ping` sets it, so `ft_ping` matches reference behavior. |
| 13–16 | `if (... < 0) { fprintf ... return (-1); }` | `setsockopt` returns `-1` on error; `errno` explains why (permission, unsupported option). Abort socket setup. |
| 18–19 | `setsockopt(..., IPPROTO_IP, IP_TTL, &ping->ttl, ...)` | **Level** `IPPROTO_IP` (IPv4). **Option** `IP_TTL` — default TTL for **outgoing** IP headers built by the kernel. Value from `ping->ttl` (default **64**, `--ttl N`). See [TTL.md](TTL.md). |
| 20–23 | error handling | Same pattern: print `IP_TTL` failure and return `-1`. |
| 25 | `if (ping->tos >= 0)` | TOS is optional. `init_ping()` sets `tos = -1`; `-T N` sets `0…255`. Skip `IP_TOS` when flag not used. |
| 27–28 | `setsockopt(..., IP_TOS, &ping->tos, ...)` | Sets the **Type of Service** byte on outgoing IPv4 packets. See [TOS.md](TOS.md). Many networks ignore or rewrite TOS. |
| 29–33 | error handling | TOS failure is fatal for startup (same as other options). |
| 35 | `if (g_dontroute)` | Only when user passed **`-r`** (bypass routing tables). |
| 37 | `one = 1;` | Reuse `one` (still 1; explicit reassignment before `SO_DONTROUTE`). |
| 38–39 | `setsockopt(..., SO_DONTROUTE, &one, ...)` | **Do not consult the routing table** — send only to directly connected networks. Works for `127.0.0.1`; remote targets often fail cleanly. |
| 40–44 | error handling | `SO_DONTROUTE` failure → message and `-1`. |
| 46–47 | `tv.tv_sec = 1; tv.tv_usec = 0;` | Receive timeout: **1 second**, 0 microseconds. |
| 48–49 | `setsockopt(..., SO_RCVTIMEO, &tv, ...)` | If `recvmsg()` blocks longer than 1 s with no data, it returns with an error (often `EAGAIN`/`EWOULDBLOCK`). The main loop also uses `select()` with 10 ms so sends stay on schedule; this is a **socket-level safety net**. |
| 50–54 | error handling | `SO_RCVTIMEO` failure → `-1`. |
| 55 | `return (0);` | All options applied successfully. |

### `setsockopt` call shape

```c
setsockopt(sockfd, level, option_name, &value, sizeof(value));
```

| Parameter | In `ft_ping` |
|-----------|----------------|
| `sockfd` | `ping->sockfd` from `create_socket()` |
| `level` | `SOL_SOCKET` (generic) or `IPPROTO_IP` (IPv4) |
| `option_name` | `SO_BROADCAST`, `IP_TTL`, … |
| `value` | Pointer to `int` or `struct timeval` |
| `length` | `sizeof` that value |

---

## Line-by-line: `set_ip_timestamp()`

Builds an **IP Timestamp** option ([RFC 791](../rfc/rfc791.txt)) and attaches it to every outgoing packet via `IP_OPTIONS`. Called from `main()` only when `--ip-timestamp tsonly` or `tsaddr` is set.

| Line | Code | Explanation |
|------|------|-------------|
| 58 | `int set_ip_timestamp(t_ping *ping)` | Public function; failure prevents program start. |
| 60 | `unsigned char rspace[MAX_IPOPTLEN];` | Buffer for raw IP option bytes. `MAX_IPOPTLEN` is **40** (`ft_ping.h`) — max IP options space in standard header. |
| 61 | `int type;` | Timestamp sub-type: timestamps only vs timestamp + address. |
| 63–64 | `if (ping->ip_ts_type & SOPT_TSADDR) type = IPOPT_TS_TSANDADDR;` | User chose **`tsaddr`** (`--ip-timestamp tsaddr`). Each router may record time **and** its IP. |
| 65–66 | `else type = IPOPT_TS_TSONLY;` | Default / **`tsonly`** — record timestamps only. |
| 67 | `memset(rspace, 0, sizeof(rspace));` | Zero buffer before filling option layout. |
| 68 | `rspace[0] = IPOPT_TS;` | Option **type** = 68 (`0x44`) — IP Timestamp per RFC 791. |
| 69 | `rspace[1] = sizeof(rspace);` | Option **length** — initially full 40 bytes. |
| 70–71 | `if (type != IPOPT_TS_TSONLY) rspace[1] -= sizeof(uint32_t);` | For **tsandaddr**, inetutils uses a slightly shorter option (reserve 4 bytes less in length field). |
| 72 | `rspace[2] = 5;` | **Pointer** — offset (in 4-byte units from start of option) where the next timestamp slot will be written. Value `5` matches inetutils layout (data starts after 4-byte option header). |
| 73 | `rspace[3] = type;` | Low 4 bits of byte 3 = flag: `0` = tsonly, `1` = tsandaddr (`IPOPT_TS_TSONLY` / `IPOPT_TS_TSANDADDR`). |
| 74–75 | `setsockopt(..., IP_OPTIONS, rspace, rspace[1])` | Pass built option to kernel. Length argument is `rspace[1]` (not always 40 after adjustment). Kernel copies option into outgoing IP headers. |
| 76–79 | error handling | Common failures: option not supported, buffer too large, permission. |
| 81 | `return (0);` | Timestamp option active for this socket. |

### IP Timestamp option layout (first bytes)

```
Byte:  0        1           2          3           4 …
     +--------+-----------+----------+-----------+-----
     | type   | length    | pointer  | OFLW|FLG | data area …
     | 0x44   | 40 or 36  | 5        | type      | (routers fill on path)
     +--------+-----------+----------+-----------+-----
```

On receive, if the reply still carries options, `print_ip_opt()` in `print.c` may print a `TS:` block. Many networks **drop** packets with IP options — expect loss, not a crash.

---

## Line-by-line: `create_socket()`

Entry point: allocate the raw ICMP socket and apply `set_sock_options()`.

| Line | Code | Explanation |
|------|------|-------------|
| 84 | `int create_socket(t_ping *ping)` | Called from `main()` after `parse_args()`; requires root (checked in `main` before this call). |
| 86 | `ping->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);` | Create socket: **IPv4** (`AF_INET`), **raw** (`SOCK_RAW`), protocol **ICMP** (`IPPROTO_ICMP` = 1). Raw socket lets the process read/write ICMP (and see full IP packets on receive). Requires **CAP_NET_RAW** / root. |
| 87–90 | `if (ping->sockfd < 0)` | `socket()` returns `-1` on failure (no permission, kernel limit, etc.). Print `ft_ping: socket: …` and return `-1`. |
| 92 | `if (set_sock_options(ping) != 0)` | Apply TTL, TOS, broadcast, optional `-r`, receive timeout. |
| 93–96 | `close(ping->sockfd); ping->sockfd = -1; return (-1);` | **Cleanup on partial failure** — do not leak fd if `setsockopt` failed. Mark fd invalid so `cleanup()` does not double-close wrongly (caller must not use bad fd). |
| 98 | `return (0);` | Socket ready; `main()` may call `set_ip_timestamp()` next, then `setuid()`. |

### Why `SOCK_RAW` + `IPPROTO_ICMP`

| Socket type | What you send | What you receive |
|-------------|---------------|------------------|
| Normal UDP/TCP | Application payload | Payload only |
| **Raw ICMP** | ICMP message (type 8 + data); kernel adds IP header | **Full IP datagram** (IP header + ICMP) |

`ft_ping` builds the ICMP header in `send.c` and parses IP + ICMP in `recv.c`.

---

## Options summary

| `setsockopt` | Flag / source | Effect |
|--------------|---------------|--------|
| `SO_BROADCAST` | always | Match inetutils; allow broadcast on socket |
| `IP_TTL` | `ping->ttl` (`--ttl`, default 64) | Outgoing hop limit |
| `IP_TOS` | `ping->tos` (`-T`, if ≥ 0) | Outgoing QoS byte |
| `SO_DONTROUTE` | `g_dontroute` (`-r`) | Bypass routing table |
| `SO_RCVTIMEO` | fixed 1 s | Max block time in `recvmsg` |
| `IP_OPTIONS` | `set_ip_timestamp()` (`--ip-timestamp`) | IP Timestamp option on send |

---

## Error messages

| Message | Typical cause |
|---------|----------------|
| `ft_ping: socket: Operation not permitted` | Not run as root |
| `ft_ping: setsockopt(IP_TTL): …` | Invalid TTL value or kernel restriction |
| `ft_ping: setsockopt(IP_OPTIONS): …` | IP options not supported (common on some hosts) |
| `ft_ping: setsockopt(SO_DONTROUTE): …` | `-r` not allowed for this socket/target |
