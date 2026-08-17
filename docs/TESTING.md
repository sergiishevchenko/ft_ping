# Testing `ft_ping`

This document is a **manual test checklist** for the `ft_ping` project.

Flag semantics: **`docs/FLAGS.md`**.

Per-command **code flow** (functions, stop conditions, flag → field mapping): **`docs/COMMAND_FLOW.md`**.

Oral evaluation cheat sheet: **[Defense commands](#defense-commands)**.

## Prerequisites

- **Root privileges** are required (raw ICMP sockets):

```bash
sudo -v
```

- Build:

```bash
make re
```

- Useful targets:
  - `127.0.0.1` (always reachable)
  - `8.8.8.8` / `1.1.1.1` (public IPs, if allowed)
  - A router or LAN host (low latency)

## Comparing with system `ping`

Open two terminals and run the same row from the table below. The `ft_ping` column matches **Mandatory tests**, **Output format checks**, and **Bonus flags tests**; `ping` is the inetutils equivalent (`ping -V` on Debian VM). On macOS use `-t` instead of `--ttl` (already shown in the table).

Flag details (mandatory vs bonus, behavior, defaults): **`FLAGS.md`**.

**Automated diff testing:** see **[DIFF_TESTS.md](DIFF_TESTS.md)** for ready-to-paste diff commands and `diff_tests.sh` for a full automated suite.

RTT values will differ; compare line **format**, not exact milliseconds. See [DIFF_TESTS.md](DIFF_TESTS.md) § *Expected differences* for what the evaluator ignores.

| Test | What it does | `ft_ping` | `ping` | Expected |
|------|--------------|-----------|--------|----------|
| **Mandatory** | | | | |
| 1) Help (`-?`) | Print usage and exit (no root needed) | `./ft_ping -?` | `ping -?` | Exits OK; prints usage |
| 1) Help (`--help`) | Same as `-?` (long form) | `./ft_ping --help` | `ping --help` | Exits OK; prints usage |
| 2) Basic IPv4 (Ctrl+C) | Continuous ping until interrupted | `sudo ./ft_ping 127.0.0.1` | `ping 127.0.0.1` | `PING 127.0.0.1 (127.0.0.1): 56 data bytes`; `64 bytes from ... icmp_seq=0 ...`; Ctrl+C → statistics |
| 3) Hostname / FQDN | Resolve hostname to IPv4, then ping | `sudo ./ft_ping google.com` | `ping google.com` | `PING google.com (x.x.x.x): 56 data bytes`; replies show IP only |
| 4) Verbose (`-v`) | Extra detail: id in header, ICMP errors, `IP Hdr Dump:` | `sudo ./ft_ping -v -c 2 127.0.0.1` | `ping -v -c 2 127.0.0.1` | Header includes `id 0x.... = ....` |
| 5) TTL exceeded | Cap hop count so the packet dies on the first router | `sudo ./ft_ping --ttl 1 -c 3 8.8.8.8` | `ping --ttl 1 -c 3 8.8.8.8` | `Time to live exceeded` from router; 100% loss |
| 5) TTL exceeded (`-v`) | Same TTL test with verbose ICMP/IP dump | `sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8` | `ping -v --ttl 1 -c 3 8.8.8.8` | Same error + `IP Hdr Dump:` block |
| **Output format** | | | | |
| Statistics block | Stop after N replies; print loss and RTT summary | `sudo ./ft_ping -c 3 127.0.0.1` | `ping -c 3 127.0.0.1` | `--- ... ping statistics ---`; packet counts; `round-trip min/avg/max/stddev` |
| **Bonus** | | | | |
| `-c` (count) | Exit after N unique replies | `sudo ./ft_ping -c 1 127.0.0.1` | `ping -c 1 127.0.0.1` | Exits after 1 reply |
| `-s 0` | ICMP payload size = 0 (no room for RTT timestamp) | `sudo ./ft_ping -s 0 -c 1 127.0.0.1` | `ping -s 0 -c 1 127.0.0.1` | `0 data bytes`; reply `8 bytes` (no `time=`) |
| `-s 56` | Default payload size (56 data bytes → 64-byte reply) | `sudo ./ft_ping -s 56 -c 1 127.0.0.1` | `ping -s 56 -c 1 127.0.0.1` | Reply `64 bytes` (8 + 56) |
| `-s 1000` | Large ICMP payload | `sudo ./ft_ping -s 1000 -c 1 127.0.0.1` | `ping -s 1000 -c 1 127.0.0.1` | Reply `1008 bytes` (8 + 1000) |
| `-w` (timeout) | Wall-clock deadline: stop after N seconds | `sudo ./ft_ping -w 2 8.8.8.8` | `ping -w 2 8.8.8.8` | Stops after ~2 s; prints statistics |
| `-W` (linger) | After last send with `-c`, wait up to N s for late replies | `sudo ./ft_ping -c 2 -W 3 8.8.8.8` | `ping -c 2 -W 3 8.8.8.8` | Exits when enough replies arrive (often before `-W` elapses) |
| `-W` (linger, visible) | Same linger, but no replies → wait full window | `time sudo ./ft_ping -c 2 -W 3 192.0.2.1` | `time ping -c 2 -W 3 192.0.2.1` | ~4 s total (no replies); see **`-W` tests** below |
| `--ttl 1` | Set IP TTL to 1 (packet dies after one hop) | `sudo ./ft_ping --ttl 1 -c 1 8.8.8.8` | `ping --ttl 1 -c 1 8.8.8.8` | `Time to live exceeded` |
| `--ttl 64` | Set IP TTL to default-like value (normal reachability) | `sudo ./ft_ping --ttl 64 -c 1 8.8.8.8` | `ping --ttl 64 -c 1 8.8.8.8` | Normal echo reply |
| `-T 0` | Set IP Type of Service (TOS) to 0 | `sudo ./ft_ping -T 0 -c 1 127.0.0.1` | `ping -T 0 -c 1 127.0.0.1` | No error (TOS may be ignored) |
| `-T 16` | Set IP TOS to 16 (low delay / class selector) | `sudo ./ft_ping -T 16 -c 1 127.0.0.1` | `ping -T 16 -c 1 127.0.0.1` | No error (TOS may be ignored) |
| `-p ff` | Fill payload with repeating hex pattern `ff` | `sudo ./ft_ping -p ff -s 56 -c 1 127.0.0.1` | `ping -p ff -s 56 -c 1 127.0.0.1` | No crash |
| `-p` (long hex) | Fill payload with a long hex pattern (up to 16 bytes) | `sudo ./ft_ping -p 00112233445566778899aabbccddeeff -s 56 -c 1 127.0.0.1` | `ping -p 00112233445566778899aabbccddeeff -s 56 -c 1 127.0.0.1` | No crash |
| `-p zz` (invalid) | Reject bad hex in `-p` | `sudo ./ft_ping -p zz 127.0.0.1` | `ping -p zz 127.0.0.1` | Error; exits non-zero |
| `-f` (flood) | Flood mode: fast interval, dots instead of reply lines | `sudo ./ft_ping -f -c 100 127.0.0.1` | `sudo ping -f -c 100 127.0.0.1` | Dots only; no per-packet lines |
| `-l` (preload) | Send first N probes with no inter-packet delay | `sudo ./ft_ping -l 10 -c 10 127.0.0.1` | `ping -l 10 -c 10 127.0.0.1` | First 10 packets sent fast |
| `-r` (bypass routing) | Bypass routing table (`SO_DONTROUTE`); OK on localhost | `sudo ./ft_ping -r -c 1 127.0.0.1` | `ping -r -c 1 127.0.0.1` | OK on localhost |
| `-n` (numeric) | Inetutils parity flag; intentional no-op (output unchanged) | `sudo ./ft_ping -n -c 1 google.com` | `ping -n -c 1 google.com` | Flag accepted (no-op); same output as without `-n`; numeric IP in replies |
| `--ip-timestamp tsonly` | Attach IP Timestamp option (time only); routers may fill `TS:` | `sudo ./ft_ping --ip-timestamp tsonly -c 1 127.0.0.1` | `ping --ip-timestamp tsonly -c 1 127.0.0.1` | `TS:` block or loss; no crash |
| `--ip-timestamp tsaddr` | Same, but each hop may record address + time | `sudo ./ft_ping --ip-timestamp tsaddr -c 1 127.0.0.1` | `ping --ip-timestamp tsaddr -c 1 127.0.0.1` | `TS:` block or loss; no crash |
| **Negative / robustness** | | | | |
| No args | Missing destination must fail | `./ft_ping` | `ping` | Error + usage; non-zero exit |
| Invalid option | Unknown flag must fail | `./ft_ping -Z 127.0.0.1` | `ping -Z 127.0.0.1` | `invalid option`; non-zero exit |
| Unknown host | DNS failure must fail cleanly | `./ft_ping does-not-exist.invalid` | `ping does-not-exist.invalid` | `unknown host`; non-zero exit |
| No permissions | Raw socket without root must fail | `./ft_ping 127.0.0.1` | — | `Operation not permitted`; non-zero exit |
| Multiple hosts | Only one destination allowed | `./ft_ping 127.0.0.1 127.0.0.2` | — | `only one host allowed`; non-zero exit |
| Bad `--ip-timestamp` | Reject unsupported timestamp type | `./ft_ping --ip-timestamp foobar 127.0.0.1` | — | `unsupported timestamp type`; non-zero exit |
| `-r` remote | `SO_DONTROUTE` to remote usually fails; must not crash | `sudo ./ft_ping -r -c 1 8.8.8.8` | `ping -r -c 1 8.8.8.8` | May fail; no crash |
| Unreachable host | No replies → non-zero exit | `sudo ./ft_ping -c 1 -w 2 192.0.2.1` | `ping -c 1 -w 2 192.0.2.1` | Non-zero exit (no replies) |

