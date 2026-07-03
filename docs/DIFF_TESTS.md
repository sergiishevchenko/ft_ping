# Diff tests — ready-to-paste commands

Compare `ft_ping` output with inetutils `ping` on the **Debian VM**.

## How to read

Each row is a **single command** you paste into the terminal. It:

1. Runs system `ping` → normalizes output → saves to `/tmp/ref.txt`
2. Runs `./ft_ping` → normalizes output → saves to `/tmp/ft.txt`
3. Runs `diff` and prints differences (empty = pass)

**Normalization** removes what the evaluation ignores:
- `time=…` values replaced with `TIME` (±30 ms tolerance)
- Last line (`round-trip …`) removed (RTT stats ignored)
- `id 0x…` replaced (different PID per process)
- Reverse DNS in error replies stripped (`_gateway (ip)` → `ip`)
- Trailing whitespace stripped

## Prerequisites

```bash
make re
ping -V   # must say "inetutils" (not BSD ping)
```

All commands below assume **root** (`sudo -i` or prefix each with `sudo`).

---

## Helper function

Paste this once per terminal session:

```bash
norm() {
  grep -v '^round-trip' \
  | sed -E 's/time=[0-9.]+ ms/time=TIME ms/' \
  | sed -E 's/id 0x[0-9a-f]+ = [0-9]+/id 0xID = ID/' \
  | sed -E 's/from [a-zA-Z0-9._-]+ \(([0-9.]+)\)/from \1/' \
  | sed 's/[[:space:]]*$//'
}
```

---

## Mandatory tests

| # | Test | Command |
|---|------|---------|
| 1 | Basic IPv4 (`-c 3`) | `ping -c 3 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -c 3 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 2 | Verbose (`-v`) | `ping -v -c 2 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -v -c 2 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 3 | Ctrl+C (4 sec) | `timeout -s INT 4 ping 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; timeout -s INT 4 ./ft_ping 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 4 | Hostname / FQDN | `ping -c 2 google.com 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -c 2 google.com 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 5 | TTL exceeded | `ping --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 6 | TTL exceeded + verbose | `ping -v --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -v --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |

---

## Bonus tests

| # | Test | Command |
|---|------|---------|
| 7 | `-c 1` | `ping -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 8 | `-s 0` | `ping -s 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -s 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 9 | `-s 56` (default) | `ping -s 56 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -s 56 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 10 | `-s 1000` | `ping -s 1000 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -s 1000 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 11 | `-T 0` (TOS) | `ping -T 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -T 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 12 | `-T 16` (TOS) | `ping -T 16 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -T 16 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 13 | `-p ff` (pattern) | `ping -p ff -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -p ff -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 14 | `-r` (dontroute) | `ping -r -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -r -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 15 | `-w 2` (timeout) | `ping -w 2 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -w 2 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 16 | `--ip-timestamp tsonly` | `ping --ip-timestamp tsonly -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ip-timestamp tsonly -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 17 | `--ip-timestamp tsaddr` | `ping --ip-timestamp tsaddr -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ip-timestamp tsaddr -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 18 | `-f -c 50` (flood) | `ping -f -c 50 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -f -c 50 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 19 | `-l 5 -c 5` (preload) | `ping -l 5 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -l 5 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 20 | `--ttl 64` | `ping --ttl 64 -c 1 8.8.8.8 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ttl 64 -c 1 8.8.8.8 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 21 | `-n` (numeric) | `ping -n -c 1 google.com 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -n -c 1 google.com 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |

**Note on `-n`:** in `ft_ping` the flag is an intentional no-op — it is in the `getopt_long` optstring so parsing succeeds, but `handle_option()` has an empty branch and no `t_ping` field is set. The diff test checks that `./ft_ping -n …` runs and matches inetutils output; it does not verify a behavioral toggle (there is none).

---

## Negative tests (error messages)

Error messages do not need to match word-for-word (subject says "handle errors"). Check exit code and that **something** is printed to stderr.

| # | Test | Command |
|---|------|---------|
| 22 | No args | `ping 2>&1 > /tmp/ref.txt; ./ft_ping 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |
| 23 | Unknown host | `ping no.such.host.invalid 2>&1 > /tmp/ref.txt; ./ft_ping no.such.host.invalid 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |
| 24 | Invalid option | `ping -Z 2>&1 > /tmp/ref.txt; ./ft_ping -Z 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |
| 25 | Unreachable host | `ping -c 1 -w 2 192.0.2.1 2>&1 > /tmp/ref.txt; ./ft_ping -c 1 -w 2 192.0.2.1 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |

---

## Automated script

All tests above (plus more) are packaged in **`diff_tests.sh`** at the project root:

```bash
sudo bash diff_tests.sh
```

The script handles additional normalization that `norm()` above does not:
- **Verbose mode:** normalizes `id 0x… = …` and `ICMP: id 0x…, seq 0x…` (different PID per process), and IP header hex dumps / decoded lines (IP ID, checksum differ between packets)
- **Hostname tests:** normalizes resolved IPs and `ttl=` values (DNS load balancing may return different addresses and different TTLs per route)
- **IP timestamp tests:** compares format, not exact entry count (kernel fills different numbers of slots depending on loopback path)
- **Flood mode:** compares only header + statistics lines (dot output is non-deterministic)
- **Network tests:** skipped automatically if `8.8.8.8` is unreachable
- **Ctrl+C:** simulated via `timeout -s INT 4`

---

## Expected differences (not bugs)

| Difference | Why | Acceptable? |
|------------|-----|-------------|
| `time=` values differ | Different packets, different RTT | Yes (±30 ms) |
| `id 0x…` differs | Different PID per process | Yes |
| `_gateway (ip)` vs `ip` | System ping does reverse DNS in errors; ft_ping does not | Yes (subject: DNS in return is NOT mandatory) |
| google.com resolves to different IP | DNS load balancing | Yes |
| IP header dump hex differs | IP ID, checksum unique per packet | Yes |
| `ttl=` differs for hostname tests | Different route per DNS response | Yes |
| `--ip-timestamp` entry count varies | Kernel fills different number of slots per run | Usually yes |
| `round-trip` line values differ | RTT accumulation differs | Yes (line is ignored by evaluator) |

---

## Notes

- Run on **Debian VM** only (inetutils ping). macOS system ping has different output.
- For tests hitting the network (`8.8.8.8`, `google.com`), results depend on connectivity.
- `-p` prints a `PATTERN:` line — both pings should show it identically.
- Flood mode (`-f`) output is non-deterministic (dot count depends on timing). Compare only the statistics line.
- Verbose IP header dump is for information — exact hex values will always differ between two distinct packets.
