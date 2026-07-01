# Diff tests — ready-to-paste commands

Compare `ft_ping` output with inetutils-2.0 `ping` on the **Debian VM**.

## How to read

Each row is a **single command** you paste into the terminal. It:

1. Runs system `ping` → normalizes output → saves to `/tmp/ref.txt`
2. Runs `./ft_ping` → normalizes output → saves to `/tmp/ft.txt`
3. Runs `diff` and prints differences (empty = pass)

**Normalization** removes what the evaluation ignores:
- `time=…` values replaced with `TIME` (±30 ms tolerance)
- Last line (`round-trip …`) removed (RTT stats ignored)
- Trailing whitespace stripped

## Prerequisites

```bash
make re
ping -V   # must say "inetutils 2.0" (not BSD ping)
```

All commands below assume **root** (`sudo -i` or prefix each with `sudo`).

---

## Helper function

Paste this once per terminal session:

```bash
norm() {
  grep -v '^round-trip' | sed -E 's/time=[0-9.]+ ms/time=TIME ms/' | sed 's/[[:space:]]*$//'
}
```

---

## Mandatory tests

| # | Test | Command |
|---|------|---------|
| 1 | Basic IPv4 (`-c 3`) | `ping -c 3 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -c 3 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 2 | Verbose (`-v`) | `ping -v -c 2 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -v -c 2 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 3 | Hostname / FQDN | `ping -c 2 google.com 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -c 2 google.com 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 4 | TTL exceeded | `ping --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 5 | TTL exceeded + verbose | `ping -v --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -v --ttl 1 -c 2 8.8.8.8 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |

---

## Bonus tests

| # | Test | Command |
|---|------|---------|
| 6 | `-c 1` | `ping -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 7 | `-s 0` | `ping -s 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -s 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 8 | `-s 1000` | `ping -s 1000 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -s 1000 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 9 | `--ttl 64` | `ping --ttl 64 -c 1 8.8.8.8 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ttl 64 -c 1 8.8.8.8 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 10 | `-T 0` (TOS) | `ping -T 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -T 0 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 11 | `-T 16` (TOS) | `ping -T 16 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -T 16 -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 12 | `-p ff` (pattern) | `ping -p ff -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -p ff -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 13 | `-f -c 50` (flood) | `ping -f -c 50 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -f -c 50 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 14 | `-l 5 -c 5` (preload) | `ping -l 5 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -l 5 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 15 | `-r` (dontroute) | `ping -r -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -r -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 16 | `-n` (numeric) | `ping -n -c 1 google.com 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -n -c 1 google.com 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 17 | `-w 2` (timeout) | `ping -w 2 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping -w 2 -c 5 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 18 | `--ip-timestamp tsonly` | `ping --ip-timestamp tsonly -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ip-timestamp tsonly -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |
| 19 | `--ip-timestamp tsaddr` | `ping --ip-timestamp tsaddr -c 1 127.0.0.1 2>&1 \| norm > /tmp/ref.txt; ./ft_ping --ip-timestamp tsaddr -c 1 127.0.0.1 2>&1 \| norm > /tmp/ft.txt; diff /tmp/ref.txt /tmp/ft.txt` |

---

## Negative tests (error messages)

Error messages do not need to match word-for-word (subject says "handle errors"). Check exit code and that **something** is printed to stderr.

| # | Test | Command |
|---|------|---------|
| 20 | No args | `ping 2>&1 > /tmp/ref.txt; ./ft_ping 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |
| 21 | Unknown host | `ping no.such.host.invalid 2>&1 > /tmp/ref.txt; ./ft_ping no.such.host.invalid 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |
| 22 | Invalid option | `ping -Z 2>&1 > /tmp/ref.txt; ./ft_ping -Z 2>&1 > /tmp/ft.txt; echo "ref=$?"; echo "ft=$?"` |

---

## Ctrl+C test (manual)

This cannot be fully automated with a one-liner because both processes must receive SIGINT at roughly the same time. Use two terminals or this approach:

```bash
# Terminal 1: reference
ping 127.0.0.1 2>&1 | norm | tee /tmp/ref.txt
# wait 3-4 lines, Ctrl+C

# Terminal 2: ft_ping
./ft_ping 127.0.0.1 2>&1 | norm | tee /tmp/ft.txt
# same number of lines, Ctrl+C

# Then:
diff /tmp/ref.txt /tmp/ft.txt
```

Or automated with a timeout signal:

```bash
timeout -s INT 4 ping 127.0.0.1 2>&1 | norm > /tmp/ref.txt
timeout -s INT 4 ./ft_ping 127.0.0.1 2>&1 | norm > /tmp/ft.txt
diff /tmp/ref.txt /tmp/ft.txt
```

---

## Automated script

All tests above are packaged in **`diff_tests.sh`** at the project root:

```bash
sudo bash diff_tests.sh
```

The script:
- Runs each test from the tables above automatically
- Normalizes output (removes `round-trip`, replaces `time=`)
- Simulates Ctrl+C via `timeout -s INT 4`
- Compares flood mode by statistics only (dots are non-deterministic)
- Skips network-dependent tests if `8.8.8.8` is unreachable
- Prints `[OK]` / `[FAIL]` / `[SKIP]` with a summary at the end

---

## Interpreting diff output

| `diff` shows | Meaning | Action |
|--------------|---------|--------|
| (empty) | Identical after normalization | Pass |
| Only `time=TIME ms` lines differ | Normalization bug (re-check `norm`) | Fix `norm()` regex |
| `< PING host (ip): 56 data bytes` vs `> PING host (ip): 56 data bytes` | Hostname/IP difference | Check `resolve_host` / `print_header` |
| `< 0%` vs `> 0.0%` | Packet loss format | Fix `printf` in `print_statistics` |
| Extra/missing blank lines | Whitespace difference | Check `\n` placement |
| `< ... icmp_seq=1` vs `> ... icmp_seq=0` | Sequence start differs | inetutils starts at 0 |

---

## Notes

- Run on **Debian VM** only (inetutils ping). macOS system ping has different output.
- For tests hitting the network (`8.8.8.8`, `google.com`), results depend on connectivity.
- `-p` prints a `PATTERN:` line — both pings should show it identically.
- Flood mode (`-f`) output is non-deterministic (dot count depends on timing). Compare only the statistics line.
