#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail
source_root=${1:-$(cd -- "$(dirname -- "$0")/../../../.." && pwd)}
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
awk '
 /^int orig_impu_has_contact\(/ { capture=1 }
 capture { print }
 capture && /^}/ { exit }
' "$source_root/src/modules/ims_registrar_scscf/lookup.c" > "$test_dir/cx_admission.inc"
awk '
 /^#define VALID_CONTACT/ { capture=1 }
 capture && /^$/ { exit }
 capture { print }
' "$source_root/src/modules/ims_usrloc_scscf/usrloc.h" > "$test_dir/cx_valid_contact.inc"
test -s "$test_dir/cx_admission.inc"
test -s "$test_dir/cx_valid_contact.inc"
cc -std=gnu99 -ffreestanding -Wall -Wextra -Wno-unused-function \
    -I"$test_dir" "$source_root/src/modules/ims_usrloc_scscf/test/cx_profile_test.c" \
    -o "$test_dir/cx-profile-test"
"$test_dir/cx-profile-test"
printf '%s\n' 'PASS: Cx profile replacement, RTR cleanup and originating admission'
