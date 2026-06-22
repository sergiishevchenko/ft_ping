# TOS (Type of Service)

**Type of Service (TOS)** is a single byte in the IPv4 header that historically carried **quality-of-service hints**: which packets should be preferred under congestion, and which path or queue to use. In **ft_ping**, TOS is optional and controlled with the **`-T`** flag.

For the full IPv4 header layout, see [IPv4.md](IPv4.md).

---

## Where TOS lives: IPv4 header

TOS is the third byte of the IPv4 header (same diagram as in [TTL.md](TTL.md)):

```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|      Fragment Offset    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |         Header Checksum       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

On the wire this byte is often called:

| Era | Name | Notes |
|-----|------|-------|
| Classic IPv4 | **TOS** | 8 bits: precedence + flags |
| Later RFCs | **DS field** (Differentiated Services) | 6-bit DSCP + 2-bit ECN |
| Linux socket API | `IP_TOS` | `setsockopt` name kept for compatibility |

`ft_ping` sets the raw byte via `IP_TOS`; it does not interpret DSCP or ECN separately.

---

## Classic TOS layout (historical)

Originally ([RFC 791](../rfc/rfc791.txt)), the byte was structured as:

```
  0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
|   Precedence    |D|T|R| M | R |
+---+---+---+---+---+---+---+---+
  3 bits           1 1 1 1   2 bits
                   | | | |
                   | | | +-- Minimize monetary cost (rare)
                   | | +---- Minimize delay
                   | +------ High throughput
                   +-------- High reliability
```

**Precedence** (bits 7–5): rough priority class (routine, priority, immediate, …).

**D/T/R/M**: request low delay, high throughput, high reliability, or low cost.

### Common decimal values (still used in tests)

| `-T` value | Binary (typical reading) | Traditional meaning |
|------------|--------------------------|---------------------|
| **0** | `00000000` | Normal service |
| **16** | `00010000` | Minimize delay (D bit) |
| **8** | `00001000` | Maximize throughput (T bit) |
| **4** | `00000100` | Maximize reliability (R bit) |

Modern networks often **ignore or rewrite** this byte. Setting TOS in `ping` is mainly for **testing** that the socket option works and observing whether middleboxes preserve it.

---

## Differentiated Services (DSCP) today

Many routers today treat the byte as:

```
  0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
|         DSCP          |  ECN  |
+---+---+---+---+---+---+---+---+
  6 bits                  2 bits
```

- **DSCP** (Differentiated Services Code Point): per-hop behavior (EF, AF classes, etc.).
- **ECN** (Explicit Congestion Notification): marks congestion without dropping.

`ft_ping` accepts any value **0–255** for `-T` and passes it unchanged to the kernel. It does not validate DSCP classes.

---

## How TOS affects routing (theory)

Routers *may* use TOS/DSCP to:

- Choose a different route (policy routing).
- Place packets in different queues (QoS).
- Reject traffic when no path supports the requested TOS — ICMP **Destination Unreachable** codes **11** and **12** mention “At This TOS” (`print.c` strings: *Destination Network/Host Unreachable At This TOS*).

In practice, on lab networks and the public Internet, probes with `-T 0` vs `-T 16` often behave identically.

---

## TOS in ft_ping

### Default: unset

```c
ping->tos = -1;   /* init_ping() in srcs/main.c */
```

`-1` means **do not** call `setsockopt(IP_TOS)`. The kernel uses its default TOS (usually 0).

### CLI: `-T <tos>`

| Item | Detail |
|------|--------|
| Flag | `-T <N>` |
| Range | `0` … `255` (validated by `parse_number()`) |
| inetutils | Same `-T` on Debian `ping` |

Example:

```bash
sudo ./ft_ping -T 16 -c 1 127.0.0.1
```

### Applying TOS to the socket

In `set_sock_options()` (`srcs/socket.c`):

```c
if (ping->tos >= 0)
    setsockopt(ping->sockfd, IPPROTO_IP, IP_TOS, &ping->tos, sizeof(ping->tos));
```

Like TTL, TOS is applied by the **kernel** on each outgoing IP header. User code only sets the socket option once at startup.

### TOS in received packets

**Echo replies** do not print TOS on the normal one-line output. You only see TOS when:

- **Verbose ICMP errors** (`-v`): `print_ip_header_dump()` prints `TOS` in the decoded header line:

  ```
  Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src     Dst     Data
   4  5  00  ...
  ```

- **ICMP Redirect** messages may refer to “Type of Service” in their text (`get_redirect_str()` codes 2 and 3).

There is no `-T` echo on successful pings; verifying TOS usually requires `tcpdump` or similar on the wire.

### Relation to ICMP “At This TOS” errors

If a network cannot reach a host at the requested TOS, you might see ICMP type **3** (Destination Unreachable) with code **11** or **12**. `ft_ping` prints the corresponding strings from `get_dest_unreach_str()` in `srcs/print.c`. These are rare in normal testing.

---

## TOS vs other “type of service” mentions in the project

| Topic | Meaning |
|-------|---------|
| **`-T` flag** | IPv4 header TOS byte on **outgoing** probes |
| **ICMP Redirect code 2/3** | Router redirect based on TOS + network/host |
| **ICMP Unreachable code 11/12** | No route to destination **for this TOS** |

Do not confuse TOS with:

- **ICMP type** (Echo, Unreachable, …) — see [ICMP.md](ICMP.md).
- **IP options** (`--ip-timestamp`) — separate IP option bytes after the fixed header.

---

## Typical tests

| Command | Expected |
|---------|----------|
| `sudo ./ft_ping -T 0 -c 1 127.0.0.1` | Runs without error; normal reply |
| `sudo ./ft_ping -T 16 -c 1 127.0.0.1` | Same; TOS may be ignored by stack/network |

No change in reply line format is required for a correct implementation.
