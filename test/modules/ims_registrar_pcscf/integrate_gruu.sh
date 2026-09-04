#!/usr/bin/env bash
set -euo pipefail

TOPDIR="$(cd "$(dirname "$0")/../../.." && pwd)"
TESTDIR="$TOPDIR/test/modules/ims_registrar_pcscf"
WORKDIR="${TMPDIR:-/tmp}/gruu_e2e_$$"
DBFILE="$WORKDIR/pcscf.db"
CFG="$WORKDIR/kamailio_gruu_e2e.cfg"
RUN_DIR="$WORKDIR/run"
KAMLOG="$WORKDIR/kamailio.log"
KAMBIN="$TOPDIR/src/kamailio"
PASS=0
FAIL=0

pass() { echo "PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "FAIL: $1"; FAIL=$((FAIL + 1)); }

cleanup() {
	if [ -n "${KPID:-}" ] && kill -0 "$KPID" 2>/dev/null; then
		kill "$KPID" 2>/dev/null || true
		wait "$KPID" 2>/dev/null || true
	fi
	if [ "${FAIL:-0}" -eq 0 ]; then
		rm -rf "$WORKDIR"
	else
		echo "DEBUG: retaining workdir $WORKDIR"
	fi
}
trap cleanup EXIT

echo "=== Build prerequisites ==="
if [ ! -x "$KAMBIN" ]; then
	echo "ERROR: kamailio binary missing at $KAMBIN"
	exit 1
fi

if make -C "$TOPDIR/src/modules/ims_usrloc_pcscf" ims_usrloc_pcscf.so >/dev/null; then
	pass "ims_usrloc_pcscf module builds"
else
	fail "ims_usrloc_pcscf module build failed"
fi

if make -C "$TOPDIR/src/modules/ims_registrar_pcscf" ims_registrar_pcscf.so >/dev/null; then
	pass "ims_registrar_pcscf module builds"
else
	fail "ims_registrar_pcscf module build failed"
fi

if [ "$FAIL" -ne 0 ]; then
	echo "=== Integration summary: $PASS passed, $FAIL failed ==="
	exit 1
fi

echo "=== SQLite seed ==="
mkdir -p "$WORKDIR" "$RUN_DIR"
sqlite3 "$DBFILE" < "$TOPDIR/utils/kamctl/db_sqlite/standard-create.sql"
sqlite3 "$DBFILE" < "$TOPDIR/utils/kamctl/db_sqlite/ims_usrloc_pcscf-create.sql"
sqlite3 "$DBFILE" < "$TOPDIR/test/modules/ims_usrloc_pcscf/sql/seed_gruu.sql"
sqlite3 "$DBFILE" \
	"DROP TABLE IF EXISTS location; CREATE TABLE location AS SELECT * FROM pcscf_location;"
sqlite3 "$DBFILE" \
	"UPDATE location SET pub_gruu='urn:uuid:3333-4444', temp_gruu='urn:uuid:3333-4444' WHERE aor='sip:gruu-user@ims.local';"

seed_pub_gruu="$(sqlite3 "$DBFILE" \
	"SELECT pub_gruu FROM pcscf_location WHERE aor='sip:gruu-user@ims.local';")"
if [ "$seed_pub_gruu" = "sip:gruu-user@ims.local;gr=urn:uuid:3333-4444" ]; then
	pass "seed C2 public GRUU present"
else
	fail "seed C2 public GRUU mismatch: $seed_pub_gruu"
fi

echo "=== Start Kamailio ==="
sed -e "s|REPLACE_DB_PATH|$DBFILE|g" \
	-e "s|REPLACE_MODULE_PATH|$TOPDIR/src/modules|g" \
	"$TESTDIR/kamailio_gruu_e2e.cfg" > "$CFG"

"$KAMBIN" -f "$CFG" -w "$WORKDIR" -Y "$RUN_DIR" -E -e -m 64 -M 64 >"$KAMLOG" 2>&1 &
KPID=$!

started=0
for _i in $(seq 1 50); do
	if ! kill -0 "$KPID" 2>/dev/null; then
		break
	fi
	if [ -S "$RUN_DIR/kamailio_ctl" ]; then
		started=1
		break
	fi
	sleep 0.2
done

if [ "$started" -eq 1 ]; then
	pass "kamailio started with ctl socket"
elif kill -0 "$KPID" 2>/dev/null; then
	pass "kamailio process running (no ctl socket)"
else
	fail "kamailio failed to start"
	echo "--- kamailio log tail ---"
	if [ -f "$KAMLOG" ]; then
		sed -n '1,120p' "$KAMLOG"
	fi
fi

if [ "$FAIL" -ne 0 ]; then
	echo "=== Integration summary: $PASS passed, $FAIL failed ==="
	exit 1
fi

echo "=== SIPp GRUU INVITE ==="
if ! command -v sipp >/dev/null 2>&1; then
	echo "SKIP: sipp not installed (startup validated)"
	echo "=== Integration summary: $PASS passed, $FAIL failed ==="
	exit 0
fi

if sipp -sf "$TESTDIR/sipp/invite_gruu.xml" \
	-i 127.0.0.1 -p 15081 -m 1 -timeout 15s 127.0.0.1:15080 >/dev/null; then
	pass "SIPp INVITE with gr= received 200 OK"
else
	fail "SIPp INVITE with gr= did not receive 200 OK"
	echo "--- kamailio log tail ---"
	sed -n '1,180p' "$KAMLOG"
fi

echo "=== Integration summary: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
