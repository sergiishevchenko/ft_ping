# Routing, hops, and the routing table

When `ft_ping` calls `sendto()`, the kernel must decide **which network interface** to use and **which next-hop device** should receive the packet. That decision comes from the **routing table** — a list of rules maintained by the operating system (and sometimes updated by DHCP or network managers).

This page explains **hops**, **how routing works**, **common Linux tools**, and how **`ft_ping`** interacts with routing (`--ttl`, `-r`, ICMP errors from routers).

Related: [ROUTING-TABLE-INTERFACES-GATEWAY.md](ROUTING-TABLE-INTERFACES-GATEWAY.md) (interfaces, FIB fields, gateways, ARP — **detailed**), [TTL.md](TTL.md), [IPv4.md](IPv4.md), [ICMP.md](ICMP.md), [SOCKET.md](SOCKET.md), [KERNEL-NETWORKING.md](KERNEL-NETWORKING.md).

---

## What is a hop?

A **hop** is one **layer-3 forwarding step** — typically one **router** (or L3 switch) that receives an IP packet, looks at the destination address, and forwards it toward the next network.

```
  source host      hop 1           hop 2           destination
  (ft_ping)        (router)        (router)        (8.8.8.8)
     |                |               |                |
     |--- packet ---->|               |                |
     |                |--- packet --->|                |
     |                |               |--- packet ---->|
```

Each hop **decrements TTL** by 1 (see [TTL.md](TTL.md)). If TTL reaches 0 at a router, that router **drops** the packet and usually sends ICMP **Time Exceeded** back to the source — that reveals the router exists on the path.

| Term | Meaning |
|------|---------|
| **Hop** | One router (or L3 device) along the path |
| **Next hop** | The immediate neighbor the kernel sends the packet to (often the default gateway) |
| **Path** | Ordered list of hops from source to destination |
| **RTT** | Round-trip time — not the same as hop count; includes queuing and host processing |

`ping` normally uses a **high TTL** (default 64) so the packet survives enough hops to reach the target. Intermediate hops are not shown intermediate hops in normal ping output — only the final reply (or an error if something blocks the path).

---

## The routing table

The **routing table** answers: *“For destination IP X, which interface and which next hop should be used?”*

On Linux the kernel holds this table. User-space tools (`ip route`, NetworkManager, etc.) add or change entries.

For a full treatment of **interfaces**, **route entry fields** (`via`, `dev`, `scope`, `metric`), **gateways**, **ARP**, and **`ip route get`**, see [ROUTING-TABLE-INTERFACES-GATEWAY.md](ROUTING-TABLE-INTERFACES-GATEWAY.md).

### Example table (typical home / VM setup)

```bash
ip route
```

```
default via 10.0.2.2 dev eth0
10.0.2.0/24 dev eth0 proto kernel scope link src 10.0.2.15
127.0.0.0/8 dev lo scope host
```

| Route | Meaning |
|-------|---------|
| `default via 10.0.2.2 dev eth0` | **Default gateway** — any destination not matched below goes to `10.0.2.2` on `eth0` |
| `10.0.2.0/24 dev eth0` | **Directly connected** — hosts on the local LAN; send on `eth0` without a remote gateway |
| `127.0.0.0/8 dev lo` | **Loopback** — `127.0.0.1` stays on interface `lo` |

### Longest prefix match

If several routes could match a destination, the kernel picks the **most specific** one (longest network prefix).

Example:

```
10.0.0.0/8      via 192.168.1.1
10.0.2.0/24     dev eth0
```

Destination `10.0.2.50` matches **both**, but `/24` is more specific → packet goes **directly on eth0**, not via `192.168.1.1`.

### Directly connected vs remote

| Destination type | Kernel behavior |
|------------------|-----------------|
| **On-link** (same subnet as an interface) | ARP resolves the host’s MAC; frame sent directly |
| **Off-link** (elsewhere on the Internet) | Packet sent to **default gateway** (or a more specific gateway route); gateway forwards further |

`127.0.0.1` is always handled by the **loopback** route — no physical NIC, no external router.

---

## End-to-end: what happens when ft_ping sends a probe

```
  ft_ping                    kernel                         network
  ───────                    ──────                         ───────

  send_ping()
  sendto(ICMP)  ──────────►  1. Lookup route for dest IP
                             2. Build IP header (src, dst, TTL, TOS, …)
                             3. Set link-layer next hop (ARP / gateway MAC)
                             4. Transmit on chosen interface
                                        ─────────────────────────────►
                                                             routers …
                                                             target
```

`ft_ping` never reads the routing table directly. It only:

1. Sets **socket options** that affect the IP header (`IP_TTL`, `IP_TOS`, `SO_DONTROUTE`) — see [SOCKET.md](SOCKET.md).
2. Passes the **destination address** (`ping->dest_addr`) to `sendto()`.
3. The **kernel** performs routing.

On receive, ICMP replies and errors come back through the same socket; routing for **inbound** packets is the kernel’s job (reverse path).

---

## Default gateway

The **default gateway** (default route) is the router the host uses when it does not have a more specific route.

```
  Laptop 10.0.2.15  ──►  Gateway 10.0.2.2  ──►  ISP router  ──►  Internet  ──►  8.8.8.8
         (ft_ping)         (hop 1 from VM)      (hop 2…)
```

Without a default route, `ping 8.8.8.8` fails — there is no instruction for “where to send unknown destinations.” Loopback (`127.0.0.1`) still works because it uses the `127.0.0.0/8` route.

