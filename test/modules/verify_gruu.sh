#!/usr/bin/env bash
set -euo pipefail
TOPDIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$TOPDIR"

echo "=== Layer 1: usrloc unit tests ==="
make -C test/modules/ims_usrloc_pcscf clean test 2>/dev/null || {
  cd test/modules/ims_usrloc_pcscf
  for t in test_db_layout test_pcontact_serialize test_pcontact_index test_temp_gruu_lru test_impu_match; do
    make $t && ./$t
  done
  cd "$TOPDIR"
}

echo "=== Layer 1: registrar unit tests ==="
make -C test/modules/ims_registrar_pcscf clean test

echo "=== Layer 2: SQLite integration ==="
bash test/modules/ims_usrloc_pcscf/integrate_sqlite.sh

echo "=== Layer 3: E2E SIPp ==="
bash test/modules/ims_registrar_pcscf/integrate_gruu.sh

echo "=== Isolation audit ==="
if git diff master -- '*.c' '*.h' '*.xml' | grep -E \
    'reg_event_|emergency_reg|access_network_info|reg_id|restored|pcscf_route_mt|reginfo_state|emergency\.c|mt_route'; then
    echo "FAIL: out-of-scope symbols in diff"
    exit 1
fi

echo "ALL GRUU VERIFICATION PASSED"
