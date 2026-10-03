#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

KAMAILIO_BIN="${DIR}/../../../../src/kamailio"
MODULES_DIR="${DIR}/../../../../src/modules"

if [ ! -x "$KAMAILIO_BIN" ]; then
    KAMAILIO_BIN="kamailio"
fi

# Configurable Tarantool connection parameters via environment
TNT_ADDR="${TNT_ADDR:-127.0.0.1}"
TNT_PORT="${TNT_PORT:-3301}"
TNT_USER="${TNT_USER:-rtpe_user}"
TNT_PASS="${TNT_PASS:-rtpe_secret_password}"

if [ -n "$TNT_USER" ]; then
    TNT_AUTH=";user=${TNT_USER};pass=${TNT_PASS}"
else
    TNT_AUTH=""
fi

echo "==============================================================="
echo "       Kamailio ndb_tarantool Module In-Tree Test Suite       "
echo "==============================================================="
echo "Target Tarantool instance: ${TNT_ADDR}:${TNT_PORT} (user: ${TNT_USER:-none})"

# 1. Check syntax
echo -n "[1/3] Validating Kamailio config syntax (kamailio -c)... "
"$KAMAILIO_BIN" -c -L "$MODULES_DIR" -w "$DIR" \
    --substdef="!TNT_ADDR!${TNT_ADDR}!g" \
    --substdef="!TNT_PORT!${TNT_PORT}!g" \
    --substdef="!TNT_AUTH!${TNT_AUTH}!g" \
    --substdef="!TNT_LUA_FILE!${DIR}/test_kemi.lua!g" \
    -f "${DIR}/test.cfg" > /dev/null 2>&1
echo "OK"

# Function to run test with config and stream log
run_kam_test() {
    local cfg_file="$1"
    local title="$2"
    local log_file="/tmp/kam_mod_test.log"

    echo ""
    echo "--- ${title} ---"
    killall -9 kamailio 2>/dev/null || true
    sleep 0.5
    rm -f "$log_file"

    "$KAMAILIO_BIN" -L "$MODULES_DIR" -w "$DIR" \
        --substdef="!TNT_ADDR!${TNT_ADDR}!g" \
        --substdef="!TNT_PORT!${TNT_PORT}!g" \
        --substdef="!TNT_AUTH!${TNT_AUTH}!g" \
        --substdef="!TNT_LUA_FILE!${DIR}/test_kemi.lua!g" \
        -f "$cfg_file" -E -e > "$log_file" 2>&1 &
    local kam_pid=$!
    sleep 1.5

    # Send SIP OPTIONS and record real return code
    set +e
    python3 -c "
import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(3.0)
s.bind(('127.0.0.1', 0))
port = s.getsockname()[1]
msg = (
    f'OPTIONS sip:test@127.0.0.1:5080 SIP/2.0\r\n'
    f'Via: SIP/2.0/UDP 127.0.0.1:{port};branch=z9hG4bK-tnttest-1\r\n'
    f'From: <sip:test@example.com>;tag=tag1\r\n'
    f'To: <sip:test@example.com>\r\n'
    f'Call-ID: call-mod-test-1\r\n'
    f'CSeq: 1 OPTIONS\r\n'
    f'Content-Length: 0\r\n\r\n'
).encode('utf-8')
s.sendto(msg, ('127.0.0.1', 5080))
try:
    resp, _ = s.recvfrom(4096)
    if b'200 OK' in resp:
        sys.exit(0)
    else:
        sys.exit(2)
except Exception:
    sys.exit(1)
finally:
    s.close()
" > /dev/null 2>&1
    local rc=$?
    set -e

    killall -9 kamailio 2>/dev/null || true
    wait $kam_pid 2>/dev/null || true
    sleep 0.5

    if [ -f "$log_file" ]; then
        grep -E 'TEST |KEMI |FAILED|PASSED' "$log_file" || true
    fi

    if [ $rc -eq 0 ]; then
        echo "--> Result: PASSED (SIP 200 OK)"
        return 0
    else
        echo "--> Result: FAILED (rc=$rc)"
        echo "--- Full Kamailio Logs ---"
        cat "$log_file" | tail -n 35
        return 1
    fi
}

# 2. Run Native CFG Tests (19 tests)
run_kam_test "${DIR}/test.cfg" "[2/3] Running 19 Native CFG & Advanced Regression Tests"

# 3. Run KEMI Lua Tests (4 tests)
run_kam_test "${DIR}/test_kemi.cfg" "[3/3] Running 4 KEMI Lua Routing Framework Tests"

echo ""
echo "==============================================================="
echo "       ALL 23 IN-TREE ndb_tarantool TESTS PASSED (100%)        "
echo "==============================================================="
