# ft_ping

A from-scratch implementation of the `ping` command in C, using raw ICMP sockets. Follows the output format of **inetutils-2.0** (`ping -V`).

## Build

```bash
make        # build
make clean  # remove object files
make fclean # remove object files and binary
make re     # full rebuild
```

Requires `gcc` and `make`. Compiles with `-Wall -Wextra -Werror`.

## Usage

```bash
sudo ./ft_ping [options] <destination>
```

Root privileges are required (raw sockets).

### Options

| Flag | Description |
|------|-------------|
| `-v` | Verbose output (show ICMP errors, packet id in header) |
| `-?` | Display help |
| `-c <N>` | Stop after sending N packets |
| `-s <N>` | Set payload size in bytes (default: 56) |
| `-w <N>` | Stop after N seconds |
| `-W <N>` | Seconds to wait for each response |
| `--ttl <N>` | Set IP Time To Live |
| `-T <N>` | Set Type of Service (0-255) |
| `-f` | Flood ping (root only) |
| `-l <N>` | Send N packets as fast as possible before normal mode |
| `-p <hex>` | Fill payload with hex pattern |
| `-n` | Numeric output only |
| `-r` | Bypass routing tables (SO_DONTROUTE) |
| `--ip-timestamp <FLAG>` | IP timestamp option: `tsonly` or `tsaddr` |

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

## How it works

1. Creates a raw ICMP socket (`SOCK_RAW`, `IPPROTO_ICMP`)
2. Resolves hostname via `getaddrinfo()`
3. Sends ICMP `ECHO_REQUEST` packets with a timestamp in the payload
4. Receives replies via `select()` + `recvmsg()` loop
5. Calculates round-trip time from the embedded timestamp
6. On `SIGINT`, prints min/avg/max/stddev statistics

## Project structure

```
ft_ping/
├── Makefile
├── includes/
│   └── ft_ping.h      # Types, macros, prototypes, macOS/Linux compat
└── srcs/
    ├── main.c          # Argument parsing (getopt_long), main loop
    ├── socket.c        # Raw socket creation, setsockopt
    ├── dns.c           # DNS resolution (getaddrinfo)
    ├── send.c          # ICMP ECHO_REQUEST packet construction
    ├── recv.c          # ICMP response parsing and dispatch
    ├── print.c         # Output formatting, ICMP error display, IP options
    ├── stats.c         # Header and final statistics
    ├── checksum.c      # RFC 1071 Internet checksum
    ├── signal.c        # SIGINT handler
    └── utils.c         # Number parsing, hex pattern decoding
```

## Platform support

Works on both **macOS** and **Linux** (Debian 7+, kernel > 3.14). Platform differences in ICMP structures are handled via a compatibility layer in `ft_ping.h`.