On Linux (Debian VM), prefix `ping` with `sudo` when raw sockets require root. macOS: `-W` is in milliseconds (`-W 3000` for linger); `-w`, `-n`, `-T`, and `--ip-timestamp` are not available on BSD `ping`.

### Evaluation rules (from the scale)

The evaluator compares output via `diff` with these tolerances:

| Rule | Meaning |
|------|---------|
| ±30 ms on `time=` | RTT values may differ slightly between runs |
| No reverse DNS required | Reply lines may show IP only (no hostname) |
| Last RTT line ignored | `round-trip min/avg/max/stddev` is not compared |
| Ctrl+C stops the program | Evaluator sends SIGINT manually |

Run `sudo bash diff_tests.sh` for a full automated check with these rules applied.

## Defense commands

Ready-to-paste list for the oral evaluation. Run on the **Debian VM** with `sudo` unless noted. Compare format with inetutils `ping` when asked; `time=` may differ.

```bash
# --- Mandatory ---
./ft_ping -?                                    # help; no root; prints usage
./ft_ping --help                                # same as -?
sudo ./ft_ping 127.0.0.1                        # basic ping; Ctrl+C → statistics
sudo ./ft_ping google.com                       # hostname → IPv4; replies show IP only
sudo ./ft_ping -v -c 2 127.0.0.1                # verbose: id in header
sudo ./ft_ping --ttl 1 -c 3 8.8.8.8             # TTL exceeded from first router
sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8          # same + IP Hdr Dump:
sudo ./ft_ping -c 3 127.0.0.1                   # statistics block; 3 unique replies

# --- Bonus flags ---
sudo ./ft_ping -c 1 127.0.0.1                   # stop after 1 unique reply
sudo ./ft_ping -s 0 -c 1 127.0.0.1              # empty payload; 8-byte reply, no time=
sudo ./ft_ping -s 56 -c 1 127.0.0.1             # default size; 64-byte reply
sudo ./ft_ping -s 1000 -c 1 127.0.0.1           # large payload; 1008-byte reply
sudo ./ft_ping -w 2 8.8.8.8                     # wall-clock stop after ~2 s
sudo ./ft_ping -c 2 -W 3 8.8.8.8                # linger after last send (often unused if replies arrive)
time sudo ./ft_ping -c 2 -W 3 192.0.2.1         # no replies → wait linger (~4 s)
sudo ./ft_ping --ttl 64 -c 1 8.8.8.8            # normal TTL; echo reply
sudo ./ft_ping -T 0 -c 1 127.0.0.1              # TOS 0; must not error
sudo ./ft_ping -T 16 -c 1 127.0.0.1             # TOS 16; kernel may ignore it
sudo ./ft_ping -p ff -s 56 -c 1 127.0.0.1       # fill payload with hex pattern ff
sudo ./ft_ping -f -c 100 127.0.0.1              # flood: dots, ~10 ms interval
sudo ./ft_ping -l 10 -c 10 127.0.0.1            # preload: first 10 packets with no delay
sudo ./ft_ping -r -c 1 127.0.0.1                # bypass routing; OK on loopback
sudo ./ft_ping -n -c 1 google.com               # accepted no-op; same output as without -n
sudo ./ft_ping --ip-timestamp tsonly -c 1 127.0.0.1   # IP Timestamp (time only); TS: or loss
sudo ./ft_ping --ip-timestamp tsaddr -c 1 127.0.0.1   # IP Timestamp + address; TS: or loss

# --- Negative / must not crash ---
./ft_ping                                       # missing host → error + usage
./ft_ping -Z 127.0.0.1                          # invalid option
./ft_ping does-not-exist.invalid                # unknown host
./ft_ping 127.0.0.1                             # no root → Operation not permitted
./ft_ping 127.0.0.1 127.0.0.2                   # only one host allowed
./ft_ping --ip-timestamp foobar 127.0.0.1       # unsupported timestamp type
sudo ./ft_ping -p zz 127.0.0.1                  # bad hex pattern
sudo ./ft_ping -r -c 1 8.8.8.8                  # dontroute to remote; may fail, no crash
sudo ./ft_ping -c 1 -w 2 192.0.2.1              # unreachable; non-zero exit
```

