# Routing table, network interfaces, and gateways

When `ft_ping` calls `sendto()`, the program only supplies a **destination IP address**. Everything that happens next — choosing an interface, picking a next hop, resolving a MAC address, building the Ethernet frame — is done by the **Linux kernel** using two core data structures:

1. **Network interfaces** — the host’s attachment points to networks (`lo`, `eth0`, `wlan0`, …).
2. **The routing table** — rules that map destination addresses to an interface and, when needed, a **gateway** (next-hop router).

This document explains those concepts in depth: what they are, how Linux represents them, how the kernel uses them on every outbound packet, and how `ft_ping` fits in.

Related: [ROUTING.md](ROUTING.md) (hops, TTL, `-r`, ICMP errors), [KERNEL-NETWORKING.md](KERNEL-NETWORKING.md), [SOCKET.md](SOCKET.md), [TTL.md](TTL.md), [IPv4.md](IPv4.md).

---

## Table of contents

1. [Big picture](#big-picture)
2. [Network interfaces](#network-interfaces)
3. [IP addresses on interfaces](#ip-addresses-on-interfaces)
4. [The routing table](#the-routing-table)
5. [Gateways (default and specific)](#gateways-default-and-specific)
6. [How the kernel routes one packet](#how-the-kernel-routes-one-packet)
7. [ARP and the link layer](#arp-and-the-link-layer)
8. [Linux tools and reading output](#linux-tools-and-reading-output)
9. [Common setups and examples](#common-setups-and-examples)
10. [Failure modes and ICMP errors](#failure-modes-and-icmp-errors)
11. [How ft_ping uses routing](#how-ft_ping-uses-routing)

---

## Big picture

A host is rarely directly connected to every machine on the Internet. It is connected to **one or more local networks** through **interfaces**. To reach anything else, traffic is handed to a **router** (gateway) that knows more of the path.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Host (ft_ping)                                                         │
│                                                                         │
│   Application          Kernel                                           │
│   ───────────          ──────                                           │
│   sendto(8.8.8.8) ──►  1. Routing lookup  ──► route: via GW on eth0     │
│                        2. Pick source IP  ──► src from eth0 address     │
│                        3. ARP for GW MAC  ──► Ethernet dst = GW MAC     │
│                        4. Transmit frame on eth0                        │
└───────────────────────────────────────────────┬─────────────────────────┘
                                                │
                                                ▼
                                         Gateway (router)
                                                │
                                                ▼
                                         … more routers …
                                                │
                                                ▼
                                           8.8.8.8
```

| Concept | Question it answers |
|---------|---------------------|
| **Interface** | *Which NIC / virtual port does this host use to talk to a given network?* |
| **Routing table** | *For destination D, which interface and which next hop?* |
| **Gateway** | *Which router on the local link should receive packets for off-link destinations?* |

`ft_ping` never opens `/proc/net/route` or runs `ip route`. It passes the destination to `sendto()`; the kernel performs the lookup.

---

## Network interfaces

### What is a network interface?

A **network interface** is the kernel’s abstraction for a path to a network. It has:

| Property | Role |
|----------|------|
| **Name** | Human label: `lo`, `eth0`, `enp0s3`, `wlan0` |
| **Index** | Numeric ID used internally (`ifindex`) |
| **MAC address** | Layer-2 address on Ethernet/Wi‑Fi (not used on `lo`) |
| **MTU** | Maximum Transmission Unit — largest IP payload per frame (often 1500) |
| **Flags** | `UP`, `RUNNING`, `LOOPBACK`, `BROADCAST`, `MULTICAST`, … |
| **IP addresses** | One or more IPv4/IPv6 prefixes bound to the interface |

Interfaces are listed and configured with `ip link` and `ip addr` (modern) or legacy `ifconfig`.

### Physical vs virtual interfaces

| Type | Examples | Typical use |
|------|----------|-------------|
| **Loopback** | `lo` | Traffic to `127.0.0.0/8` — stays inside the host |
| **Ethernet** | `eth0`, `enp0s3` | Wired LAN, VM NAT/bridged NIC |
| **Wireless** | `wlan0`, `wlp2s0` | Wi‑Fi |
| **Bridge** | `br0`, `docker0` | Virtual switch connecting VMs/containers |
| **Tunnel / VPN** | `tun0`, `wg0` | Encapsulated traffic over another interface |

On the 42 evaluation **Debian VM**, you usually see `lo` and one Ethernet interface (`eth0` or similar) with an address like `10.0.2.15/24`.

### Interface state: UP and RUNNING

- **`UP`** — administratively enabled (`ip link set eth0 up`). Required for the kernel to use the interface for routing.
- **`RUNNING`** — driver reports the link is active (cable plugged in, Wi‑Fi associated). A interface can be `UP` but not `RUNNING` (no link).

If no suitable interface is `UP` with a route to the destination, `sendto()` fails (e.g. `ENETUNREACH`, `EHOSTUNREACH`).

### Example: `ip link show`

```bash
ip link show eth0
```

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
    link/ether 08:00:27:aa:bb:cc brd ff:ff:ff:ff:ff:ff
```

| Field | Meaning |
|-------|---------|
| `eth0` | Interface name |
| `UP,LOWER_UP` | Admin up; link layer up |
| `mtu 1500` | Max IP packet size on this link (before fragmentation at L2) |
| `link/ether …` | MAC address — used as **source** in outbound Ethernet frames |
| `brd ff:ff:ff:ff:ff:ff` | Broadcast MAC — ARP and local subnet broadcast |

---

## IP addresses on interfaces

### Address + prefix length

An interface is configured with addresses such as:

```
10.0.2.15/24
```

| Part | Meaning |
|------|---------|
| `10.0.2.15` | Host’s IP on that network |
| `/24` | Network mask `255.255.255.0` — hosts `10.0.2.0`–`10.0.2.255` share the link |

The **same interface** can hold multiple addresses (secondary IPs). The kernel picks a **source address** for outbound packets based on the chosen route (often the `src` field on the route or the primary address on that interface).

### Example: `ip addr show`

```bash
ip addr show eth0
```

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether 08:00:27:aa:bb:cc brd ff:ff:ff:ff:ff:ff
    inet 10.0.2.15/24 brd 10.0.2.255 scope global dynamic eth0
       valid_lft forever preferred_lft forever
    inet6 fe80::a00:27ff:feaa:bbcc/64 scope link
```

| Field | Meaning |
|-------|---------|
| `inet 10.0.2.15/24` | IPv4 address and prefix |
| `brd 10.0.2.255` | Subnet broadcast address |
| `scope global` | Address is valid beyond the local link (normal LAN/internet-facing) |
| `dynamic` | Often from DHCP |
| `inet6 fe80::…/64 scope link` | Link-local IPv6 (out of scope for `ft_ping`, which uses IPv4) |

### Loopback interface

```bash
ip addr show lo
```

```
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
```

`127.0.0.0/8` is **always local**. Packets to `127.0.0.1` never leave the host; no gateway, no ARP, no physical NIC.

---

## The routing table

### Purpose

The **routing table** (Linux **FIB** — Forwarding Information Base) is a ordered set of rules:

> *If the destination IP matches prefix P, send via interface I; optionally forward to gateway G first.*

Every outbound IPv4 packet goes through this lookup (unless bypassed — see `SO_DONTROUTE` / `-r` in [ROUTING.md](ROUTING.md)).

### Anatomy of a route entry

Modern `ip route` output:

```
default via 10.0.2.2 dev eth0 proto dhcp metric 100
10.0.2.0/24 dev eth0 proto kernel scope link src 10.0.2.15
127.0.0.0/8 dev lo scope host
```

| Field | Meaning |
|-------|---------|
| **Destination** | Network prefix or `default` (`0.0.0.0/0`) |
| **via** *GW* | **Gateway** — next-hop router IP (only for off-link destinations) |
| **dev** *IF* | **Output interface** |
| **src** *IP* | Preferred source address for packets using this route |
| **scope** | How far the destination is considered to be (see below) |
| **proto** | How the route was learned: `kernel`, `dhcp`, `static`, `boot` |
| **metric** | Preference when multiple routes match with equal prefix length (lower = preferred) |

Older `/proc/net/route` and `route -n` show the same information in a columnar format; prefer `ip route` on the VM.

### Route scopes

| Scope | Meaning | Example |
|-------|---------|---------|
| **host** | Destination is this machine only | `127.0.0.0/8 dev lo` |
| **link** | On-link — reachable without a remote gateway | `10.0.2.0/24 dev eth0` |
| **universe** | Reachable through forwarding (may need a gateway) | `default via 10.0.2.2` |

Scope affects whether the kernel treats the next hop as **on the same Ethernet segment** (ARP the host or gateway directly) or not.

### Types of routes

| Type | Example | When used |
|------|---------|-----------|
| **Host route** | `192.168.1.50/32 dev eth0` | One specific machine (uncommon on clients) |
| **Subnet / network** | `10.0.2.0/24 dev eth0` | Directly connected LAN |
| **Default route** | `default via 10.0.2.2 dev eth0` | Catch-all for Internet and unknown nets |
| **Gateway route** | `172.16.0.0/12 via 10.0.2.1 dev eth0` | Specific remote network via a particular router |

### Longest prefix match

If several routes match the destination, the kernel chooses the **most specific** (longest network prefix).

```
10.0.0.0/8      via 192.168.1.1 dev eth1    metric 200
10.0.2.0/24     dev eth0                    metric 100
default         via 10.0.2.2 dev eth0       metric 100
```

| Destination | Matching routes | Winner |
|-------------|-----------------|--------|
| `10.0.2.50` | `/8`, `/24`, `default` | **`10.0.2.0/24`** — longest prefix |
| `10.5.1.1` | `/8`, `default` | **`10.0.0.0/8`** via `192.168.1.1` |
| `8.8.8.8` | `default` only | **default** via `10.0.2.2` |

Prefix length always beats metric. Metric breaks ties only when prefix length is equal.

### Directly connected vs remote destinations

| Case | Route looks like | Next hop at L3 | L2 behavior |
|------|------------------|----------------|-------------|
| **Same host** | `127.0.0.0/8 dev lo` | self | Loopback driver |
| **On-link host** | `10.0.2.0/24 dev eth0` | destination IP | ARP for **destination** MAC |
| **Off-link** | `default via 10.0.2.2 dev eth0` | **gateway** IP | ARP for **gateway** MAC |

“On-link” means the kernel believes the destination is directly reachable on that Ethernet/Wi‑Fi segment without sending the packet to a router first.

### Multiple interfaces

A laptop might have Ethernet and Wi‑Fi. Each connected interface typically gets:

- A **direct subnet route** (`dev eth0`, `dev wlan0`).
- Possibly **different default routes** with different **metrics** — the lower metric wins.

```
default via 192.168.1.1 dev wlan0 metric 600
default via 10.0.2.2 dev eth0 metric 100
```

Here `eth0` is preferred for general Internet traffic unless a more specific route says otherwise.

### Policy routing (brief)

Beyond the **main** table (what `ip route` shows), Linux supports **policy routing** (`ip rule`) — multiple tables selected by source address, firewall mark, etc. `ft_ping` does not use this; evaluation VMs use a single main table. Mentioned only so `ip rule` output is not surprising on complex hosts.

---

## Gateways (default and specific)

### What is a gateway?

A **gateway** (also **router**, **next hop**) is a device on the **local link** that forwards IP packets toward destinations the host cannot reach directly.

From the host’s perspective:

- The gateway’s IP is on the **same subnet** as one of the host’s interfaces (e.g. host `10.0.2.15/24`, gateway `10.0.2.2/24`).
- The host does **not** need to know the full path to `8.8.8.8` — only the first hop.

### Default gateway

The **default gateway** is the route of last resort:

```
default via 10.0.2.2 dev eth0
```

Equivalent to destination `0.0.0.0/0` — matches **any** IPv4 address not covered by a more specific route.

```
  ft_ping host                default gateway              Internet
  10.0.2.15                   10.0.2.2
       │                            │
       │  ICMP to 8.8.8.8           │
       │  eth dst = GW MAC          │
       └───────────────────────────►│──── forward ────► … ────► 8.8.8.8
```

Without a default route (and without a more specific route to `8.8.8.8`), the kernel has no next hop → `sendto()` fails or ICMP **Network Unreachable** is generated locally.

Loopback (`127.0.0.1`) and on-LAN peers still work — they use more specific routes and never need the default gateway.

### Specific gateway routes

A **static** or **learned** route can point a whole network at a non-default router:

```
172.16.0.0/12 via 10.0.2.1 dev eth0
```

Traffic to `172.16.5.10` uses `10.0.2.1` instead of the default `10.0.2.2`. Enterprise VPNs and split tunneling rely on such entries.

### Gateway must be on-link

The gateway IP must be reachable on the interface named in the route. Misconfiguration example:

```
default via 8.8.8.8 dev eth0   # broken on a typical LAN — GW not on local subnet
```

The kernel cannot ARP for a gateway that is not on the local link (unless complex features apply). Packets will not leave correctly.

### One hop at a time

The host only ever sets the **immediate** next hop. Each router runs its **own** routing table lookup for `8.8.8.8` and forwards to **its** next hop. That chain of routers is what people call **hops** (see [TTL.md](TTL.md)).

---

## How the kernel routes one packet

Step-by-step for `ft_ping` sending ICMP to `8.8.8.8` on a typical VM:

| Step | Component | Action |
|------|-----------|--------|
| 1 | Application | `sendto(sock, icmp_buf, …, dest=8.8.8.8)` |
| 2 | Socket layer | Attach IP header fields from socket options (TTL, TOS, …) |
| 3 | Routing (FIB) | Lookup `8.8.8.8` → `default via 10.0.2.2 dev eth0 src 10.0.2.15` |
| 4 | Neighbor (ARP) | Resolve MAC for `10.0.2.2` on `eth0` if not cached |
| 5 | L2 | Build Ethernet frame: src=eth0 MAC, dst=gateway MAC |
| 6 | Driver | Queue frame for transmission on `eth0` |
| 7 | Gateway | Decrement TTL, route again, forward toward next router |
| … | Internet | … |
| N | Target `8.8.8.8` | ICMP Echo Reply back (reverse path may differ) |

For `127.0.0.1`:

| Step | Result |
|------|--------|
| Routing lookup | `127.0.0.0/8 dev lo scope host` |
| ARP | Not used |
| Delivery | Loopback — packet never on wire |

For on-link `10.0.2.1` (another VM on same LAN):

| Step | Result |
|------|--------|
| Routing lookup | `10.0.2.0/24 dev eth0` |
| ARP | Resolve MAC of `10.0.2.1` |
| Delivery | Direct Ethernet frame to peer |

### Reverse path (replies)

Inbound ICMP Echo Reply to `8.8.8.8` is addressed to the host’s `10.0.2.15`. The kernel routes it to a local socket (the raw ICMP socket `ft_ping` opened). **Return path routing** is independent — reply TTL and hop count on the wire may differ from the outbound path.

---

## ARP and the link layer

Routing decides **which IP** is the next hop. **ARP** (Address Resolution Protocol) maps that IP to a **MAC address** on Ethernet.

| Destination type | ARP target |
|------------------|------------|
| On-link host `10.0.2.1` | `10.0.2.1` |
| Off-link via gateway | **Gateway** `10.0.2.2`, not `8.8.8.8` |

The Ethernet frame always goes to the **next L2 hop** (neighbor or gateway). The IP header still lists the **final** destination (`8.8.8.8`).

```bash
ip neigh show dev eth0
```

```
10.0.2.2 lladdr 52:54:00:12:34:56 REACHABLE
```

If ARP fails (host down, wrong subnet), you may see **Destination Host Unreachable** locally.

`ft_ping` does not call ARP; it is entirely kernel-side between routing and the driver.

---

## Linux tools and reading output

### Full routing table

```bash
ip route
# or
ip route show table main
```

### One-shot lookup (most useful for debugging)

```bash
ip route get 8.8.8.8
```

Example output:

```
8.8.8.8 via 10.0.2.2 dev eth0 src 10.0.2.15 uid 1000
    cache
```

| Part | Meaning |
|------|---------|
| `via 10.0.2.2` | Gateway (first router hop) |
| `dev eth0` | Outgoing interface |
| `src 10.0.2.15` | Source IP that will be used |
| `cache` | Result may be cached for performance |

Compare destinations:

```bash
ip route get 127.0.0.1
ip route get 10.0.2.2
ip route get 8.8.8.8
```

### Default gateway only

```bash
ip route show default
```

### Interfaces and addresses

```bash
ip link show          # interfaces, MAC, MTU, flags
ip addr show          # all IPv4/IPv6 addresses
ip -4 addr show eth0  # IPv4 only on one interface
```

### Neighbor (ARP) cache

```bash
ip neigh show
```

### Legacy tools (still seen in tutorials)

| Legacy | Modern replacement |
|--------|-------------------|
| `route -n` | `ip route` |
| `netstat -rn` | `ip route` |
| `ifconfig` | `ip addr`, `ip link` |
| `arp -n` | `ip neigh` |

---

## Common setups and examples

### 42 Debian VM (VirtualBox NAT)

Typical values:

| Item | Example |
|------|---------|
| Interface | `eth0` |
| Host IP | `10.0.2.15/24` |
| Default gateway | `10.0.2.2` (VirtualBox NAT router) |
| Loopback | `127.0.0.1` via `lo` |

```bash
ip route
```

```
default via 10.0.2.2 dev eth0
10.0.2.0/24 dev eth0 proto kernel scope link src 10.0.2.15
127.0.0.0/8 dev lo scope host
```

`sudo ./ft_ping -c 3 8.8.8.8` works because the default route sends probes to `10.0.2.2`, which NATs them to the real network.

### Home Wi‑Fi (conceptual)

| Item | Example |
|------|---------|
| Interface | `wlan0` |
| Host IP | `192.168.1.42/24` |
| Default gateway | `192.168.1.1` (home router) |

Same logic: off-LAN traffic → ARP router → forward.

### No default route

If someone deletes the default route:

```bash
sudo ip route del default
```

`ping 8.8.8.8` fails; `ping 127.0.0.1` and pings to peers on `10.0.2.0/24` may still work.

### Bypass routing: `-r` / `SO_DONTROUTE`

`ft_ping -r` sets `SO_DONTROUTE` so the kernel **skips** normal FIB lookup and allows only **directly connected** destinations. See [ROUTING.md](ROUTING.md#-r-and-so_dontroute-bypass-routing-table).

| Target | Normal ping | `ping -r` |
|--------|-------------|-----------|
| `127.0.0.1` | OK (`lo`) | OK (on-link / loopback) |
| `10.0.2.2` (gateway on LAN) | OK | OK (on-link) |
| `8.8.8.8` | OK via gateway | Usually **fails** (not directly connected) |

---

## Failure modes and ICMP errors

When routing or forwarding fails, `ft_ping` may see errors instead of Echo Reply:

| Situation | Typical cause | ICMP / errno | ft_ping message (examples) |
|-----------|---------------|--------------|----------------------------|
| No route to destination | Missing route, `-r` to remote host | `ENETUNREACH` or ICMP Dest Unreachable net | Send error or `Destination Net Unreachable` |
| Gateway unreachable | Router down, ARP failure | ICMP Host Unreachable | `Destination Host Unreachable` |
| TTL exhausted in transit | `--ttl` too low | ICMP Time Exceeded | `Time to live exceeded` |
| Firewall drops ICMP | Silent drop | (none) | 100% packet loss |

Routers generate many of these; `print_icmp_error()` in `print.c` formats them. Details: [ROUTING.md](ROUTING.md#routers-and-icmp-errors), [ICMP.md](ICMP.md).

---

## How ft_ping uses routing

`ft_ping` interacts with routing **indirectly**:

| Stage | File | What happens |
|-------|------|--------------|
| Resolve name | `dns.c` | `getaddrinfo()` → `ping->dest_addr` (IPv4). Routing applies to this IP, not the hostname. |
| Socket options | `socket.c` | `IP_TTL`, `IP_TOS`, optional `SO_DONTROUTE` (`-r`) |
| Send | `send.c` | `sendto(ping->sockfd, …, &ping->dest_addr)` — kernel routes |
| Receive | `recv.c` | Replies/errors arrive on same socket; inbound delivery is kernel routing |

The program **does not**:

- Read or modify the routing table.
- Choose the source interface or source IP (kernel default from route).
- Perform ARP or Ethernet framing.

### Flags that touch routing behavior

| Flag | Effect |
|------|--------|
| *(none)* | Full kernel routing — normal case |
| `-r` | `SO_DONTROUTE` — only directly connected destinations |
| `--ttl N` | Does not change route; limits how many routers can forward the packet |

### Sanity checks on the VM

```bash
ip route get $(./ft_ping -n -c 0 8.8.8.8 2>/dev/null || echo 8.8.8.8)
sudo ./ft_ping -c 2 127.0.0.1
sudo ./ft_ping -c 2 8.8.8.8
sudo ./ft_ping -r -c 1 127.0.0.1
```

---

## Summary diagram

```
INTERFACES                         ROUTING TABLE                    ACTION
──────────                         ─────────────                    ──────

lo     127.0.0.1/8        ───────► 127.0.0.0/8 dev lo      ───────► local delivery
eth0   10.0.2.15/24       ───────► 10.0.2.0/24 dev eth0    ───────► on-link ARP → peer
       (MAC aa:bb:cc)     ───────► default via 10.0.2.2     ───────► ARP gateway → forward

Gateway 10.0.2.2 is not an interface on your host — it is the next-hop IP
on eth0’s subnet. The host only needs eth0 configured + a correct route row.
```
