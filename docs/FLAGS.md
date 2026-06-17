# Command-line flags (`ft_ping`)

Reference for every command-line option in **ft_ping**. Output and behavior follow **inetutils-2.0** (`ping -V` on Debian). Compare with system `ping` using the table in `docs/TESTING.md`.

## General rules

| Rule | Detail |
|------|--------|
| Target | Exactly **one** hostname or IPv4 address (positional argument) |
| Protocol | **IPv4** ICMP Echo only (`SOCK_RAW`, `IPPROTO_ICMP`) |
| Privileges | Raw socket needs **root** (`sudo ./ft_ping …`) |
| Reply lines | Show the **source IP** — **no reverse DNS** on incoming packets |
| Default probe rate | One packet per **second** (`interval = 1 000 000` µs) |
| Default payload | **56** data bytes (ICMP header 8 → **64 bytes** in reply line) |
| ICMP id | `getpid() & 0xFFFF` |
| Stop | `Ctrl+C` (SIGINT) or flags `-c`, `-w`; then print statistics |
| Reference binary | inetutils `ping` on the Debian VM |

## Flag overview

| Flag | Part | Argument | Default | inetutils `ping` |
|------|---------|----------|---------|------------------|
| `-?` / `--help` | mandatory (help) | — | — | `-?` / `--help` |
| `-v` | **mandatory** | — | off | `-v` |
| `-c` | bonus | count | unlimited | `-c` |
| `-s` | bonus | bytes | 56 | `-s` |
| `-w` | bonus | seconds | off | `-w` |
| `-W` | bonus | seconds | 10 | `-W` |
| `--ttl` | bonus | 0–255 | 64 | `-t` |
| `-T` | bonus | 0–255 | unset | `-T` |
| `-f` | bonus | — | off | `-f` |
| `-l` | bonus | count | 0 | `-l` |
| `-p` | bonus | hex | auto pattern | `-p` |
| `-n` | bonus | — | off | `-n` |
| `-r` | bonus | — | off | `-r` |
| `--ip-timestamp` | bonus | `tsonly` / `tsaddr` | off | `--ip-timestamp` |

---

## Mandatory flags

### `-?` and `--help`

| | |
|---|---|
| **Purpose** | Print usage without needing root |
| **Syntax** | `./ft_ping -?` or `./ft_ping --help` |
| **Behavior** | Print option list and exit with status **0** |
| **Implementation** | `print_usage()` in `main.c`; handled before socket creation |
| **Example** | `./ft_ping -?` |

Invalid unknown options print `invalid option` and exit non-zero.

---

### `-v` (verbose)

| | |
|---|---|
| **Purpose** | **Required.** Show ICMP problems related to **your** probes and extra diagnostic detail |
| **Syntax** | `-v` |
| **Behavior** | |
| | • Header adds ICMP echo **id**: `PING host (ip): N data bytes, id 0xHHHH = NNNN` |
| | • **ICMP errors** (TTL exceeded, unreachable, etc.) about packets **to the target** are always shown |
| | • Without `-v`, unrelated ICMP errors (different inner destination) are **filtered out** |
| | • On errors, prints **`IP Hdr Dump:`** and inner ICMP info |
| **Typical test** | `sudo ./ft_ping --ttl 1 -c 3 8.8.8.8` and `sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8` |
| **Implementation** | `OPT_VERBOSE` in `print.c` (`print_header`, `print_icmp_error`) and `stats.c` |
| **Example** | `sudo ./ft_ping -v -c 2 127.0.0.1` |

---

## Bonus flags

### `-c <count>`

