# Testing `ft_ping`

This document is a **manual test checklist** for the `ft_ping` project.

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
  - Your router / LAN host (low latency)

## Comparing with system `ping`

Open two terminals and run the same row from the table below. The `ft_ping` column matches **Mandatory tests**, **Output format checks**, and **Bonus flags tests**; `ping` is the inetutils equivalent (`ping -V` on Debian VM). On macOS use `-t` instead of `--ttl` (already shown in the table).

RTT values will differ; compare line **format**, not exact milliseconds.

| Test | `ft_ping` | `ping` | Expected |
|------|-----------|--------|----------|
| **Mandatory** | | | |
| 1) Help (`-?`) | `./ft_ping -?` | `ping -?` | Exits OK; prints usage |
| 1) Help (`--help`) | `./ft_ping --help` | `ping --help` | Exits OK; prints usage |
| 2) Basic IPv4 (Ctrl+C) | `sudo ./ft_ping 127.0.0.1` | `ping 127.0.0.1` | `PING 127.0.0.1 (127.0.0.1): 56 data bytes`; `64 bytes from ... icmp_seq=0 ...`; Ctrl+C → statistics |
| 3) Hostname / FQDN | `sudo ./ft_ping google.com` | `ping google.com` | `PING google.com (x.x.x.x): 56 data bytes`; replies show IP only |
| 4) Verbose (`-v`) | `sudo ./ft_ping -v -c 2 127.0.0.1` | `ping -v -c 2 127.0.0.1` | Header includes `id 0x.... = ....` |
| 5) TTL exceeded | `sudo ./ft_ping --ttl 1 -c 3 8.8.8.8` | `ping -t 1 -c 3 8.8.8.8` | `Time to live exceeded` from router; 100% loss |
| 5) TTL exceeded (`-v`) | `sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8` | `ping -v -t 1 -c 3 8.8.8.8` | Same error + `IP Hdr Dump:` block |
| **Output format** | | | |
| Statistics block | `sudo ./ft_ping -c 3 127.0.0.1` | `ping -c 3 127.0.0.1` | `--- ... ping statistics ---`; packet counts; `round-trip min/avg/max/stddev` |
| **Bonus** | | | |
| `-c` (count) | `sudo ./ft_ping -c 1 127.0.0.1` | `ping -c 1 127.0.0.1` | Exits after 1 reply |
| `-s 0` | `sudo ./ft_ping -s 0 -c 1 127.0.0.1` | `ping -s 0 -c 1 127.0.0.1` | `0 data bytes`; reply `8 bytes` (no `time=`) |
| `-s 56` | `sudo ./ft_ping -s 56 -c 1 127.0.0.1` | `ping -s 56 -c 1 127.0.0.1` | Reply `64 bytes` (8 + 56) |
| `-s 1000` | `sudo ./ft_ping -s 1000 -c 1 127.0.0.1` | `ping -s 1000 -c 1 127.0.0.1` | Reply `1008 bytes` (8 + 1000) |
| `-w` (timeout) | `sudo ./ft_ping -w 2 8.8.8.8` | `ping -w 2 8.8.8.8` | Stops after ~2 s; prints statistics |
| `-W` (linger) | `sudo ./ft_ping -c 2 -W 3 8.8.8.8` | `ping -c 2 -W 3 8.8.8.8` | Waits up to `-W` s for late replies |
| `--ttl 1` | `sudo ./ft_ping --ttl 1 -c 1 8.8.8.8` | `ping -t 1 -c 1 8.8.8.8` | `Time to live exceeded` |
| `--ttl 64` | `sudo ./ft_ping --ttl 64 -c 1 8.8.8.8` | `ping -t 64 -c 1 8.8.8.8` | Normal echo reply |
| `-T 0` | `sudo ./ft_ping -T 0 -c 1 127.0.0.1` | `ping -T 0 -c 1 127.0.0.1` | No error (TOS may be ignored) |
| `-T 16` | `sudo ./ft_ping -T 16 -c 1 127.0.0.1` | `ping -T 16 -c 1 127.0.0.1` | No error (TOS may be ignored) |
| `-p ff` | `sudo ./ft_ping -p ff -s 56 -c 1 127.0.0.1` | `ping -p ff -s 56 -c 1 127.0.0.1` | No crash |
| `-p` (long hex) | `sudo ./ft_ping -p 00112233445566778899aabbccddeeff -s 56 -c 1 127.0.0.1` | `ping -p 00112233445566778899aabbccddeeff -s 56 -c 1 127.0.0.1` | No crash |
| `-p zz` (invalid) | `sudo ./ft_ping -p zz 127.0.0.1` | `ping -p zz 127.0.0.1` | Error; exits non-zero |
| `-f` (flood) | `sudo ./ft_ping -f -c 100 127.0.0.1` | `sudo ping -f -c 100 127.0.0.1` | Dots only; no per-packet lines |
| `-l` (preload) | `sudo ./ft_ping -l 10 -c 10 127.0.0.1` | `ping -l 10 -c 10 127.0.0.1` | First 10 packets sent fast |
| `-r` (bypass routing) | `sudo ./ft_ping -r -c 1 127.0.0.1` | `ping -r -c 1 127.0.0.1` | OK on localhost |
| `-n` (numeric) | `sudo ./ft_ping -n -c 1 google.com` | `ping -n -c 1 google.com` | Resolves host; numeric IP in replies |
| `--ip-timestamp tsonly` | `sudo ./ft_ping --ip-timestamp tsonly -c 1 8.8.8.8` | `ping --ip-timestamp tsonly -c 1 8.8.8.8` | `TS:` block or loss; no crash |
| `--ip-timestamp tsaddr` | `sudo ./ft_ping --ip-timestamp tsaddr -c 1 8.8.8.8` | `ping --ip-timestamp tsaddr -c 1 8.8.8.8` | `TS:` block or loss; no crash |