## Mandatory tests (must be perfect)

### 1) Help without root

```bash
./ft_ping -?
./ft_ping --help
```

Expected:
- Exits successfully.
- Prints usage and option list.

### 2) Basic IPv4 ping (address)

```bash
sudo ./ft_ping 127.0.0.1
```

Expected:
- Header:
  - `PING 127.0.0.1 (127.0.0.1): 56 data bytes`
- Reply lines like:
  - `64 bytes from 127.0.0.1: icmp_seq=0 ttl=... time=... ms`

Stop with Ctrl+C and check statistics are printed.

### 3) Hostname / FQDN input

```bash
sudo ./ft_ping google.com
```

Expected:
- Header shows the input hostname and the resolved IPv4:
  - `PING google.com (x.x.x.x): 56 data bytes`
- Reply lines print the **source address** (and never do reverse DNS).

### 4) Verbose mode (`-v`)

```bash
sudo ./ft_ping -v -c 2 127.0.0.1
```

Expected:
- Header contains id:
  - `PING ...: 56 data bytes, id 0x.... = ....`

### 5) ICMP errors are displayed (inetutils-2.0 behavior)

This is the required error-path behavior: errors about **local packets to the target** must be printed (and must not crash the program).

#### TTL exceeded (works on most networks)

```bash
sudo ./ft_ping --ttl 1 -c 3 8.8.8.8
```

