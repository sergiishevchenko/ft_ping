# ft_ping

A from-scratch implementation of the `ping` command in C, using raw ICMP sockets. Follows the output format of **inetutils-2.0** (`ping -V`).

## VM setup (Debian)

This project is expected to be run and defended on a **Debian VM (>= 7.0)** with Linux kernel **> 3.14**.

Quick start on the VM:

```bash
sudo apt-get update && sudo apt-get install -y build-essential make gcc git
cd ft_ping && make re
sudo ./ft_ping -c 3 127.0.0.1
```

## Build

```bash
make        # build
make clean  # remove object files
make fclean # remove object files and binary
make re     # full rebuild
```

Requires `gcc` and `make`. Compiles with `-Wall -Wextra -Werror` and links with `-lm`.

## Usage

```bash
sudo ./ft_ping [options] <destination>
```

Root privileges are required (raw sockets). After socket creation the binary drops privileges via `setuid(getuid())`.

### Options

| Flag | Part | Description |
|------|---------|-------------|
| `-v` | mandatory | Verbose: id in header, ICMP errors about local packets, `IP Hdr Dump:` |
| `-?` / `--help` | mandatory | Display help (no root needed) |
| `-c <N>` | bonus | Stop after N **unique** replies |
| `-s <N>` | bonus | Payload size in bytes (default 56, max 65507) |
| `-w <N>` | bonus | Stop after N seconds (wall clock) |
| `-W <N>` | bonus | Wait N seconds for replies after last send with `-c` (default 10) |
| `--ttl <N>` | bonus | IP TTL (default 64) |
| `-T <N>` | bonus | IP Type of Service (0-255) |
| `-f` | bonus | Flood ping (dots, 10 ms interval) |
| `-l <N>` | bonus | Send first N packets with no delay |
| `-p <hex>` | bonus | Fill payload with hex pattern (max 16 bytes) |
| `-n` | bonus | Numeric replies (inetutils compatibility) |
| `-r` | bonus | Bypass routing tables (`SO_DONTROUTE`) |
| `--ip-timestamp <FLAG>` | bonus | IP timestamp: `tsonly` or `tsaddr` |

### Examples

```bash
sudo ./ft_ping 8.8.8.8
sudo ./ft_ping -v -c 5 google.com
sudo ./ft_ping --ttl 10 -v 1.1.1.1
sudo ./ft_ping -s 1000 -c 3 127.0.0.1
sudo ./ft_ping -f -c 100 192.168.1.1
```

### Output format

Matches inetutils-2.0:

```
PING google.com (142.250.185.46): 56 data bytes
64 bytes from 142.250.185.46: icmp_seq=0 ttl=118 time=1.234 ms
64 bytes from 142.250.185.46: icmp_seq=1 ttl=118 time=1.456 ms
^C
--- google.com ping statistics ---
2 packets transmitted, 2 packets received, 0% packet loss
round-trip min/avg/max/stddev = 1.234/1.345/1.456/0.111 ms
```

## Project structure

```
ft_ping/
├── Makefile
├── includes/
│   └── ft_ping.h      # types, macros, macOS/Linux ICMP compat layer
└── srcs/
    ├── main.c          # init, argument parsing (getopt_long), ping loop, cleanup
    ├── socket.c        # raw socket, setsockopt (TTL, TOS, SO_DONTROUTE, IP timestamp)
    ├── dns.c           # DNS resolution (getaddrinfo)
    ├── send.c          # ICMP ECHO_REQUEST construction, payload fill
    ├── recv.c          # ICMP response parsing, duplicate detection, dispatch
    ├── print.c         # echo reply / ICMP error formatting, IP options display
    ├── stats.c         # PING header line, final statistics (min/avg/max/stddev)
    ├── checksum.c      # RFC 1071 internet checksum
    ├── signal.c        # SIGINT handler (sets g_stop)
    └── utils.c         # parse_number, decode_pattern (hex payload)
```

## Platform support

Works on both **macOS** and **Linux** (Debian 7+, kernel > 3.14). Platform differences in ICMP structures (`struct icmp` vs `struct icmphdr`) are handled via a compatibility layer with `t_icmphdr` and `ICMP_HDR_*` macros in `ft_ping.h`.
