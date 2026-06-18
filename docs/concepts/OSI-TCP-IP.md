# OSI and TCP/IP models

Two reference models describe how data moves across a network: the **OSI model** (7 layers, theoretical) and the **TCP/IP model** (4 layers, practical Internet stack). **ft_ping** lives at the boundary of **application** and **network** layers: your program builds ICMP messages; the kernel handles IP and everything below.

---

## Why these models exist

Networks stack **abstractions** so each layer can evolve independently:

- Upper layers do not need to know whether the link is Ethernet, Wi‑Fi, or fiber.
- Lower layers do not need to know whether the payload is a web page or a ping probe.

A **protocol data unit (PDU)** at each layer wraps the layer above with its own header (and sometimes trailer):

```
Application data
    → segment/datagram (transport)
        → packet (network)
            → frame (link)
                → bits (physical)
```

On receive, each layer strips its header and passes the payload up.

---

## OSI model (7 layers)

The **Open Systems Interconnection** model (ISO/IEC 7498) is a teaching and design reference. Real systems rarely implement all seven layers as separate APIs, but the names are standard vocabulary.

| Layer | Name | PDU | Role | Examples |
|-------|------|-----|------|----------|
| **7** | Application | Data | User-facing services and APIs | HTTP, DNS, `ping` / `ft_ping` |
| **6** | Presentation | Data | Encoding, compression, encryption | TLS (often folded into app today) |
| **5** | Session | Data | Dialog control, checkpoints | Rare as a separate layer now |
| **4** | Transport | Segment | End-to-end delivery, ports | TCP, UDP |
| **3** | Network | Packet | Routing across networks | **IPv4**, IPv6, **ICMP** |
| **2** | Data Link | Frame | Hop-to-hop on one link | Ethernet, Wi‑Fi (802.11) |
| **1** | Physical | Bits | Electrical/optical/radio signals | Cables, NIC hardware |

### Mnemonic (top to bottom)

**A**ll **P**eople **S**eem **T**o **N**eed **D**ata **P**rocessing — Application, Presentation, Session, Transport, Network, Data link, Physical.

### Encapsulation (OSI view)

Sending `ft_ping` probe to `8.8.8.8` over Ethernet (simplified):

```
┌─────────────────────────────────────────────────────────────┐
│ L7  Application: ft_ping builds ICMP Echo Request           │
├─────────────────────────────────────────────────────────────┤
│ L4  Transport: (none for ICMP — not TCP/UDP)                │
├─────────────────────────────────────────────────────────────┤
│ L3  Network: IPv4 header + ICMP  (kernel adds IP on send)   │
├─────────────────────────────────────────────────────────────┤
│ L2  Data link: Ethernet header + frame trailer (FCS)        │
├─────────────────────────────────────────────────────────────┤
│ L1  Physical: signals on wire / air                         │
└─────────────────────────────────────────────────────────────┘
```

ICMP is often drawn **inside** layer 3 because it rides directly on IP (protocol number 1), not on TCP or UDP.

---

## TCP/IP model (4 layers)

The **TCP/IP** or **Internet model** matches how Unix, Linux, and BSD actually structure the stack. It predates OSI and powers the global Internet.

| TCP/IP layer | Maps to OSI (approx.) | Role | Key protocols |
|--------------|----------------------|------|----------------|
| **Application** | 5–7 | Programs and high-level protocols | HTTP, SSH, DNS, **ping** |
| **Transport** | 4 | Host-to-host multiplexing (ports) | TCP, UDP |
| **Internet** | 3 | Global addressing and routing | **IPv4**, IPv6, **ICMP** |
| **Network access** | 1–2 | Local link + physical | Ethernet, ARP, Wi‑Fi driver |

### Same probe, TCP/IP view

```
┌─────────────────────────────────────────────────────────────┐
│ Application     ft_ping (user process)                      │
├─────────────────────────────────────────────────────────────┤
│ Transport       — (ICMP bypasses TCP/UDP)                   │
├─────────────────────────────────────────────────────────────┤
│ Internet        IP + ICMP                                   │
├─────────────────────────────────────────────────────────────┤
│ Network access  Ethernet frames, driver, NIC                │
└─────────────────────────────────────────────────────────────┘
```

---

## OSI ↔ TCP/IP mapping

The models are **not identical**; mapping is approximate:

```
   OSI                          TCP/IP
  ┌────────────┐
  │ Application│  ┐
  ├────────────┤  │
  │Presentation│  ├── Application
  ├────────────┤  │
  │  Session   │  ┘
  ├────────────┤
  │ Transport  │──── Transport
  ├────────────┤
  │  Network   │──── Internet
  ├────────────┤
  │ Data link  │  ┐
  ├────────────┤  ├── Network access
  │  Physical  │  ┘
  └────────────┘
```

| Concept | OSI term | TCP/IP term | In `ft_ping` |
|---------|----------|-------------|--------------|
| Your program | Layer 7 | Application | `main.c`, `send.c`, `recv.c` |
| Ports / TCP | Layer 4 | Transport | **Not used** |
| IP addresses, routing | Layer 3 | Internet | Kernel + `struct ip` in `recv.c` |
| ICMP echo | Layer 3 (with IP) | Internet | Entire probe logic |
| Ethernet MAC | Layer 2 | Network access | Kernel / NIC (invisible to app) |
| Cable / radio | Layer 1 | Network access | Hardware |