Expected:
- Expected output includes:
  - `... bytes from <router-ip>: Time to live exceeded`
- With `-v`, it should additionally print the embedded IP header dump/inner protocol info:

```bash
sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8
```

Expected:
- Same error line(s)
- Plus an `IP Hdr Dump:` block after the error line.

## Output format checks (inetutils-2.0)

These verify output shape against inetutils-2.0.

### 1) Statistics block

Run:

```bash
sudo ./ft_ping -c 3 127.0.0.1
```

Expected at the end:
- `--- <host> ping statistics ---`
- `<N> packets transmitted, <N> packets received, <loss>% packet loss`
- RTT summary:
  - `round-trip min/avg/max/stddev = .../.../.../... ms`

### 2) Duplicate replies formatting

Duplicate replies are difficult to reproduce reliably; when they occur:
- Each duplicate reply line must include ` (DUP!)`
- Statistics must include `+<N> duplicates, `
- `-c <count>` must count only **unique** replies (duplicates must not make it stop early).

## Bonus flags tests

### `-c <N>` (count)

```bash
sudo ./ft_ping -c 1 127.0.0.1
```

Expected:
- Exits after 1 successful (non-duplicate) reply.

### `-s <N>` (payload size)

```bash
sudo ./ft_ping -s 0 -c 1 127.0.0.1
sudo ./ft_ping -s 56 -c 1 127.0.0.1
sudo ./ft_ping -s 1000 -c 1 127.0.0.1
```

Expected:
- Works without crash.
- Reply `bytes` value should reflect size (ICMP header + payload).

### `-w <N>` (global timeout)

```bash
sudo ./ft_ping -w 2 8.8.8.8
```

Expected:
- Stops after ~2 seconds and prints statistics.

### `-W <N>` (linger / wait for replies after send quota)

