# Command-line flags (`ft_ping`)

Reference for every command-line option in **ft_ping**. Output and behavior follow **inetutils-2.0** (`ping -V` on Debian). Compare with system `ping` using the table in `docs/TESTING.md`.

For how options are **parsed** (`getopt_long`, `struct option`, `OPT_TTL`), see [GETOPT-LONG.md](GETOPT-LONG.md). For **`optarg`** and `parse_number`, see [OPTARG.md](OPTARG.md).

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

Prints a short usage summary and exits immediately. This path does **not** open a raw socket, so you can run `./ft_ping -?` or `./ft_ping --help` without `sudo`. Use it to check supported options during development or on the evaluation VM. Any unknown option (for example `-z`) prints `invalid option` and a non-zero exit code instead of help.

| | |
|---|---|
| **Purpose** | Print usage without needing root |
| **Syntax** | `./ft_ping -?` or `./ft_ping --help` |
| **Behavior** | Print option list and exit with status **0** |
| **Implementation** | `print_usage()` in `main.c`; handled before socket creation |
| **Example** | `./ft_ping -?` |

---

### `-v` (verbose)

**Required by the subject.** Verbose mode changes how much the program tells you about the network beyond a simple echo reply line. In normal mode you only see successful replies; many ICMP error packets from routers are silently ignored if they are not about your target. With `-v`, the header line also shows the ICMP **identifier** (useful when several ping processes run at once), and when something goes wrong you get human-readable error text plus an **`IP Hdr Dump:`** block that decodes the quoted inner IP packet from the error.

The classic classroom test is `--ttl 1` toward a remote host: without `-v` you may see little; with `-v` you see *Time to live exceeded* and the inner probe that triggered it. See [ICMP.md](concepts/ICMP.md) and [TTL.md](concepts/TTL.md).

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

Limits how long the session runs by counting **successful unique replies**, not merely sends. After `(num_recv - num_rept) >= count`, the loop stops and statistics are printed. Duplicate replies marked `(DUP!)` do not advance the counter — only the first reply for each sequence number counts. If the last probes are still in flight, `-W` controls how long to keep listening before giving up. Without `-c`, ping runs until `Ctrl+C` or `-w`.

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

Controls the **ICMP data** portion only (not the 8-byte ICMP header). The default of 56 bytes is the historic Unix ping size and produces the familiar `64 bytes from …` reply line. The first 16 bytes of payload (when size allows) store a `struct timeval` send timestamp; the remote host echoes it back unchanged so RTT can be computed. With `-s 0` there is no payload, no timestamp, and reply lines show `8 bytes` without `time=… ms`. Large values approach the IPv4 maximum payload (`65507` bytes of ICMP data).

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

Sets a **wall-clock** limit on the entire session, measured from `start_time` in whole seconds. Unlike `-c`, which stops after a number of replies, `-w` stops when the clock runs out regardless of how many packets were sent or received. Useful for quick smoke tests (`-w 2 8.8.8.8`) or scripts that must not hang. Can be combined with `-c`: whichever condition is met first ends the loop.

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

Only matters when `-c` is set. After all `count` probes have been **sent**, the program would otherwise exit immediately; `-W` keeps the receive loop alive for up to `linger` seconds so late replies still get counted and printed. The default of 10 seconds matches inetutils. Internally, once the send quota is reached, `ping_loop()` switches to a finishing state and stretches `interval` to `linger` microseconds between loop iterations while it keeps calling `recv_ping()`.

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

Sets the IPv4 **Time To Live** on every outgoing probe via `setsockopt(IP_TTL)`. TTL is decremented by each router; at zero the packet is dropped and the router often returns ICMP *Time to live exceeded*. This is how `traceroute` maps hops — ping normally uses 64 so packets reach the target. `--ttl 1` to a remote address is a standard diagnostic: you should see an error from the first gateway, not from the destination. On Debian inetutils the same option is **`-t`**; `ft_ping` uses the long form `--ttl` from the subject.

See [TTL.md](concepts/TTL.md) and [IPv4.md](concepts/IPv4.md).

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

Sets the IPv4 **Type of Service** byte (today often interpreted as DSCP/ECN) on outgoing packets. Unlike TTL, TOS is optional: if you omit `-T`, `ft_ping` does not call `setsockopt(IP_TOS)` and the kernel uses its default (usually 0). Many networks ignore or rewrite TOS, so `-T 0` and `-T 16` often behave identically — the flag is still required for inetutils parity. Value `16` historically meant “minimize delay” (D bit set).

See [TOS.md](concepts/TOS.md).

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

Sends probes as fast as the loop allows (~100 packets per second with a 10 ms interval) instead of one per second. Output is minimal: a dot per send and a backspace per reply instead of full reply lines — designed for load testing on localhost, not for reading RTT on each packet. Requires root like any raw ICMP use. Combine with `-c` to cap how many flood packets are sent.

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