Check gateway on the VM:

```bash
ip route show default
ip addr show
```

---

## TTL, hops, and traceroute

| Mechanism | What it does |
|-----------|--------------|
| **TTL** | Hop **limit** — decremented at each router; at 0 → drop + ICMP Time Exceeded |
| **Routing table** | Hop **direction** — where to send the packet next |
| **traceroute** | Sends probes with TTL=1, then 2, then 3… collecting Time Exceeded from each hop |
| **ping** | Usually TTL=64 so probes **reach** the target; `--ttl 1` forces failure at **first** router |

Classic diagnostic:

```bash
sudo ./ft_ping --ttl 1 -c 2 8.8.8.8
```

Expected: no reply from `8.8.8.8`; ICMP **Time to live exceeded** from the **first routing hop** (often the default gateway). With `-v`, the **quoted inner packet** is also shown — the original probe’s destination and TTL.

Full TTL behavior in the project: [TTL.md](TTL.md).

### TTL in echo reply lines

Example reply line:

```
64 bytes from 8.8.8.8: icmp_seq=0 ttl=118 time=12.3 ms
```

`ttl=118` is the **remaining TTL in the reply’s IP header**, not the outgoing value. The remote host chose its own initial TTL (often 64, 128, or 255); each router on the **return path** decremented it. Rough rule of thumb:

```
initial_ttl_peer - ttl_in_reply ≈ hops on return path
```

It is indicative, not exact — different paths, load balancers, and TTL rewriting can confuse the number.

---

## `-r` and `SO_DONTROUTE` (bypass routing table)

Flag **`-r`** sets `ping->dontroute` in `handle_option()` (`main.c`); `set_sock_options()` in `socket.c` applies:

```c
setsockopt(ping->sockfd, SOL_SOCKET, SO_DONTROUTE, &one, sizeof(one));
```

| With normal routing | With `-r` (`SO_DONTROUTE`) |
|---------------------|----------------------------|
| Kernel uses full routing table | Kernel sends **only** to **directly connected** destinations |
| `8.8.8.8` → via default gateway | Remote Internet hosts usually **fail** (no route) |
| `127.0.0.1` → loopback | `127.0.0.1` still works (on-link / loopback) |

Purpose in the project: prove the flag is wired to `setsockopt` and the program handles send failures cleanly — not to replace normal ping usage.

---

## Routers and ICMP errors

Routers **forward** IP packets; they do not terminate a normal ping unless something goes wrong.

| Situation | Who responds | ICMP type (typical) | ft_ping output |
|-----------|--------------|---------------------|----------------|
| TTL → 0 in transit | Router | 11 Time Exceeded, code 0 | `Time to live exceeded` |
| No route to network | Router | 3 Destination Unreachable, code 0 | `Destination Net Unreachable` |
| Host down / no ARP | Router or local stack | 3 Dest Unreachable, code 1 | `Destination Host Unreachable` |
| Firewall blocks ICMP | Often silence | (no reply) | 100% packet loss |
| Packet reaches target | Target host | 0 Echo Reply | `64 bytes from … time=… ms` |

`print_icmp_error()` in `print.c` formats these messages. Without `-v`, errors are shown only when the **quoted inner IP header** was destined for **the current target** — unrelated ICMP traffic is filtered out.

Details: [ICMP.md](ICMP.md).

---

## Useful commands on the Debian VM

| Command | Shows |
|---------|--------|
| `ip route` | Full routing table |
| `ip route get 8.8.8.8` | Which interface/gateway the kernel **would** use for one destination |
| `ip addr` | Interface addresses (local source addresses) |
| `ping -c 2 8.8.8.8` | System ping — sanity check that routing + ICMP work |
| `traceroute 8.8.8.8` | List of hops (if installed and not blocked) |

Example:

```bash
ip route get 8.8.8.8
# 8.8.8.8 via 10.0.2.2 dev eth0 src 10.0.2.15 uid 1000
```

That line is exactly what the kernel uses before the probe leaves the machine.

---

## Routing vs other ft_ping concepts

| Concept | Layer | ft_ping touchpoint |
|---------|-------|-------------------|
| **Routing table** | Kernel / IP | Implicit on every `sendto()`; `-r` disables normal lookup |
| **TTL / hops** | IPv4 header | `--ttl` → `setsockopt(IP_TTL)` |
| **TOS** | IPv4 header | `-T` → `setsockopt(IP_TOS)` — QoS hint, often ignored by routers |
| **DNS** | Application | `resolve_host()` → IP **before** routing applies |
| **Default gateway** | Kernel route | Needed for any non-local Internet target |
| **Record Route (RR)** | IP option | Not set by ft_ping on send; may **print** `RR:` if seen on replies — [SOCKET.md](SOCKET.md) |

---

## Overview diagram

```
                    ROUTING TABLE                    PATH ON THE WIRE
                    ─────────────                    ─────────────────

  dest 8.8.8.8  ──► default via 10.0.2.2  ──►  src ──► R1 ──► R2 ──► … ──► 8.8.8.8
  dest 127.0.0.1 ──► dev lo               ──►  src ──► (loopback, 0 routers)
  dest 10.0.2.1  ──► dev eth0 on-link     ──►  src ──► LAN host (ARP)

  Each router: TTL-- ; if TTL==0 → ICMP Time Exceeded to source
  ping: one path, one RTT measurement per reply
  traceroute: many TTL values → one hop revealed per step
```