| | |
|---|---|
| **Purpose** | Stop after receiving `<count>` **unique** echo replies |
| **Syntax** | `-c <N>` (`N` ≥ 0) |
| **Default** | `0` = ping until `Ctrl+C` or `-w` |
| **Behavior** | |
| | • Counts **unique** replies: `(num_recv - num_rept) >= count` |
| | • Duplicates (`(DUP!)`) do **not** count toward the limit |
| | • After sending `count` probes, waits up to **`-W`** seconds for late replies |
| | • Then prints statistics and exits |
| **Output** | Normal reply lines; final statistics block |
| **Implementation** | `ping->count` in `main.c` (`ping_loop`) |
| **Example** | `sudo ./ft_ping -c 3 127.0.0.1` |

---

### `-s <size>` (payload size)

| | |
|---|---|
| **Purpose** | Set ICMP **data** size in bytes (not including 8-byte ICMP header) |
| **Syntax** | `-s <N>` (`0` … `65507`) |
| **Default** | `56` |
| **Behavior** | |
| | • Header: `PING …: <N> data bytes` |
| | • Reply line `bytes` = **8 + N** (ICMP header + payload) |
| | • If `N >= 16`, first 16 bytes of payload hold a `timeval` for RTT |
| | • If `N == 0`, no payload and reply line has **no** `time=… ms` |
| **Implementation** | `data_length`, `init_data_buffer()`, `send_ping()` |
| **Examples** | `-s 0 -c 1 127.0.0.1` → `8 bytes`; `-s 1000` → `1008 bytes` |

---

### `-w <timeout>` (deadline)

| | |
|---|---|
| **Purpose** | Stop after `<timeout>` **wall-clock** seconds from start |
| **Syntax** | `-w <N>` |
| **Default** | disabled (`timeout = -1`) |
| **Behavior** | Main loop exits when `elapsed_sec >= N`; statistics printed |
| **Note** | Compares **whole seconds** only (`tv_sec`), like inetutils for this project |
| **Implementation** | `timeout_reached()` in `main.c` |
| **Example** | `sudo ./ft_ping -w 2 8.8.8.8` |

---

### `-W <linger>`

| | |
|---|---|
| **Purpose** | After all `-c` packets are **sent**, keep listening for replies up to `<linger>` seconds |
| **Syntax** | `-W <N>` |
| **Default** | `10` |
| **Behavior** | When send quota is reached, `interval` switches to `linger * 1_000_000` µs until replies arrive or linger expires |
| **Use with** | `-c` (meaningless without a send limit) |
| **Implementation** | `finishing` state in `ping_loop()` |
| **Example** | `sudo ./ft_ping -c 2 -W 3 8.8.8.8` |

---

### `--ttl <N>`

| | |
|---|---|
| **Purpose** | Set IP **Time To Live** on outgoing packets |
| **Syntax** | `--ttl <N>` (`0` … `255`) |
| **Default** | `64` |
| **inetutils** | `-t <N>` |
| **Behavior** | `setsockopt(IP_TTL)`. `--ttl 1` to a remote host usually triggers **Time to live exceeded** from the first router |
| **Implementation** | `ping->ttl` → `set_sock_options()` in `socket.c` |
| **Examples** | `sudo ./ft_ping --ttl 1 -c 3 8.8.8.8`; `sudo ./ft_ping --ttl 64 -c 1 8.8.8.8` |

---

### `-T <tos>` (Type of Service)

| | |
|---|---|
| **Purpose** | Set IPv4 **TOS** byte on outgoing packets |
| **Syntax** | `-T <N>` (`0` … `255`) |
| **Default** | not set (kernel default) |
| **Behavior** | `setsockopt(IP_TOS)` when `-T` is given; many networks ignore or rewrite TOS |
| **Implementation** | `ping->tos` in `socket.c` |
| **Example** | `sudo ./ft_ping -T 16 -c 1 127.0.0.1` |

---

### `-f` (flood)

| | |
|---|---|
| **Purpose** | Send probes as fast as possible (privileged flood mode) |
| **Syntax** | `-f` |
| **Default** | off; interval **1 s** |
| **With `-f`** | interval **10 ms**; prints `.` on send, `\b` on reply — **no** per-packet lines |
| **Implementation** | `OPT_FLOOD`, `PING_FLOOD_INTERVAL` in `main.c` / `print.c` |
| **Example** | `sudo ./ft_ping -f -c 100 127.0.0.1` |