Bursts the first `preload` packets **without waiting** between them at the start of `ping_loop()`, then continues at the normal interval (1 s or flood). Useful to stress the socket or simulate a burst of traffic before steady pacing. Often tested together with `-c` so the preload count matches the total send count (everything fires at once, then the program waits for replies).

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

Fills the ICMP payload (after the embedded timestamp) with a repeating **hexadecimal** pattern you provide, instead of the default `0x00, 0x01, 0x02, …`. Handy for spotting corruption on the wire or matching inetutils pattern tests. Up to 16 bytes are decoded from the argument; an odd number of hex digits uses the last nibble alone. Invalid characters produce `error in pattern near '…'` and exit non-zero before any packet is sent.

| | |
|---|---|
| **Purpose** | Fill ICMP payload (after timestamp) with a repeating **hex** pattern |
| **Syntax** | `-p <hexdigits>` (up to **16** bytes decoded; odd length allowed — last nibble alone) |
| **Default** | bytes `0x00, 0x01, 0x02, …` if `-p` omitted |
| **Errors** | Non-hex digit → `error in pattern near '…'` and exit non-zero |
| **Implementation** | `decode_pattern()` in `utils.c`, `init_data_buffer()` |
| **Examples** | `-p ff -s 56 -c 1 127.0.0.1`; invalid: `./ft_ping -p zz 127.0.0.1` |

#### How `decode_pattern()` reads the hex string

One **byte** in the packet is built from **one or two** hex characters on the command line:

| Hex chars | Meaning |
|-----------|---------|
| two (`de`) | full byte: first char = high nibble, second = low nibble |
| one (`a`) | high nibble only; low nibble forced to `0` → byte `0xA0` |

The function walks the string left to right with pointer `arg`. Each loop iteration produces one byte in `pattern[]`.

**Core line** (`srcs/utils.c`):

```c
pattern[i] = (unsigned char)((high << 4) | low);   /* two digits */
pattern[i] = (unsigned char)(high << 4);           /* one digit */
```

A byte is two 4-bit halves:

```
byte = [ high (4 bits) | low (4 bits) ]
```

- `high << 4` — puts `high` into the **left** half (same as `high × 16`).
- `| low` — fills the **right** half.

##### Example 1: `-p de` (two characters → one byte)

String in memory:

```
index:   0    1    2
char:   'd'  'e'  '\0'
```

| Step | Code | Result |
|------|------|--------|
| read first char | `high = hex_digit('d')` | `high = 13` |
| advance | `arg++` | `arg` points at `'e'` |
| second char exists | `if (*arg)` | true |
| read second char | `low = hex_digit('e')` | `low = 14` |
| merge | `(13 << 4) \| 14` | see below |

Binary merge:

```
high = 13  →  1101
high << 4  →  1101 0000   (= 208)

low = 14   →  1110

  1101 0000
| 0000 1110
  ─────────
  1101 1110  = 222 = 0xDE
```

`pattern[0] = 0xDE`, `pattern_len = 1`.

##### Example 2: `-p a` (one character → one byte)

```
index:   0    1
char:   'a'  '\0'
```

| Step | Result |
|------|--------|
| `high = hex_digit('a')` | `10` |
| `arg++` → `*arg` is `'\0'` | no second digit |
| `pattern[0] = high << 4` | `10 << 4` = `160` = **`0xA0`** |

Single digit → `0xA0`, not `0x0A`.

##### Example 3: `-p dead` (four characters → two bytes)

```
d e | a d
───   ───
DE    AD
```

| Iteration | Reads | high | low | Byte |
|-----------|-------|------|-----|------|
| 1 | `d`, `e` | 13 | 14 | `0xDE` |
| 2 | `a`, `d` | 10 | 13 | `0xAD` |

`pattern = [0xDE, 0xAD]`, `pattern_len = 2`.

##### Example 4: `-p ff`

| | |
|---|---|
| `high` | `15` (`'f'`) |
| `low` | `15` (`'f'`) |
| byte | `(15 << 4) \| 15` = **`0xFF`** |

##### Example 5: `-p deadbeef` (eight characters → four bytes)

| Pair | Byte |
|------|------|
| `de` | `0xDE` |
| `ad` | `0xAD` |
| `be` | `0xBE` |
| `ef` | `0xEF` |

##### After decoding: fill the payload

`decode_pattern()` only builds the **template** (max 16 bytes). `init_data_buffer()` in `srcs/send.c` repeats it across the full `-s` size:

```c
ping->data_buffer[i] = ping->pattern[i % ping->pattern_len];
```

Example: `-p deadbeef -s 56` → 4-byte template repeated 14 times to fill 56 data bytes.

