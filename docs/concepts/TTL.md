# TTL (Time To Live)

TTL is a field in the **IPv4 header**. It limits how many **router hops** a packet may traverse before being discarded. Despite the name “Time To Live”, on modern IPv4 networks TTL is almost always a **hop counter**, not a wall-clock timer.

For all IPv4 header fields, see [IPv4.md](IPv4.md).

---

## Where TTL lives: IPv4 header

TTL is byte **8** of the standard 20-byte IPv4 header (no options):

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|      Fragment Offset    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |         Header Checksum       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Source Address                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination Address                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Size | Range | Role |
|-------|------|-------|------|
| **TTL** | 1 byte | 0–255 | Hop limit; decremented by each forwarding router |

Related fields:

- **Protocol** (`ip_p`) — for ping packets this is `IPPROTO_ICMP` (1).
- **Type of Service** (`ip_tos`) — see [TOS.md](TOS.md).

---

## How TTL works on the wire

1. The **sender** (here: the kernel when `ft_ping` calls `sendto`) sets TTL on the outgoing IP header.
2. Each **router** that forwards the packet decrements TTL by **1**.
3. If TTL reaches **0** before the packet reaches its destination, the router **drops** the packet and usually sends an ICMP **Time Exceeded** (type **11**, code **0**) back to the source.

```
  ft_ping                    Router R1              Destination
     |  TTL=1 (with --ttl 1)      |                      |
     |--------------------------->|                      |
     |                            | TTL becomes 0        |
     |<--- ICMP Time Exceeded ----| (drops packet)       |
     |     (from R1's IP)         |                      |
```

With default TTL **64**, a reply line might show `ttl=118` if the remote host started with 128 and the path back is short — you see the **remaining TTL on the reply**, not your outgoing value.

### TTL vs traceroute

`traceroute` exploits TTL: it sends probes with TTL=1, then 2, then 3, … collecting **Time Exceeded** messages from each hop. `ping` normally uses a high enough TTL that packets reach the target; setting `--ttl 1` is mainly a **diagnostic** to force an error from the first router.

---

## ICMP “Time to live exceeded”

When TTL hits zero in transit:

| ICMP field | Value |
|------------|-------|
| **type** | 11 (`ICMP_TIME_EXCEEDED`) |
| **code** | 0 (TTL exceeded in transit) |

`ft_ping` maps this to the string **"Time to live exceeded"** in `get_time_exceeded_str()` (`srcs/print.c`).

Another code under type 11:

| Code | Meaning |
|------|---------|
| 0 | TTL exceeded in transit |
| 1 | Fragment reassembly time exceeded |

---

## TTL in ft_ping

### Default and CLI

| Setting | Value | Where |
|---------|-------|-------|
| Default TTL | **64** | `PING_DEFAULT_TTL` in `includes/ft_ping.h` |
| Flag | `--ttl <N>` | `0` … `255` |
| inetutils alias | `-t` | macOS / Debian `ping -t` |

Initialization in `init_ping()` (`srcs/main.c`):

```c
ping->ttl = PING_DEFAULT_TTL;
```

Parsing: long option `--ttl` → `ping->ttl = parse_number(optarg, 255, "TTL")`.

### Applying TTL to the socket

Before sending, `set_sock_options()` in `srcs/socket.c` calls:

```c
setsockopt(ping->sockfd, IPPROTO_IP, IP_TTL, &ping->ttl, sizeof(ping->ttl));
```

The kernel copies this value into the **outgoing IP header** for every probe. `ft_ping` does not modify TTL in user space — it only configures the socket.

### Reading TTL on echo replies

On a successful **Echo Reply**, the printed `ttl=` value comes from the **reply packet’s IP header**, not from `ping->ttl`:

```c
printf(" ttl=%d", ip_hdr->ip_ttl);   /* srcs/print.c, print_echo_reply() */
```

That is the **remaining TTL** when the reply arrived at your host. It reflects the peer’s initial TTL minus hops on the return path. It is useful as a rough hint of path length, not as proof of the TTL you sent.

### Verbose mode and TTL errors

`sudo ./ft_ping --ttl 1 -c 3 8.8.8.8` typically yields:

- No echo replies (100% packet loss toward the target).
- ICMP errors from the first hop: `Time to live exceeded`.

With `-v`, `print_icmp_error()` also dumps the **quoted inner IP header**, where you can read the TTL and destination of your original probe:

```
Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src     Dst     Data
 4  5  00  ...                         01  01  ...     ...     ...
```

Here `TTL` in the dump is often `01` when you used `--ttl 1`.

### Filtering unrelated TTL errors

`print_icmp_error()` only displays errors whose **inner quoted packet** was destined for the current target (`inner_ip->ip_dst == ping->dest_addr`), unless `-v` behavior is always-on for matching errors (unrelated destinations are still filtered without `-v`).

---

## Typical tests

| Command | Expected behavior |
|---------|-------------------|
| `sudo ./ft_ping --ttl 64 -c 1 127.0.0.1` | Normal echo reply on loopback |
| `sudo ./ft_ping --ttl 1 -c 3 8.8.8.8` | `Time to live exceeded` from first router; no replies from 8.8.8.8 |
| `sudo ./ft_ping -v --ttl 1 -c 3 8.8.8.8` | Same + `IP Hdr Dump:` and inner ICMP info |

See [TESTING.md](../TESTING.md) for side-by-side comparison with system `ping`.