`-W` only works **with `-c`**. It does **not** mean “wait N seconds after all replies arrived”. The loop exits as soon as enough **unique** replies are received; `-W` is only the **maximum** extra listen time after all `-c` packets have been **sent**.

Two exit paths in `ping_loop()` (whichever happens first):

1. **Enough replies** — `(num_recv - num_rept) >= count` → exit immediately.
2. **Linger expired** — all packets sent, `finishing` mode, no more sends, wait up to `-W` seconds → exit.

#### Why `-W 100` on `8.8.8.8` looks like it does nothing

Replies arrive in milliseconds. Path (1) fires before linger matters — total runtime is ~1–2 s, not 100 s. That is **correct**.

```bash
sudo ./ft_ping -c 2 -W 100 8.8.8.8
```

Expected:
- Two reply lines within ~1–2 s.
- Statistics: `2 packets transmitted, 2 packets received`.
- Program exits quickly — **not** after 100 s.

#### Demonstrating `-W` visibly (no replies)

Use **TEST-NET-1** (`192.0.2.1`, RFC 5737): packets can be sent but no host replies. Then only path (2) applies.

```bash
time sudo ./ft_ping -c 2 -W 3 192.0.2.1
time sudo ./ft_ping -c 2 -W 100 192.0.2.1
```

Expected wall time (approximate):

| Command | Total time | Breakdown |
|---------|------------|-----------|
| `-c 2 -W 3` | **~4 s** | ~0 s 1st send + ~1 s 2nd send + **3 s** linger |
| `-c 2 -W 100` | **~101 s** | ~0 s 1st send + ~1 s 2nd send + **100 s** linger |

Statistics for both:
- `2 packets transmitted, 0 packets received, 100% packet loss`
- Exit code non-zero (`1`)

Compare with inetutils on the Debian VM:

```bash
time ping -c 2 -W 3 192.0.2.1
time ping -c 2 -W 100 192.0.2.1
```

`ft_ping` and `ping` should finish within a few seconds of each other (same linger semantics).

#### `-W` without `-c` (no effect)

```bash
sudo ./ft_ping -W 100 127.0.0.1
```

Expected:
- Pings until Ctrl+C — `-W` is ignored (`count == 0`, send branch never enters `finishing`).
- Same behavior as `sudo ./ft_ping 127.0.0.1`.

#### Quick regression on a live host (parity with inetutils)

```bash
sudo ./ft_ping -c 2 -W 3 8.8.8.8
ping -c 2 -W 3 8.8.8.8
```

Expected:
- Both exit after 2 replies (fast).
- Confirms flag is accepted and does not break the happy path.

### `--ttl <N>`

```bash
sudo ./ft_ping --ttl 1 -c 1 8.8.8.8
sudo ./ft_ping --ttl 64 -c 1 8.8.8.8
```

Expected:
- `--ttl 1` likely triggers “Time to live exceeded”.

### `-T <tos>`

```bash
sudo ./ft_ping -T 0 -c 1 127.0.0.1
sudo ./ft_ping -T 16 -c 1 127.0.0.1
```

Expected:
- Works without errors (some networks may ignore/normalize TOS).

### `-p <hex>` (pattern)

```bash
sudo ./ft_ping -p ff -s 56 -c 1 127.0.0.1
sudo ./ft_ping -p 00112233445566778899aabbccddeeff -s 56 -c 1 127.0.0.1
```

Expected:
- Works without crash.
- Pattern is accepted as hex; invalid hex must produce an error and exit non-zero:

```bash
sudo ./ft_ping -p zz 127.0.0.1
```

### `-f` (flood)

```bash
sudo ./ft_ping -f -c 100 127.0.0.1
```

Expected:
- Prints dots quickly.
- Does not print full per-packet lines.

### `-l <preload>` (preload)

```bash
sudo ./ft_ping -l 10 -c 10 127.0.0.1
```

Expected:
- Sends the first `preload` packets quickly, then continues normally (or stops by `-c`).

### `-r` (bypass routing)

```bash
sudo ./ft_ping -r -c 1 127.0.0.1
```

Expected:
- Works on localhost.
- For non-local targets it may fail depending on route requirements; it must fail cleanly (no crash).

### `-n` (numeric only)

```bash
sudo ./ft_ping -n -c 1 google.com
```

