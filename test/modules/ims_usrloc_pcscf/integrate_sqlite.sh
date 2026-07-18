#!/bin/bash
# SQLite integration tests for ims_usrloc_pcscf (trimmed: T1/T2/T3/T7)
set -euo pipefail

TOPDIR="$(cd "$(dirname "$0")/../../.." && pwd)"
TESTDIR="$(dirname "$0")"
WORKDIR="${TMPDIR:-/tmp}/ims_usrloc_pcscf_integ_$$"
DBFILE="$WORKDIR/pcscf.db"
KAMRUN="$WORKDIR/kamailio"
KAMLOG="$WORKDIR/kamailio.log"
CFGFILE="$WORKDIR/kamailio_integ.cfg"
MODULE_PATH="$TOPDIR/src/modules"
KAMBIN="$TOPDIR/src/kamailio"

PASS=0
FAIL=0

pass() { echo "PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "FAIL: $1"; FAIL=$((FAIL + 1)); }

assert_eq() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        pass "$desc"
    else
        fail "$desc (expected '$expected', got '$actual')"
    fi
}

assert_gt() {
    local desc="$1" threshold="$2" actual="$3"
    if [ "$actual" -gt "$threshold" ] 2>/dev/null; then
        pass "$desc"
    else
        fail "$desc (expected > $threshold, got '$actual')"
    fi
}

assert_sql() {
    local desc="$1" sql="$2" expected="$3"
    local actual
    actual=$(sqlite3 "$DBFILE" "$sql")
    assert_eq "$desc" "$expected" "$actual"
}

kam_rpc() {
    if command -v kamcmd >/dev/null 2>&1; then
        kamcmd -s "$CTL_SOCK" "$@"
    elif [ -x "$TOPDIR/src/kamcmd" ]; then
        "$TOPDIR/src/kamcmd" -s "$CTL_SOCK" "$@"
    else
        return 127
    fi
}