---

### `-l <preload>`

| | |
|---|---|
| **Purpose** | Send the first `<preload>` packets **back-to-back** before normal timing |
| **Syntax** | `-l <N>` |
| **Default** | `0` |
| **Behavior** | At loop start, calls `send_ping()` `preload` times with no delay, then continues at normal `interval` |
| **Implementation** | `ping_loop()` preload loop in `main.c` |
| **Example** | `sudo ./ft_ping -l 10 -c 10 127.0.0.1` |

---

### `-p <hex>` (pattern)

| | |
|---|---|
| **Purpose** | Fill ICMP payload (after timestamp) with a repeating **hex** pattern |
| **Syntax** | `-p <hexdigits>` (up to **16** bytes decoded; odd length allowed — last nibble alone) |
| **Default** | bytes `0x00, 0x01, 0x02, …` if `-p` omitted |
| **Errors** | Non-hex digit → `error in pattern near '…'` and exit non-zero |
| **Implementation** | `decode_pattern()` in `utils.c`, `init_data_buffer()` |
| **Examples** | `-p ff -s 56 -c 1 127.0.0.1`; invalid: `./ft_ping -p zz 127.0.0.1` |

---

### `-n` (numeric)

| | |
|---|---|
| **Purpose** | Numeric output — do not resolve hostnames in **reply** lines |
| **Syntax** | `-n` |
| **Behavior (inetutils)** | Target may still be a hostname (forward DNS for sending); replies show IPs only |
| **This project** | Replies always use `inet_ntoa()` (no reverse DNS). Flag is **accepted** for inetutils compatibility |
| **Example** | `sudo ./ft_ping -n -c 1 google.com` |

---

### `-r` (bypass routing)

| | |
|---|---|
| **Purpose** | Send packets **without** normal routing (`SO_DONTROUTE`) |
| **Syntax** | `-r` |
| **Behavior** | Works for `127.0.0.1`; remote hosts may fail with a clean error (no crash) |
| **Implementation** | `g_dontroute` → `setsockopt(SO_DONTROUTE)` in `socket.c` |
| **Example** | `sudo ./ft_ping -r -c 1 127.0.0.1` |

---

### `--ip-timestamp <flag>`

| | |
|---|---|
| **Purpose** | Add IP **timestamp** option to outgoing packets |
| **Syntax** | `--ip-timestamp tsonly` or `--ip-timestamp tsaddr` |
| **Values** | |
| | • `tsonly` — timestamps only |
| | • `tsaddr` — timestamp + address (inetutils `tsaddr`) |
| **Behavior** | `setsockopt(IP_OPTIONS)`. If replies include options, prints `TS:` (and `RR:` if present) |
| **Network** | Many routers/firewalls **drop** IP options → packet loss is normal; program must not crash |
| **Invalid value** | `unsupported timestamp type` → exit non-zero |
| **Implementation** | `set_ip_timestamp()` in `socket.c`, `print_ip_opt()` in `print.c` |
| **Examples** | `sudo ./ft_ping --ip-timestamp tsonly -c 1 8.8.8.8` |

---

## Positional argument: `<destination>`

| | |
|---|---|
| **Purpose** | Single IPv4 address or hostname (FQDN) |
| **Resolution** | `getaddrinfo()` with `AF_INET` only |
| **Header** | Keeps the **string you typed**: `PING google.com (142.250.…)` |
| **Errors** | Missing host → `missing host operand`; unknown → `unknown host`; two hosts → `only one host allowed` |

---

## Exit status

| Condition | Exit code |
|-----------|-----------|
| Help (`-?`) | `0` |
| At least one reply received | `0` |
| No replies received | non-zero (`1`) |
| Parse / socket / pattern errors | non-zero |
