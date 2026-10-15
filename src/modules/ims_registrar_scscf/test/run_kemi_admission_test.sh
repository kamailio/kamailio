#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail
source_root=${1:-$(cd -- "$(dirname -- "$0")/../../../.." && pwd)}
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
source_file="$source_root/src/modules/ims_registrar_scscf/kemi.c"
awk '
 /^static int ki_orig_impu_has_contact\(/ { capture=1 }
 capture { print }
 capture && /^}/ { exit }
' "$source_file" > "$test_dir/kemi_wrapper.inc"
awk '
 /^static sr_kemi_t / { capture=1 }
 capture { print }
' "$source_file" > "$test_dir/kemi_registration.inc"
test -s "$test_dir/kemi_wrapper.inc"
test -s "$test_dir/kemi_registration.inc"
cc -std=gnu99 -ffreestanding -Wall -Wextra -Wno-unused-function \
    -I"$test_dir" "$source_root/src/modules/ims_registrar_scscf/test/kemi_admission_test.c" \
    -o "$test_dir/kemi-admission-test"
"$test_dir/kemi-admission-test"
printf '%s\n' 'PASS: KEMI domain lookup, native delegation and export registration'
