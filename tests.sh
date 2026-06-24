#!/bin/bash
#
# ft_ping test suite
# Run: sudo bash tests.sh
# Compares ft_ping output with inetutils ping where possible.

set -u

PING="./ft_ping"
REF="ping"  # inetutils ping on Debian VM; on macOS system ping differs
PASS=0
FAIL=0
SKIP=0
TOTAL=0

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[0;33m'
BLU='\033[0;34m'
RST='\033[0m'

header() {
	echo ""
	echo -e "${BLU}=== $1 ===${RST}"
}

ok() {
	((PASS++))
	((TOTAL++))
	echo -e "  ${GRN}[OK]${RST}   $1"
}

ko() {
	((FAIL++))
	((TOTAL++))
	echo -e "  ${RED}[FAIL]${RST} $1"
	[ -n "${2:-}" ] && echo -e "         ${RED}$2${RST}"
}

skip() {
	((SKIP++))
	((TOTAL++))
	echo -e "  ${YEL}[SKIP]${RST} $1"
}

# ──────────────────────────────────────────────
# Pre-checks
# ──────────────────────────────────────────────

if [ "$(id -u)" -ne 0 ]; then
	echo -e "${RED}Error: run with sudo${RST}"
	exit 1
fi

REAL_USER="${SUDO_USER:-$(whoami)}"

if [ ! -x "$PING" ]; then
	echo "Building..."
	sudo -u "$REAL_USER" make -s re || { echo "Build failed"; exit 1; }
fi

if [ ! -x "$PING" ]; then
	echo -e "${RED}Error: $PING not found${RST}"
	exit 1
fi

# ──────────────────────────────────────────────
# MANDATORY TESTS
# ──────────────────────────────────────────────

header "MANDATORY: Binary name"
if [ -f "$PING" ]; then
	ok "ft_ping exists"
else
	ko "ft_ping not found"
fi

# --- Makefile ---
header "MANDATORY: Makefile"
sudo -u "$REAL_USER" make -s re 2>/dev/null && ok "make re" || ko "make re failed"
sudo -u "$REAL_USER" make -s clean 2>/dev/null
if [ -f "$PING" ]; then
	ok "make clean keeps binary"
else
	ko "make clean removed binary"
fi
sudo -u "$REAL_USER" make -s fclean 2>/dev/null
if [ ! -f "$PING" ]; then
	ok "make fclean removes binary"
else
	ko "make fclean did not remove binary"
fi
sudo -u "$REAL_USER" make -s 2>/dev/null && ok "make (rebuild)" || ko "make failed"

# --- Help ---
header "MANDATORY: -? / help"
OUT=$($PING '-?' 2>&1)
if echo "$OUT" | grep -qi "usage\|help\|options"; then
	ok "-? shows usage"
else
	ko "-? does not show usage"
fi

# --- No arguments ---
header "MANDATORY: Error handling"
OUT=$($PING 2>&1)
RET=$?
if [ $RET -ne 0 ]; then
	ok "No args → non-zero exit ($RET)"
else
	ko "No args → exit 0 (expected non-zero)"
fi

# --- Invalid option ---
OUT=$($PING -Z 2>&1)
RET=$?
if [ $RET -ne 0 ]; then
	ok "Invalid option -Z → non-zero exit"
else
	ko "Invalid option -Z → exit 0"
fi

# --- Unknown host ---
OUT=$($PING "thishostdoesnotexist.invalid" 2>&1)
RET=$?
if [ $RET -ne 0 ]; then
	ok "Unknown host → non-zero exit"
else
	ko "Unknown host → exit 0"
fi

# --- Basic ping 127.0.0.1 ---
header "MANDATORY: Basic ping (127.0.0.1)"
OUT=$($PING -c 3 127.0.0.1 2>&1)
RET=$?

if [ $RET -eq 0 ]; then
	ok "Exit code 0"
else
	ko "Exit code $RET (expected 0)"
fi

if echo "$OUT" | grep -q "^PING 127.0.0.1"; then
	ok "Header line present"
else
	ko "Header line missing"
fi

if echo "$OUT" | grep -q "bytes from 127.0.0.1"; then
	ok "Reply lines present"
else
	ko "Reply lines missing"
fi

if echo "$OUT" | grep -q "icmp_seq="; then
	ok "icmp_seq field present"
else
	ko "icmp_seq field missing"
fi

if echo "$OUT" | grep -q "ttl="; then
	ok "ttl field present"
else
	ko "ttl field missing"
fi

if echo "$OUT" | grep -q "time="; then
	ok "time field present"
else
	ko "time field missing"
fi

if echo "$OUT" | grep -q "ping statistics"; then
	ok "Statistics block present"
else
	ko "Statistics block missing"
fi

if echo "$OUT" | grep -q "packets transmitted"; then
	ok "Transmitted count present"
else
	ko "Transmitted count missing"
fi

if echo "$OUT" | grep -q "packets received"; then
	ok "Received count present"
else
	ko "Received count missing"
fi

if echo "$OUT" | grep -q "packet loss"; then
	ok "Packet loss present"
else
	ko "Packet loss missing"
fi

if echo "$OUT" | grep -q "round-trip\|rtt"; then
	ok "RTT stats present"
else
	ko "RTT stats missing"
fi

# --- Count check ---
REPLY_COUNT=$(echo "$OUT" | grep -c "bytes from")
if [ "$REPLY_COUNT" -eq 3 ]; then
	ok "-c 3 → exactly 3 replies"
else
	ko "-c 3 → $REPLY_COUNT replies (expected 3)"
fi

# --- Hostname resolution ---
header "MANDATORY: Hostname resolution"
OUT=$($PING -c 1 localhost 2>&1)
if echo "$OUT" | grep -q "bytes from"; then
	ok "localhost resolves and responds"
else
	ko "localhost ping failed"
fi

# --- Verbose ---
header "MANDATORY: -v (verbose)"
OUT=$($PING -v -c 1 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "id 0x"; then
	ok "-v shows id in header"
else
	ko "-v does not show id in header"
fi

# --- ICMP error with -v (TTL=1 to remote) ---
header "MANDATORY: -v ICMP error (--ttl 1)"
OUT=$(timeout 5 $PING -v --ttl 1 -c 1 -w 3 8.8.8.8 2>&1)
if echo "$OUT" | grep -qi "time.*exceeded\|Time to live\|from"; then
	ok "TTL exceeded error shown with -v"
else
	skip "Could not trigger TTL exceeded (network?)"
fi

# --- FQDN (no reverse DNS on reply) ---
header "MANDATORY: FQDN / no reverse DNS"
OUT=$(timeout 5 $PING -c 1 -w 3 google.com 2>&1)
if echo "$OUT" | grep -q "bytes from"; then
	REPLY_IP=$(echo "$OUT" | grep "bytes from" | head -1 | sed 's/.*from \([^ :]*\).*/\1/')
	if echo "$REPLY_IP" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$'; then
		ok "Reply shows numeric IP (no reverse DNS)"
	else
		ko "Reply shows hostname instead of IP: $REPLY_IP"
	fi
else
	skip "google.com not reachable"
fi

# --- 0% loss on loopback ---
header "MANDATORY: 0% loss on loopback"
OUT=$($PING -c 5 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "0% packet loss"; then
	ok "0% packet loss on loopback"
else
	LOSS=$(echo "$OUT" | grep "packet loss" | sed 's/.*\([0-9]*%\).*/\1/')
	ko "Packet loss on loopback: $LOSS"
fi

# --- No crash on Ctrl+C (SIGINT) ---
header "MANDATORY: SIGINT handling"
$PING 127.0.0.1 &
PID=$!
sleep 2
kill -INT "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
RET=$?
if [ $RET -le 128 ] || [ $RET -eq 130 ]; then
	ok "Clean exit on SIGINT (exit $RET)"
else
	ko "Bad exit on SIGINT (exit $RET)"
fi

# --- Exit code 1 when host unreachable ---
header "MANDATORY: Exit code on failure"
OUT=$(timeout 5 $PING -c 1 -w 2 192.0.2.1 2>&1)
RET=$?
[ $RET -eq 124 ] && RET=1
if [ $RET -ne 0 ]; then
	ok "Unreachable host → non-zero exit ($RET)"
else
	ko "Unreachable host → exit 0 (expected non-zero)"
fi

# ──────────────────────────────────────────────
# BONUS TESTS
# ──────────────────────────────────────────────

header "BONUS: -c <count>"
OUT=$($PING -c 2 127.0.0.1 2>&1)
CNT=$(echo "$OUT" | grep -c "bytes from")
if [ "$CNT" -eq 2 ]; then
	ok "-c 2 → 2 replies"
else
	ko "-c 2 → $CNT replies"
fi

header "BONUS: -s <size>"
OUT=$($PING -c 1 -s 100 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "100 data bytes"; then
	ok "-s 100 → header shows 100 data bytes"
else
	ko "-s 100 → header does not show 100"
fi

header "BONUS: --ttl <N>"
OUT=$($PING -c 1 --ttl 42 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "ttl="; then
	ok "--ttl 42 accepted"
else
	ko "--ttl 42 failed"
fi

header "BONUS: -w <timeout>"
START=$(date +%s)
OUT=$($PING -w 2 127.0.0.1 2>&1)
END=$(date +%s)
ELAPSED=$((END - START))
if [ $ELAPSED -le 4 ]; then
	ok "-w 2 stops within ~2s (took ${ELAPSED}s)"
else
	ko "-w 2 took ${ELAPSED}s (expected ~2)"
fi

header "BONUS: -W <linger>"
OUT=$($PING -c 1 -W 1 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "bytes from"; then
	ok "-W 1 accepted"
else
	ko "-W 1 failed"
fi

header "BONUS: -p <pattern>"
OUT=$($PING -c 1 -p ff 127.0.0.1 2>&1)
RET=$?
if [ $RET -eq 0 ]; then
	ok "-p ff accepted"
else
	ko "-p ff failed (exit $RET)"
fi

OUT=$($PING -c 1 -p ZZ 2>&1)
RET=$?
if [ $RET -ne 0 ]; then
	ok "-p ZZ (invalid) → error"
else
	ko "-p ZZ (invalid) → no error"
fi

header "BONUS: -f (flood)"
OUT=$($PING -f -c 10 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "packets transmitted"; then
	ok "-f flood mode works"
else
	ko "-f flood mode failed"
fi

header "BONUS: -l <preload>"
OUT=$($PING -l 3 -c 5 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "packets transmitted"; then
	ok "-l 3 accepted"
else
	ko "-l 3 failed"
fi

header "BONUS: -n (numeric)"
OUT=$($PING -n -c 1 127.0.0.1 2>&1)
RET=$?
if [ $RET -eq 0 ]; then
	ok "-n accepted"
else
	ko "-n failed"
fi

header "BONUS: -r (bypass routing)"
OUT=$($PING -r -c 1 127.0.0.1 2>&1)
if echo "$OUT" | grep -q "bytes from"; then
	ok "-r on loopback works"
else
	ko "-r on loopback failed"
fi

header "BONUS: -T <tos>"
OUT=$($PING -c 1 -T 0 127.0.0.1 2>&1)
RET=$?
if [ $RET -eq 0 ]; then
	ok "-T 0 accepted"
else
	ko "-T 0 failed"
fi

header "BONUS: --ip-timestamp tsonly"
OUT=$($PING -c 1 --ip-timestamp tsonly 127.0.0.1 2>&1)
if echo "$OUT" | grep -qi "TS:\|ms"; then
	ok "--ip-timestamp tsonly shows timestamps"
else
	skip "--ip-timestamp tsonly (may not work on all systems)"
fi

# ──────────────────────────────────────────────
# EDGE CASES / CRASH TESTS
# ──────────────────────────────────────────────

header "EDGE: No crash on bad inputs"

for ARG in \
	"-c 0" \
	"-c -1" \
	"-c abc" \
	"-s 99999" \
	"-s -1" \
	"--ttl 0" \
	"--ttl 999" \
	"-T 256" \
	"-w 0" \
	"-W -1" \
	"-l -1" \
	""; do
	if [ -z "$ARG" ]; then continue; fi
	# shellcheck disable=SC2086
	timeout 3 $PING $ARG 127.0.0.1 </dev/null >/dev/null 2>&1
	RET=$?
	# timeout returns 124 when killed; treat as normal (no crash)
	[ $RET -eq 124 ] && RET=0
	if [ $RET -le 128 ]; then
		ok "No crash: ft_ping $ARG (exit $RET)"
	else
		SIG=$((RET - 128))
		ko "Crash (signal $SIG): ft_ping $ARG"
	fi
done

# multiple hosts
$PING 127.0.0.1 127.0.0.2 </dev/null >/dev/null 2>&1
RET=$?
if [ $RET -ne 0 ] && [ $RET -le 128 ]; then
	ok "Multiple hosts → error, no crash (exit $RET)"
else
	ko "Multiple hosts → unexpected exit $RET"
fi

# ──────────────────────────────────────────────
# OUTPUT FORMAT (compare with inetutils ping)
# ──────────────────────────────────────────────

header "OUTPUT FORMAT: Compare with inetutils ping"
if command -v $REF >/dev/null 2>&1; then
	REF_VER=$($REF -V 2>&1 | head -1)
	echo "  Reference: $REF_VER"

	FT_OUT=$($PING -c 1 127.0.0.1 2>&1)
	REF_OUT=$($REF -c 1 127.0.0.1 2>&1)

	FT_HDR=$(echo "$FT_OUT" | head -1)
	REF_HDR=$(echo "$REF_OUT" | head -1)
	if [ "$(echo "$FT_HDR" | sed 's/[0-9]//g')" = "$(echo "$REF_HDR" | sed 's/[0-9]//g')" ]; then
		ok "Header format matches"
	else
		ko "Header format differs" "ft:  $FT_HDR\n         ref: $REF_HDR"
	fi

	FT_STAT=$(echo "$FT_OUT" | grep "ping statistics")
	REF_STAT=$(echo "$REF_OUT" | grep "ping statistics")
	if [ -n "$FT_STAT" ] && [ -n "$REF_STAT" ]; then
		if [ "$(echo "$FT_STAT" | sed 's/[^ ]*//')" = "$(echo "$REF_STAT" | sed 's/[^ ]*//')" ]; then
			ok "Statistics header matches"
		else
			ko "Statistics header differs" "ft:  $FT_STAT\n         ref: $REF_STAT"
		fi
	fi
else
	skip "inetutils ping not found — format comparison skipped"
fi

# ──────────────────────────────────────────────
# SUMMARY
# ──────────────────────────────────────────────

echo ""
echo -e "${BLU}═══════════════════════════════════════${RST}"
echo -e "  Total: $TOTAL"
echo -e "  ${GRN}Pass:  $PASS${RST}"
echo -e "  ${RED}Fail:  $FAIL${RST}"
echo -e "  ${YEL}Skip:  $SKIP${RST}"
echo -e "${BLU}═══════════════════════════════════════${RST}"

[ $FAIL -eq 0 ] && exit 0 || exit 1