| `-p` input | Invalid |
|------------|---------|
| hex digits `0-9`, `a-f`, `A-F` | `zz` → `error in pattern near '…'` |

#### Why `MAXPATTERN` is 16

`MAXPATTERN` (`includes/ft_ping.h`) caps how many bytes are decoded from the `-p` argument into `ping->pattern[]`:

```c
# define MAXPATTERN  16

unsigned char pattern[MAXPATTERN];   /* t_ping */
```

This is **not** the ICMP payload size (`-s`). It is only the **template** length.

| Concept | Flag / field | Typical size |
|---------|--------------|--------------|
| Template from `-p` | `pattern[]`, `pattern_len` | up to **16 bytes** |
| Full ICMP data | `-s`, `data_length` | default **56** bytes |

After decoding, `init_data_buffer()` **repeats** the template to fill the whole payload:

```
-p deadbeef  →  template: DE AD BE EF  (4 bytes)
-s 56        →  DE AD BE EF DE AD BE EF …  (56 bytes total)
```

So you do not type 56 bytes of hex on the command line — a short pattern is enough.

**Why 16 specifically:**

| Reason | Detail |
|--------|--------|
| **inetutils compatibility** | System `ping` (inetutils) uses the same 16-byte cap; `ft_ping` matches it for defense/tests |
| **Practical** | A repeating template does not need to be as long as the payload |
| **Simple storage** | Fixed `pattern[16]` inside `t_ping` — no extra `malloc` for the template |

In `decode_pattern()`, the loop stops at 16 bytes; extra hex digits in the argument are **silently ignored**:

```c
while (*arg && i < MAXPATTERN)
```

Maximum hex input: **32 characters** (= 16 bytes × 2 nibbles each).

#### All cases: what fills the payload

Payload size is always **`-s`** (default 56 data bytes). The pattern only affects **data after the embedded timestamp** (first 8 or 16 bytes of payload when size allows). It does **not** change the ICMP header.

| Command | `pattern_set` | Payload fill (repeated to `-s` size) |
|---------|---------------|-------------------------------------|
| no `-p` | `false` | `0x00, 0x01, 0x02, 0x03, …` (byte index) |
| `-p ff` | `true` | `0xFF, 0xFF, 0xFF, …` |
| `-p deadbeef` | `true` | `0xDE, 0xAD, 0xBE, 0xEF, …` |
| `-p` with no argument | — | **parse error** (`p:` in optstring requires a value) |
| `-p zz` | — | **error** `error in pattern near '…'` |

**Important:** “`-p` omitted” and “`-p` without hex” are **not** the same. Only omitting the flag entirely selects the `0x00, 0x01, 0x02, …` fill. Writing `-p` alone does not fall back to the default — `getopt_long` expects an argument.

#### What you see on screen

`-p` changes **packet bytes**, not the reply **line format**:

| Mode | Terminal output |
|------|-----------------|
| normal (with or without `-p`) | `64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.123 ms` |
| with `-f` (flood) | `.` on send, `\b` on reply — no per-packet lines |

The hex pattern is inside the ICMP data; it is echoed back in the reply but is not printed as hex in the default output.

#### Default fill: why `0x00, 0x01, 0x02, …` when `-p` is omitted

At startup, `init_ping()` sets `pattern_set = false`. `init_data_buffer()` (`srcs/send.c`) then uses:

```c
ping->data_buffer[i] = (unsigned char)(i & 0xFF);
```

| Payload byte index `i` | Value |
|------------------------|-------|
| 0 | `0x00` |
| 1 | `0x01` |
| 2 | `0x02` |
| … | … |
| 255 | `0xFF` |
| 256 | `0x00` again (`i & 0xFF` wraps) |

**Why this default:**

| Reason | Detail |
|--------|--------|
| **inetutils compatibility** | System `ping` uses the same index-based fill when `-p` is not given |
| **No extra flag needed** | Payload is still non-empty and deterministic |
| **Easy corruption check** | If byte 5 in the reply is not `0x05`, something altered the data |
| **Any `-s`** | One formula works for any payload length without typing hex |

With `-p`, `pattern_set = true` and the buffer is filled from `pattern[]` instead:

```c
ping->data_buffer[i] = ping->pattern[i % ping->pattern_len];
```

---

### `-n` (numeric)

In inetutils, `-n` disables DNS lookups in **reply** lines so you always see numeric IPs. In **ft_ping**, reply lines already use `inet_ntoa()` and never call reverse DNS, so `-n` does not change output — the flag is **accepted** for command-line compatibility when comparing with system `ping`. Forward DNS still runs once at startup if you pass a hostname as the destination.

