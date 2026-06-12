# ft_ping

A from-scratch implementation of the `ping` command in C, using raw ICMP sockets. Follows the output format of **inetutils-2.0** (`ping -V`).

## VM setup (Debian)

This project is expected to be run and defended on a **Debian VM (>= 7.0)** with Linux kernel **> 3.14**.

See **`docs/VM_SETUP.md`** for a full guide: creating the VM, packages, networking, SSH from a 42 cluster machine, build/run, and defense checklist.

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

## Testing

See **`docs/TESTING.md`** for the full checklist (mandatory, output format, bonus, negative tests).

To compare `ft_ping` with system `ping`, open two terminals and run the paired commands from the table at the top of `docs/TESTING.md`. Each row has `ft_ping`, the matching `ping` command (inetutils on Debian), and the expected result. On the VM, check `ping -V` — that is the reference binary.

```bash
sudo ./ft_ping -c 3 127.0.0.1    # terminal 1
ping -c 3 127.0.0.1              # terminal 2
```

On macOS, `/sbin/ping` is BSD ping: use `-t` for TTL instead of `--ttl`; some inetutils flags (`-w`, `-n`, `-T`, `--ip-timestamp`) are not available.