On Linux (Debian VM), prefix `ping` with `sudo` when raw sockets require root. macOS: `-W` is in milliseconds (`-W 3000` for linger); `-w`, `-n`, `-T`, and `--ip-timestamp` are not available on BSD `ping`.

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

This is the key “error path” requirement: errors about **your packets to the target** must be printed (and must not crash the program).

#### TTL exceeded (works on most networks)

```bash
sudo ./ft_ping --ttl 1 -c 3 8.8.8.8
```

Expected:
- You should see something like:
  - `... bytes from <router-ip>: Time to live exceeded`
- With `-v`, it should additionally print the embedded IP header dump/inner protocol info:

```bash
sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8
```

Expected:
- Same error line(s)
- Plus an `IP Hdr Dump:` block after the error line.

## Output format checks (inetutils-2.0)

These are quick “shape” checks the evaluators look at.

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

This is hard to reproduce on demand in a stable way, but when duplicates happen:
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

### `-W <N>` (linger / “wait for replies” after sending done)

```bash
sudo ./ft_ping -c 2 -W 3 8.8.8.8
```

Expected:
- After the last send, program may stay a bit to receive late replies (up to `-W` seconds) before printing final statistics.

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
- Still resolves the target hostname to send packets.
- Prints numeric addresses in reply lines (no reverse DNS).

### `--ip-timestamp <FLAG>`

```bash
sudo ./ft_ping --ip-timestamp tsonly -c 1 8.8.8.8
sudo ./ft_ping --ip-timestamp tsaddr -c 1 8.8.8.8
```

Expected:
- If the network allows IP options, you may see a `TS:` block (and possibly `RR:` if present).
- Many networks drop IP options; in that case, the program may show packet loss, but must not crash.

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

## Platform notes (Linux/macOS)

- On **Linux (Debian VM)** the project should be tested in the evaluation environment.
- On **macOS** raw sockets also require root. Behavior of some IP options (especially timestamps) can differ due to OS/network restrictions.
