#!/bin/bash
#
# diff_tests.sh — compare ft_ping output with inetutils ping via diff
# Run: sudo bash diff_tests.sh
#
# Rules (from the evaluation scale):
#   - time= values: ±30 ms tolerance → replaced with placeholder
#   - round-trip line: ignored (removed before diff)
#   - DNS resolution in reply: not mandatory (both use numeric IP)
#   - Program stopped via Ctrl+C (simulated with timeout -s INT)

set -u

PING="./ft_ping"
REF="ping"

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[0;33m'
BLU='\033[0;34m'
RST='\033[0m'

PASS=0
FAIL=0
SKIP=0
TOTAL=0

if [ "$(id -u)" -ne 0 ]; then
	echo -e "${RED}Error: run with sudo${RST}"
	exit 1
fi

if [ ! -x "$PING" ]; then
	REAL_USER="${SUDO_USER:-$(whoami)}"
	echo "Building..."
	sudo -u "$REAL_USER" make -s re || { echo "Build failed"; exit 1; }
fi

if ! command -v "$REF" > /dev/null 2>&1; then
	echo -e "${RED}Error: system ping not found${RST}"
	exit 1
fi

echo -e "${BLU}Reference: $($REF -V 2>&1 | head -1)${RST}"
echo ""

norm() {
	grep -v '^round-trip' \
	| sed -E 's/time=[0-9.]+ ms/time=TIME ms/' \
	| sed -E 's/id 0x[0-9a-f]+ = [0-9]+/id 0xID = ID/' \
	| sed -E 's/from [a-zA-Z0-9._-]+ \(([0-9.]+)\)/from \1/' \
	| sed 's/[[:space:]]*$//'
}

norm_hostname() {
	grep -v '^round-trip' \
	| sed -E 's/time=[0-9.]+ ms/time=TIME ms/' \
	| sed -E 's/id 0x[0-9a-f]+ = [0-9]+/id 0xID = ID/' \
	| sed -E 's/from [a-zA-Z0-9._-]+ \(([0-9.]+)\)/from \1/' \
	| sed -E 's/\([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+\)/(IP)/' \
	| sed -E 's/from [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/from IP/' \
	| sed 's/[[:space:]]*$//'
}

norm_verbose() {
	grep -v '^round-trip' \
	| sed -E 's/time=[0-9.]+ ms/time=TIME ms/' \
	| sed -E 's/id 0x[0-9a-f]+ = [0-9]+/id 0xID = ID/' \
	| sed -E 's/from [a-zA-Z0-9._-]+ \(([0-9.]+)\)/from \1/' \
	| sed -E 's/^ [0-9a-f ]{39,}$/IP_HDR_DUMP/' \
	| sed 's/[[:space:]]*$//'
}

norm_flood() {
	grep -E '(^PING|statistics|transmitted|round-trip)' \
	| grep -v '^round-trip' \
	| sed 's/[[:space:]]*$//'
}

run_diff() {
	local name="$1"; shift
	local ref_args="$*"
	local ft_args="$*"

	((TOTAL++))

	$REF $ref_args > /tmp/diff_ref.txt 2>&1
	local ref_ret=$?
	$PING $ft_args > /tmp/diff_ft.txt 2>&1
	local ft_ret=$?

	norm < /tmp/diff_ref.txt > /tmp/diff_ref_norm.txt
	norm < /tmp/diff_ft.txt  > /tmp/diff_ft_norm.txt

	if diff -q /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt > /dev/null 2>&1; then
		echo -e "  ${GRN}[OK]${RST}   $name"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name"
		diff /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt | head -8 | sed 's/^/         /'
		((FAIL++))
	fi
}

run_diff_hostname() {
	local name="$1"; shift
	local args="$*"

	((TOTAL++))

	$REF $args > /tmp/diff_ref.txt 2>&1
	$PING $args > /tmp/diff_ft.txt 2>&1

	norm_hostname < /tmp/diff_ref.txt > /tmp/diff_ref_norm.txt
	norm_hostname < /tmp/diff_ft.txt  > /tmp/diff_ft_norm.txt

	if diff -q /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt > /dev/null 2>&1; then
		echo -e "  ${GRN}[OK]${RST}   $name"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name"
		diff /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt | head -8 | sed 's/^/         /'
		((FAIL++))
	fi
}

run_diff_verbose() {
	local name="$1"; shift
	local args="$*"

	((TOTAL++))

	$REF $args > /tmp/diff_ref.txt 2>&1
	$PING $args > /tmp/diff_ft.txt 2>&1

	norm_verbose < /tmp/diff_ref.txt > /tmp/diff_ref_norm.txt
	norm_verbose < /tmp/diff_ft.txt  > /tmp/diff_ft_norm.txt

	if diff -q /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt > /dev/null 2>&1; then
		echo -e "  ${GRN}[OK]${RST}   $name"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name"
		diff /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt | head -8 | sed 's/^/         /'
		((FAIL++))
	fi
}

run_diff_ts() {
	local name="$1"; shift
	local args="$*"

	((TOTAL++))

	$REF $args > /tmp/diff_ref.txt 2>&1
	$PING $args > /tmp/diff_ft.txt 2>&1

	local ref_has_ts ft_has_ts
	ref_has_ts=$(grep -c 'TS:' /tmp/diff_ref.txt)
	ft_has_ts=$(grep -c 'TS:' /tmp/diff_ft.txt)

	if [ "$ft_has_ts" -ge 1 ] && [ "$ref_has_ts" -ge 1 ]; then
		norm < /tmp/diff_ref.txt | grep -v '^\s*$' | grep -v 'TS:' | grep -v 'ms$' | grep -v '^\s' > /tmp/diff_ref_norm.txt
		norm < /tmp/diff_ft.txt  | grep -v '^\s*$' | grep -v 'TS:' | grep -v 'ms$' | grep -v '^\s' > /tmp/diff_ft_norm.txt
		if diff -q /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt > /dev/null 2>&1; then
			echo -e "  ${GRN}[OK]${RST}   $name (TS present; entry count may vary)"
			((PASS++))
		else
			echo -e "  ${RED}[FAIL]${RST} $name"
			diff /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt | head -8 | sed 's/^/         /'
			((FAIL++))
		fi
	elif [ "$ft_has_ts" -eq 0 ] && [ "$ref_has_ts" -eq 0 ]; then
		echo -e "  ${GRN}[OK]${RST}   $name (no TS in either — packet loss)"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name (ref TS=$ref_has_ts, ft TS=$ft_has_ts)"
		((FAIL++))
	fi
}

run_diff_flood() {
	local name="$1"; shift
	local args="$*"

	((TOTAL++))

	$REF $args > /tmp/diff_ref.txt 2>&1
	$PING $args > /tmp/diff_ft.txt 2>&1

	norm_flood < /tmp/diff_ref.txt > /tmp/diff_ref_norm.txt
	norm_flood < /tmp/diff_ft.txt  > /tmp/diff_ft_norm.txt

	if diff -q /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt > /dev/null 2>&1; then
		echo -e "  ${GRN}[OK]${RST}   $name"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name"
		diff /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt | head -8 | sed 's/^/         /'
		((FAIL++))
	fi
}

run_diff_sigint() {
	local name="$1"
	local secs="$2"; shift 2
	local args="$*"

	((TOTAL++))

	timeout -s INT "$secs" $REF $args > /tmp/diff_ref.txt 2>&1
	timeout -s INT "$secs" $PING $args > /tmp/diff_ft.txt 2>&1

	norm < /tmp/diff_ref.txt > /tmp/diff_ref_norm.txt
	norm < /tmp/diff_ft.txt  > /tmp/diff_ft_norm.txt

	if diff -q /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt > /dev/null 2>&1; then
		echo -e "  ${GRN}[OK]${RST}   $name"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name"
		diff /tmp/diff_ref_norm.txt /tmp/diff_ft_norm.txt | head -8 | sed 's/^/         /'
		((FAIL++))
	fi
}

run_diff_exit() {
	local name="$1"; shift
	local args="$*"

	((TOTAL++))

	$REF $args > /dev/null 2>&1
	local ref_ret=$?
	$PING $args > /dev/null 2>&1
	local ft_ret=$?

	if [ $ref_ret -ne 0 ] && [ $ft_ret -ne 0 ]; then
		echo -e "  ${GRN}[OK]${RST}   $name (both non-zero: ref=$ref_ret ft=$ft_ret)"
		((PASS++))
	elif [ $ref_ret -eq 0 ] && [ $ft_ret -eq 0 ]; then
		echo -e "  ${GRN}[OK]${RST}   $name (both zero)"
		((PASS++))
	else
		echo -e "  ${RED}[FAIL]${RST} $name (ref=$ref_ret ft=$ft_ret)"
		((FAIL++))
	fi
}

skip_test() {
	local name="$1"
	((TOTAL++))
	((SKIP++))
	echo -e "  ${YEL}[SKIP]${RST} $name"
}

has_network() {
	ping -c 1 -w 2 8.8.8.8 > /dev/null 2>&1
}

# ──────────────────────────────────────────────
echo -e "${BLU}=== MANDATORY: diff tests ===${RST}"
# ──────────────────────────────────────────────

run_diff "basic -c 3 (127.0.0.1)" \
	-c 3 127.0.0.1

run_diff_verbose "verbose -v -c 2 (127.0.0.1)" \
	-v -c 2 127.0.0.1

run_diff_sigint "Ctrl+C after 4s (127.0.0.1)" 4 \
	127.0.0.1

if has_network; then
	run_diff_hostname "hostname -c 2 (google.com)" \
		-c 2 google.com

	run_diff "TTL exceeded --ttl 1 -c 2 (8.8.8.8)" \
		--ttl 1 -c 2 8.8.8.8

	run_diff_verbose "verbose TTL exceeded -v --ttl 1 -c 2 (8.8.8.8)" \
		-v --ttl 1 -c 2 8.8.8.8
else
	skip_test "hostname (no network)"
	skip_test "TTL exceeded (no network)"
	skip_test "verbose TTL exceeded (no network)"
fi

# ──────────────────────────────────────────────
echo ""
echo -e "${BLU}=== BONUS: diff tests ===${RST}"
# ──────────────────────────────────────────────

run_diff "-c 1 (127.0.0.1)" \
	-c 1 127.0.0.1

run_diff "-s 0 -c 1 (127.0.0.1)" \
	-s 0 -c 1 127.0.0.1

run_diff "-s 56 -c 1 (127.0.0.1)" \
	-s 56 -c 1 127.0.0.1

run_diff "-s 1000 -c 1 (127.0.0.1)" \
	-s 1000 -c 1 127.0.0.1

run_diff "-T 0 -c 1 (127.0.0.1)" \
	-T 0 -c 1 127.0.0.1

run_diff "-T 16 -c 1 (127.0.0.1)" \
	-T 16 -c 1 127.0.0.1

run_diff "-p ff -c 1 (127.0.0.1)" \
	-p ff -c 1 127.0.0.1

run_diff "-r -c 1 (127.0.0.1)" \
	-r -c 1 127.0.0.1

run_diff "-w 2 -c 5 (127.0.0.1)" \
	-w 2 -c 5 127.0.0.1

run_diff_ts "--ip-timestamp tsonly -c 1 (127.0.0.1)" \
	--ip-timestamp tsonly -c 1 127.0.0.1

run_diff_ts "--ip-timestamp tsaddr -c 1 (127.0.0.1)" \
	--ip-timestamp tsaddr -c 1 127.0.0.1

run_diff_flood "-f -c 50 (flood, 127.0.0.1)" \
	-f -c 50 127.0.0.1

run_diff "-l 5 -c 5 (preload, 127.0.0.1)" \
	-l 5 -c 5 127.0.0.1

if has_network; then
	run_diff "--ttl 64 -c 1 (8.8.8.8)" \
		--ttl 64 -c 1 8.8.8.8

	run_diff_hostname "-n -c 1 (google.com)" \
		-n -c 1 google.com
else
	skip_test "--ttl 64 (no network)"
	skip_test "-n google.com (no network)"
fi

# ──────────────────────────────────────────────
echo ""
echo -e "${BLU}=== NEGATIVE: exit codes ===${RST}"
# ──────────────────────────────────────────────

run_diff_exit "no args" ""
run_diff_exit "unknown host" "no.such.host.invalid"
run_diff_exit "invalid option -Z" "-Z"

if has_network; then
	run_diff_exit "unreachable -c 1 -w 2 (192.0.2.1)" \
		"-c 1 -w 2 192.0.2.1"
else
	skip_test "unreachable host (no network)"
fi

# ──────────────────────────────────────────────
echo ""
echo -e "${BLU}═══════════════════════════════════════${RST}"
echo -e "  Total: $TOTAL"
echo -e "  ${GRN}Pass:  $PASS${RST}"
echo -e "  ${RED}Fail:  $FAIL${RST}"
echo -e "  ${YEL}Skip:  $SKIP${RST}"
echo -e "${BLU}═══════════════════════════════════════${RST}"

[ $FAIL -eq 0 ] && exit 0 || exit 1