| | |
|---|---|
| **Purpose** | Numeric output — do not resolve hostnames in **reply** lines |
| **Syntax** | `-n` |
| **Behavior (inetutils)** | Target may still be a hostname (forward DNS for sending); replies show IPs only |
| **This project** | Replies always use `inet_ntoa()` (no reverse DNS). Flag is **accepted** for inetutils compatibility |
| **Example** | `sudo ./ft_ping -n -c 1 google.com` |

#### Why replies are already numeric: `inet_ntoa()`

Each echo reply line is built in `print_echo_reply()` (`srcs/print.c`). The source address comes from the **socket address of the received packet**, not from a DNS lookup:

```c
printf("%d bytes from %s: icmp_seq=%u",
    datalen,
    inet_ntoa(((struct sockaddr_in *)msg->msg_name)->sin_addr),
    ntohs(ICMP_HDR_SEQ(icmp_hdr)));
```

| Piece | Role |
|-------|------|
| `msg->msg_name` | `struct sockaddr_in` filled by `recvmsg()` — who sent the reply |
| `sin_addr` | IPv4 address in binary (network byte order) |
| `inet_ntoa()` | Converts that address to a dotted string (`"8.8.8.8"`) for `printf` |

`inet_ntoa()` only formats an IP that is **already in the packet metadata**. It does **not** call `gethostbyaddr`, `getnameinfo`, or any reverse DNS. So every reply line is numeric by construction — there is no code path that would print `dns.google` instead of `8.8.8.8`.

That is different from the **startup banner**, which still shows the hostname you typed:

```c
printf("PING %s (%s): %zu data bytes", ping->hostname, ping->ip_str, ...);
```

Forward DNS in `resolve_host()` (`getaddrinfo`) runs once to turn `google.com` into `ping->ip_str`; that is unrelated to `-n`.

| Output line | Address shown | DNS involved |
|-------------|---------------|--------------|
| `PING google.com (142.250.…)` | hostname + IP from forward lookup | yes, once at start |
| `64 bytes from 142.250.…: icmp_seq=0` | IP via `inet_ntoa()` | no |

Because of this, `handle_option()` treats `-n` as a no-op (`else if (opt == 'n') ;`) — inetutils accepts the flag, and output already matches numeric reply behavior.

---

### `-r` (bypass routing)

Sets `SO_DONTROUTE` on the socket so packets are sent **without consulting the normal routing table** — typically only works for directly connected networks or loopback. `127.0.0.1` succeeds; a remote host usually fails with a send or network error, but the program must exit cleanly (no crash). Useful to verify the flag is wired to `setsockopt` on the evaluation VM.

| | |
|---|---|
| **Purpose** | Send packets **without** normal routing (`SO_DONTROUTE`) |
| **Syntax** | `-r` |
| **Behavior** | Works for `127.0.0.1`; remote hosts may fail with a clean error (no crash) |
| **Implementation** | `g_dontroute` → `setsockopt(SO_DONTROUTE)` in `socket.c` |
| **Example** | `sudo ./ft_ping -r -c 1 127.0.0.1` |

---

### `--ip-timestamp <flag>`

Attaches an IPv4 **timestamp option** (RFC 791) to each outgoing probe — separate from ICMP and from the `time=… ms` RTT in normal output. Routers that support it can record time (and optionally their address) as the packet passes; if the reply includes IP options, `ft_ping` prints a `TS:` block (and `RR:` for record-route if present). Many modern networks **strip or drop** packets with IP options, so loss with this flag is normal; the program must handle that without crashing. Accepted values: `tsonly` (timestamps only) and `tsaddr` (timestamp + address per hop). Anything else → `unsupported timestamp type`.

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

Exactly **one** target is required after all options: an IPv4 dotted-quad (`127.0.0.1`, `8.8.8.8`) or a hostname (`google.com`). `getaddrinfo()` resolves names to IPv4 only (`AF_INET`). The **header line keeps the string you typed** (`PING google.com (142.250.…)`) while probes use the resolved address. Missing operand, unknown host, or two hostnames are hard errors with a message and non-zero exit.

| | |
|---|---|
| **Purpose** | Single IPv4 address or hostname (FQDN) |
| **Resolution** | `getaddrinfo()` with `AF_INET` only |
| **Header** | Keeps the **string you typed**: `PING google.com (142.250.…)` |
| **Errors** | Missing host → `missing host operand`; unknown → `unknown host`; two hosts → `only one host allowed` |

---

## Exit status

The process exits **0** after help or if at least one echo reply was received; **non-zero** if no replies arrived or if parsing, pattern, socket, or host resolution failed. Scripts can use `$?` after `ft_ping -c 1 host` as a simple reachability check.

| Condition | Exit code |
|-----------|-----------|
| Help (`-?`) | `0` |
| At least one reply received | `0` |
| No replies received | non-zero (`1`) |
| Parse / socket / pattern errors | non-zero |
