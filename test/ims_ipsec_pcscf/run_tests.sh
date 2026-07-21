#!/bin/sh
#
# Runner script for ims_ipsec_pcscf 3GPP Rel-18 unit tests
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Building ims_ipsec_pcscf 3GPP Rel-18 Unit Tests ==="
make clean
make all

echo ""
echo "=== Running ims_ipsec_pcscf 3GPP Rel-18 Unit Tests ==="
make test

echo ""
echo "=== All 3GPP Rel-18 Tests Passed Successfully ==="
