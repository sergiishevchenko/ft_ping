# `ft_ping` architecture and implementation notes

This document explains **how the program is structured**, **what it does at runtime**, and **why certain design choices were made** to match the reference behavior (inetutils-2.0) and the project constraints.

`ft_ping` implements the classic ping workflow:
- build ICMP **Echo Request** probes
- send them periodically over IPv4 using a **raw socket**
- receive **Echo Replies** and ICMP **error messages**
- compute RTT and print a statistics summary on exit

Two practical constraints influence almost every part of the code:

- **Raw sockets require privileges**: the program must start with root privileges to create `SOCK_RAW` / `IPPROTO_ICMP`.
- **Networks are imperfect**: packets may be dropped, duplicated, reordered, or filtered (especially when using IP options).

## File layout (what lives where)

The code is split by responsibility:

- **`srcs/main.c`**: argument parsing (`getopt_long`), initialization, loop scheduling, stop conditions, cleanup
- **`srcs/dns.c`**: IPv4 destination resolution (`getaddrinfo`)
- **`srcs/socket.c`**: raw socket creation + `setsockopt` configuration (+ IP timestamp option)
- **`srcs/send.c`**: packet construction (ICMP header, payload, checksum) + `sendto()`
- **`srcs/recv.c`**: `recvmsg()` + IP/ICMP parsing + dispatch
- **`srcs/print.c`**: output formatting (echo replies, ICMP errors, verbose dumps, IP options parsing)
- **`srcs/stats.c`**: header + final summary (loss + min/avg/max/stddev)
- **`srcs/checksum.c`**: RFC 1071 checksum (one’s complement sum)
- **`srcs/signal.c`**: SIGINT handler (Ctrl+C)
- **`srcs/utils.c`**: strict numeric parsing + hex pattern decode (`-p`)

## Runtime flow (step-by-step)

### 1) Argument parsing

Entry point is `main()` in `srcs/main.c`.

`parse_args()` uses `getopt_long()` and supports:
- Mandatory: `-v`, `-?` / `--help`
- Bonus: `-c`, `-f`, `-l`, `-n`, `-p`, `-r`, `-s`, `-T`, `-w`, `-W`, `--ttl`, `--ip-timestamp`

Key behavior:
- `-?` / `--help` works **without root** (parsing happens before raw socket creation).
- Exactly one destination operand is allowed.

### 2) Resolve destination to IPv4 (no reverse DNS later)

`resolve_host()` in `srcs/dns.c`:
- calls `getaddrinfo()` with `AF_INET`
- stores the sockaddr in `ping->dest_addr`
- stores the numeric string in `ping->ip_str` (`inet_ntop`)
- stores the original argument in `ping->hostname` (used in the header)

The project requirements include: **FQDN without reverse DNS in the packet return**.
So reply lines always display numeric addresses for packet sources.

### 3) Privileges and socket creation

`ft_ping` uses:

```c
socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

This requires privileges. The program:
- refuses to run without root (prints “Operation not permitted”)
- creates the socket
- then drops privileges via `setuid(getuid())` to avoid staying privileged longer than needed

### 4) Socket options

`set_sock_options()` (`srcs/socket.c`) configures:
- `SO_BROADCAST`
- `IP_TTL` (from `--ttl`, default 64)
- `IP_TOS` (from `-T`, optional)
- `SO_DONTROUTE` (from `-r`)
- `SO_RCVTIMEO` (fixed 1s; the main loop still uses `select()` for responsiveness)

If `--ip-timestamp` is enabled, `set_ip_timestamp()` sets `IP_OPTIONS` to request timestamps.
Many networks will drop such packets; this is expected.

### 5) Payload template (`-p` pattern)

`init_data_buffer()` (`srcs/send.c`) allocates a payload template:
- default: incrementing bytes `00 01 02 ...`
- `-p <hex>`: repeats the decoded bytes across the payload

This buffer is reused for each outgoing probe (only the timestamp region is updated per packet).

### 6) Scheduler / main loop

`ping_loop()` (`srcs/main.c`) is single-threaded:
- sends preload packets (`-l`) quickly
- sends probes periodically (default 1 second, flood mode uses a smaller interval)
- uses `select()` to wait for readable packets with a small timeout (10ms)

Stop conditions:
- Ctrl+C (`SIGINT` sets `g_stop`)
- `-w <timeout>` wall-clock timeout
- `-c <count>`: stop after **count unique replies** (duplicates must not make it stop early)

### 7) Build and send an Echo Request

`send_ping()` (`srcs/send.c`):
- allocates a buffer of `ICMP header (8) + payload`
- fills ICMP fields (type/code/id/seq)
- writes a `struct timeval` into the payload (when payload is large enough)
- copies the remaining bytes from `data_buffer`
- computes checksum over the whole ICMP message
- sends via `sendto()`

### 8) Receive and classify incoming packets

`recv_ping()` (`srcs/recv.c`):
- reads with `recvmsg()`
- parses the outer IPv4 header to find ICMP header offset
- dispatches:
  - `ICMP_ECHOREPLY` with matching id → echo reply printing + stats
  - other ICMP types → ICMP error printing (and optionally verbose dump)

### 9) Echo replies: RTT, duplicates, IP options

`print_echo_reply()` (`srcs/print.c`):
- computes RTT from the embedded timestamp (`gettimeofday()` delta)
- updates min/avg/max/stddev accumulators
- prints the reply line in inetutils format

Duplicate detection:
- a bitset tracks seen sequences (`recv_table`)
- duplicates:
  - print ` (DUP!)`
  - increment `num_rept`
  - still increment `num_recv`, but `-c` uses `num_recv - num_rept` for “unique”

IP options:
- if options exist on the received packet, `print_ip_opt()` parses and prints:
  - Timestamp (`TS:`)
  - Record Route (`RR:`)
  - `NOP` and unknown options (for visibility / compatibility)

### 10) ICMP errors and inetutils-2.0 behavior

`print_icmp_error()` (`srcs/print.c`) handles errors like:
- Destination Unreachable
- Time Exceeded
- Redirect
- Parameter Problem

Reference behavior (inetutils-2.0):
- Errors about **your own probes to the current target** should be visible even without `-v`.
- Verbose mode (`-v`) enables extra dumps (`IP Hdr Dump` and inner protocol info).

To match that:
- without `-v`, the code checks the **quoted (inner) IP header** and prints only if the destination matches the current target
- with `-v`, it prints and dumps the inner packet

### 11) Exit and statistics

On exit (`SIGINT`, `-w`, `-c` completion), `print_statistics()` (`srcs/stats.c`) prints:
- transmitted / received / packet loss
- `+N duplicates` when duplicates occurred
- RTT summary `min/avg/max/stddev` when timing data exists

Stddev is computed from sums in `calc_stddev()` (`srcs/utils.c`).

## Cross-platform notes (Linux / macOS)

Linux and macOS use different ICMP structures:
- Linux: `struct icmphdr`
- macOS: `struct icmp`

To keep the implementation portable:
- `includes/ft_ping.h` defines `t_icmphdr` and accessor macros
  (`ICMP_HDR_TYPE`, `ICMP_HDR_CODE`, `ICMP_HDR_ID`, `ICMP_HDR_SEQ`, `ICMP_HDR_CKSUM`)

## Testing

For a practical checklist (mandatory + bonus flags + negative cases), see:
- `docs/TESTING.md`