Expected:
- Flag is **accepted** (no `invalid option`); `handle_option()` does nothing — no `t_ping` field, no output change.
- Output is **identical** to the same command without `-n`.
- Still resolves the target hostname to send packets (forward DNS).
- Reply lines always show numeric IP (`inet_ntoa()`; reverse DNS is never used).

### `--ip-timestamp <FLAG>`

```bash
sudo ./ft_ping --ip-timestamp tsonly -c 1 127.0.0.1
sudo ./ft_ping --ip-timestamp tsaddr -c 1 127.0.0.1
```

Expected:
- If the network allows IP options, a `TS:` block may appear (and possibly `RR:` if present).
- Many networks drop IP options; in that case, the program may show packet loss, but must not crash.

## Makefile

```bash
make re
```

Expected: clean rebuild, binary `ft_ping` exists.

```bash
make clean
```

Expected: object files removed, binary **kept**.

```bash
make fclean
```

Expected: binary removed.

```bash
make
```

Expected: rebuilds successfully.

## Negative / robustness tests

### 1) Missing host operand

```bash
./ft_ping
```

Expected:
- Prints error + usage and exits non-zero.

### 2) Unknown host

```bash
./ft_ping does-not-exist.invalid
```

Expected:
- Prints `unknown host ...` and exits non-zero.

### 3) No permissions (raw socket)

```bash
./ft_ping 127.0.0.1
```

Expected:
- Prints: `ft_ping: socket: Operation not permitted`
- Exits non-zero.

### 4) Invalid option

```bash
./ft_ping -Z 127.0.0.1
```

Expected:
- Prints `invalid option -- 'Z'` and exits non-zero.

### 5) Multiple hosts

```bash
./ft_ping 127.0.0.1 127.0.0.2
```

Expected:
- Prints `only one host allowed` and exits non-zero.

### 6) Invalid `--ip-timestamp` value

```bash
./ft_ping --ip-timestamp foobar -c 1 127.0.0.1
```

Expected:
- Prints `unsupported timestamp type: foobar` and exits non-zero.

## Exit codes

### Help exits 0

```bash
./ft_ping -?; echo $?
./ft_ping --help; echo $?
```

Expected: exit code `0`.

### Replies received exits 0

```bash
sudo ./ft_ping -c 1 127.0.0.1; echo $?
```

Expected: exit code `0`.

### No replies exits non-zero

```bash
sudo ./ft_ping -c 1 -w 2 192.0.2.1; echo $?
```

Expected: exit code `1` (no replies from TEST-NET-1).

## SIGINT handling

```bash
sudo ./ft_ping 127.0.0.1 &
PID=$!
sleep 2
kill -INT $PID
wait $PID
echo $?
```

Expected:
- Program prints statistics on SIGINT, then exits cleanly.
- Exit code ≤ 128 or exactly 130 (128 + SIGINT).

## Edge cases (bad numeric arguments — no crash)

Each command must **not** crash (no segfault / signal). A clean error message and non-zero exit is OK; timeout (the command taking too long) is also acceptable. The program must never be killed by a signal.

```bash
sudo ./ft_ping -c 0 127.0.0.1         # 0 = unlimited (same as no -c)
sudo ./ft_ping -c -1 127.0.0.1        # negative count
sudo ./ft_ping -c abc 127.0.0.1       # non-numeric count
sudo ./ft_ping -s -1 127.0.0.1        # negative size
sudo ./ft_ping -s 99999 127.0.0.1     # size above 65507
sudo ./ft_ping --ttl 0 127.0.0.1      # TTL 0
sudo ./ft_ping --ttl 999 127.0.0.1    # TTL above 255
sudo ./ft_ping -T 256 127.0.0.1       # TOS above 255
sudo ./ft_ping -w 0 127.0.0.1         # immediate deadline
sudo ./ft_ping -W -1 127.0.0.1        # negative linger
sudo ./ft_ping -l -1 127.0.0.1        # negative preload
```

## `-r` on remote host (clean failure)

```bash
sudo ./ft_ping -r -c 1 8.8.8.8
```

Expected:
- May fail with a send/network error (routing bypassed); must **not** crash.

## Platform notes (Linux/macOS)

- On **Linux (Debian VM)** the project should be tested in the evaluation environment.
- On **macOS** raw sockets also require root. Behavior of some IP options (especially timestamps) can differ due to OS/network restrictions.
