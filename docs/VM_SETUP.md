# Virtual machine setup for `ft_ping`

This project is expected to be **developed, run, and defended** on a **Debian virtual machine** (Debian **>= 7.0**, Linux kernel **> 3.14**). Evaluation is done on that environment, not on your host OS alone.

This guide walks through creating the VM, preparing it, moving the project onto it, building, and getting ready for defense.

---

## Requirements (evaluation environment)

| Requirement | Why it matters |
|-------------|----------------|
| Debian VM (>= 7.0) | Expected evaluation target |
| Kernel > 3.14 | Raw sockets / networking behavior |
| VM usable from a **cluster computer** | You must be able to log in and work from 42 machines |
| `gcc`, `make`, project builds with your `Makefile` | Standard 42 C project rules |
| Run with **root** (`sudo`) | Raw ICMP sockets need privileges |

You may develop on macOS or another Linux box for convenience, but **always verify on the Debian VM** before submission and defense.

---

## 1. Create the virtual machine

Use any hypervisor you prefer (VirtualBox, VMware Fusion, UTM, Parallels, etc.).

### Suggested VM settings

| Setting | Recommendation |
|---------|----------------|
| OS | Debian (64-bit). A current **Debian 12** or **11** image is fine and satisfies >= 7.0 |
| RAM | 2 GB minimum (4 GB comfortable) |
| Disk | 20–40 GB |
| Network | **NAT** or **Bridged** (bridged is easier if you need SSH from another machine on the LAN) |
| CPU | 1–2 cores |

### Download Debian

- Official images: https://www.debian.org/download
- Pick the **netinst** (small installer) ISO if you want a minimal system.

Install Debian with defaults, create a user account, and enable **OpenSSH server** during install if the installer offers it (or install it later).

---

## 2. First boot: system checks

Log in on the VM (console or SSH) and confirm versions:

```bash
cat /etc/debian_version
uname -r
```

Expected:

- Debian version **7.0 or higher**
- Kernel version **greater than 3.14** (e.g. `6.x` on modern Debian)

---

## 3. Install build tools and basics

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  make \
  gcc \
  git \
  curl \
  ca-certificates
```

Optional but useful for defense and comparison:

```bash
sudo apt-get install -y \
  inetutils-ping \
  dnsutils \
  iproute2 \
  net-tools
```

`inetutils-ping` provides a reference `ping` (expected output style: **inetutils-2.0**). Check version if needed:

```bash
ping -V
```

---

## 4. Networking

`ft_ping` needs **IPv4** connectivity and **DNS** for hostnames.

```bash
ip addr
ip route
ping -c 2 127.0.0.1
ping -c 2 8.8.8.8
ping -c 2 google.com
```

If DNS fails, check `/etc/resolv.conf` and your VM network mode (NAT usually works out of the box).

**ICMP from the VM:** many networks allow outbound ping; if `ping 8.8.8.8` works, `ft_ping` should be able to reach the same targets (with `sudo`).

---

## 5. Get the project onto the VM

Pick one method.

### Option A: Git clone (recommended)

On the VM:

```bash
cd ~
git clone <your-repo-url> ft_ping
cd ft_ping
```

Use your 42 Git remote (GitLab intra, etc.).

### Option B: Copy from your laptop (SCP)

From your **host** (replace `user` and `vm-ip`):

```bash
scp -r /path/to/ft_ping user@vm-ip:~/
```

Then on the VM:

```bash
cd ~/ft_ping
```

### Option C: 42 cluster → VM

If the repo lives on the cluster, clone or `git pull` there, then `scp` to the VM, or clone directly on the VM if the VM has network access to Git.

---

## 6. Build and run

```bash
cd ~/ft_ping
make re
```

Run **as root** (raw socket):

```bash
sudo ./ft_ping -c 3 127.0.0.1
sudo ./ft_ping -c 3 8.8.8.8
sudo ./ft_ping -?
```

Help (`-?`) must work **without** sudo (parsing happens before the socket is created).

Quick mandatory checks:

```bash
sudo ./ft_ping -v -c 2 127.0.0.1
sudo ./ft_ping --ttl 1 -c 2 8.8.8.8
```

You should see echo replies on localhost and often **Time to live exceeded** toward a remote host when TTL is 1.

Full test checklist: `docs/TESTING.md`.

---

## 7. Use the VM from a 42 cluster computer

You should be able to use the VM **from a cluster machine**.

Typical setup:

1. **SSH server on the VM**

   ```bash
   sudo apt-get install -y openssh-server
   sudo systemctl enable ssh
   sudo systemctl start ssh
   ip addr
   ```

2. **Know the VM IP** (bridged adapter → LAN IP; or port-forward SSH in VirtualBox NAT: host `localhost:2222` → guest `22`).

3. **From the cluster**

   ```bash
   ssh youruser@<vm-ip>
   cd ~/ft_ping
   make re
   sudo ./ft_ping -c 2 127.0.0.1
   ```

If the cluster cannot reach your laptop’s NAT VM, use **bridged networking** or run the VM on a machine reachable from the cluster (some students keep the VM on a home server or use 42’s provided VMs if available).

---

## 8. Compare with reference `ping` (optional)

On Debian, system `ping` is often from inetutils or iputils. For output shape checks:

```bash
ping -c 3 127.0.0.1
sudo ./ft_ping -c 3 127.0.0.1
```

Compare:

- `PING host (ip): N data bytes` header
- Reply line layout (`bytes from`, `icmp_seq`, `ttl`, `time`)
- Statistics block and RTT summary

A **±30 ms** tolerance on receive timing is typically accepted; line **indentation** should match inetutils-2.0 (except RTT values and no reverse DNS on replies).



---