---

## Layer 3 vs layer 4: why ping is special

Most apps use **sockets at layer 4**:

```c
socket(AF_INET, SOCK_STREAM, 0);   /* TCP */
socket(AF_INET, SOCK_DGRAM, 0);    /* UDP */
```

**ft_ping** uses a **raw socket at layer 3**:

```c
socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

| Aspect | TCP/UDP socket | Raw ICMP (`ft_ping`) |
|--------|----------------|----------------------|
| What you send | Payload; kernel adds IP + transport header | **ICMP message**; kernel adds **IP** header |
| Ports | Source/dest port in transport header | No ports |
| Privileges | Usually any user | **Root** required on most systems |
| What you receive | Transport payload (or connected stream) | **Full IP packet** (IP + ICMP) |

ICMP is an integral part of the Internet layer in TCP/IP: it shares IP addresses with TCP/UDP but uses its own message types (echo, unreachable, time exceeded). See [ICMP.md](ICMP.md).

---

## Addresses at each layer

Each layer uses its own addressing scheme. A single ping crosses all of them:

| Layer | Address example | Scope |
|-------|-----------------|-------|
| Application | `google.com` (hostname) | Human / DNS name |
| Internet (L3) | `142.250.185.46` | Global IPv4 |
| Data link (L2) | `aa:bb:cc:dd:ee:ff` (MAC) | One broadcast domain (hop) |

**DNS** (`resolve_host()` in `srcs/dns.c`) resolves the hostname to IPv4 at **application** setup time. After that, probes use `struct sockaddr_in` with the binary IPv4 address.

**ARP** (Address Resolution Protocol) maps the **next hop** IP to a MAC address on the local link. `ft_ping` never calls ARP; the kernel performs it when emitting frames.

---

## Data flow: one Echo Request

### Outbound (send)

```
1. Application (ft_ping)
   send_ping() → ICMP type 8, id, seq, payload, checksum

2. Transport
   (skipped)

3. Internet (kernel)
   Add IPv4 header: src, dst, TTL (--ttl), TOS (-T), proto=ICMP
   Routing table → choose interface and next hop
   Optional: fragment if MTU requires (unusual for small pings)

4. Network access (kernel + NIC)
   ARP if needed → destination MAC
   Wrap in Ethernet frame
   Transmit on wire

5. Physical
   Electrical/optical/radio transmission
```

### Inbound (Echo Reply)

```
5. Physical → frame received

4. Network access
   NIC/driver validates frame, passes payload up

3. Internet
   Kernel validates IP, demultiplexes proto=ICMP to raw socket

2. Transport
   (skipped)

1. Application
   recvmsg() in recv_ping() → parse IP header, then ICMP
   print_echo_reply() if type 0 and id matches
```

---

## Layer responsibilities vs `ft_ping` code

| Layer | Who implements | What `ft_ping` does |
|-------|----------------|---------------------|
| Application | Your code | CLI, loop, stats, build/parse ICMP |
| Transport | — | Nothing |
| Internet | Kernel + your parsing | `setsockopt(IP_TTL, IP_TOS)`; read `struct ip`; ICMP in `send.c` / `recv.c` |
| Network access | Kernel / driver | Nothing directly |
| Physical | Hardware | Nothing |

Fields you configure from user space map to **IP header** (Internet layer):

| Flag | IP field | Doc |
|------|----------|-----|
| `--ttl N` | TTL | [TTL.md](TTL.md) |
| `-T N` | Type of Service | [TOS.md](TOS.md) |

Fields you read from replies:

| Output | Layer | Field |
|--------|-------|-------|
| `ttl=118` | Internet (IP) | `ip_ttl` in reply |
| `icmp_seq=0` | Internet (ICMP) | sequence in ICMP header |
| `time=1.234 ms` | Application | computed from echoed payload |

---

## Comparison table: OSI vs TCP/IP vs ping

| Question | OSI answer | TCP/IP answer | `ft_ping` |
|----------|------------|---------------|-----------|
| How many layers? | 7 | 4 | Uses app + Internet concepts |
| Where is ICMP? | Network (L3) | Internet | Core of the program |
| Where is IP? | Network (L3) | Internet | Kernel adds; app parses on recv |
| Where is TCP? | Transport (L4) | Transport | Not used |
| End-to-end reliability? | L4 (TCP) | Transport | None — ICMP echo is best-effort |
| Hop-by-hop limit? | L3 TTL | Internet TTL | `--ttl` |

---

## Common misconceptions

| Myth | Reality |
|------|---------|
| “Ping uses TCP or UDP” | Ping uses **ICMP**, which sits on IP with protocol number 1. |
| “OSI and TCP/IP layers are 1:1” | Presentation and session are usually merged into **application** in practice. |
| “Layer 2 addresses reach the destination host globally” | MAC addresses are **per hop**; IP addresses are end-to-end. |
| “`ft_ping` sends full IP packets” | It sends **ICMP only**; the kernel builds the IP header. |
| “More layers = more headers on every packet” | A given protocol stack only adds the headers it needs; ping has no transport header. |


## Further reading (concepts)

- **RFC 791** — IPv4
- **RFC 792** — ICMP
- **RFC 1122** — Host requirements (ICMP echo handling)
- OSI vs TCP/IP: both are **models**; the running code on Linux follows the TCP/IP layering with BSD sockets as the API boundary between user space and kernel.