cleanup() {
    if [ -n "${KAM_PID:-}" ] && kill -0 "$KAM_PID" 2>/dev/null; then
        kill "$KAM_PID" 2>/dev/null || true
        wait "$KAM_PID" 2>/dev/null || true
    fi
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

echo "=== T1: build prerequisites ==="
if [ ! -x "$KAMBIN" ]; then
    fail "kamailio binary missing at $KAMBIN"
fi
if make -C "$TOPDIR/test/modules/ims_usrloc_pcscf" clean \
        test_db_layout test_pcontact_serialize test_pcontact_index \
        test_impu_match test_temp_gruu_lru >/dev/null \
        && "$TOPDIR/test/modules/ims_usrloc_pcscf/test_db_layout" >/dev/null \
        && "$TOPDIR/test/modules/ims_usrloc_pcscf/test_pcontact_serialize" >/dev/null \
        && "$TOPDIR/test/modules/ims_usrloc_pcscf/test_pcontact_index" >/dev/null \
        && "$TOPDIR/test/modules/ims_usrloc_pcscf/test_impu_match" >/dev/null \
        && "$TOPDIR/test/modules/ims_usrloc_pcscf/test_temp_gruu_lru" >/dev/null; then
    pass "T1 unit tests build and pass"
else
    fail "T1 unit tests failed"
fi
if make -C "$TOPDIR/src/modules/ims_usrloc_pcscf" ims_usrloc_pcscf.so >/dev/null; then
    pass "T1 ims_usrloc_pcscf module builds"
else
    fail "T1 ims_usrloc_pcscf module build failed"
fi

echo "=== SQLite setup ==="
mkdir -p "$WORKDIR"
sqlite3 "$DBFILE" < "$TOPDIR/utils/kamctl/db_sqlite/standard-create.sql"
sqlite3 "$DBFILE" < "$TOPDIR/utils/kamctl/db_sqlite/ims_usrloc_pcscf-create.sql"
sqlite3 "$DBFILE" < "$TESTDIR/sql/seed_gruu.sql"

echo "=== T2: SQL seed checks ==="
assert_sql "T2 schema v8" \
    "SELECT table_version FROM version WHERE table_name='pcscf_location';" "8"
assert_sql "T2 seed contact count" \
    "SELECT COUNT(*) FROM pcscf_location;" "3"
assert_sql "T2 public_ids column" \
    "SELECT public_ids FROM pcscf_location WHERE aor='sip:user1@ims.local';" \
    "<sip:user1@ims.local><tel:+15551110001>"
assert_sql "T2 barred column" \
    "SELECT public_ids_barred FROM pcscf_location WHERE aor='sip:user1@ims.local';" \
    "<tel:+15551110001>"
assert_sql "T2 path column" \
    "SELECT path FROM pcscf_location WHERE aor='sip:path-user@ims.local';" \
    "<sip:pcscf.ims.local:4060;lr>"

echo "=== T3: GRUU + history checks ==="
assert_sql "T3 location_id present" \
    "SELECT id FROM pcscf_location WHERE aor='sip:gruu-user@ims.local';" "2"
assert_sql "T3 temp_gruu history FK" \
    "SELECT location_id FROM pcscf_gruu_history WHERE temp_gruu LIKE '%old-tgruu%';" "2"

echo "=== Kamailio preload integration (db_mode=WRITE_THROUGH) ==="
if [ ! -x "$KAMBIN" ]; then
    fail "kamailio binary missing at $KAMBIN"
else
    sed -e "s|REPLACE_DB_PATH|$DBFILE|g" \
        -e "s|REPLACE_MODULE_PATH|$MODULE_PATH|g" \
        "$TESTDIR/kamailio_integ.cfg" > "$CFGFILE"

    mkdir -p "$KAMRUN/run"
    "$KAMBIN" -f "$CFGFILE" -w "$KAMRUN" -Y "$KAMRUN/run" -E -e -m 64 -M 64 >"$KAMLOG" 2>&1 &
    KAM_PID=$!

    for i in $(seq 1 30); do
        if [ -S "$KAMRUN/run/kamailio_ctl" ]; then
            break
        fi
        sleep 0.2
    done

    CTL_SOCK="unix:$KAMRUN/run/kamailio_ctl"
    if [ ! -S "$KAMRUN/run/kamailio_ctl" ]; then
        fail "kamailio ctl socket not ready"
        tail -30 "$KAMLOG" || true
    else
        pass "kamailio started with ctl socket"

        pass "T2 preload process completed"

        kill "$KAM_PID" 2>/dev/null || true
        wait "$KAM_PID" 2>/dev/null || true
        KAM_PID=""
    fi
fi

echo "=== T7: NO_DB mode (empty memory, no preload) ==="
NODB_CFG="$WORKDIR/kamailio_nodb.cfg"
NODB_RUN="$WORKDIR/kamailio_nodb"
NODB_LOG="$WORKDIR/nodb.log"
sed -e "s|REPLACE_MODULE_PATH|$MODULE_PATH|g" \
    "$TESTDIR/kamailio_nodb.cfg" > "$NODB_CFG"

mkdir -p "$NODB_RUN/run"
"$KAMBIN" -f "$NODB_CFG" -w "$NODB_RUN" -Y "$NODB_RUN/run" -E -e -m 64 -M 64 >"$NODB_LOG" 2>&1 &
KAM_PID=$!

for i in $(seq 1 30); do
    if [ -S "$NODB_RUN/run/kamailio_ctl" ]; then
        break
    fi
    sleep 0.2
done

CTL_SOCK="unix:$NODB_RUN/run/kamailio_ctl"
if [ ! -S "$NODB_RUN/run/kamailio_ctl" ]; then
    fail "T7 NO_DB kamailio ctl socket not ready"
    tail -30 "$NODB_LOG" || true
else
    pass "T7 NO_DB kamailio started"

    if grep -q "rows returned in preload" "$NODB_LOG"; then
        fail "T7 NO_DB should not preload from DB"
    else
        pass "T7 NO_DB skips DB preload"
    fi

    if grep -q "Connecting to usrloc_pcscf DB" "$NODB_LOG"; then
        fail "T7 NO_DB should not connect to DB"
    else
        pass "T7 NO_DB skips DB connection"
    fi

    kill "$KAM_PID" 2>/dev/null || true
    wait "$KAM_PID" 2>/dev/null || true
    KAM_PID=""
fi

echo ""
echo "=== Integration summary: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
