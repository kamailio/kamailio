#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail
source_root=${1:-$(cd -- "$(dirname -- "$0")/../../../.." && pwd)}
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
awk '
 /^[ \t]*if\(ps->code/ && $0 !~ /lexpire/ { capture=1 }
 capture {
     print
     opened=gsub(/\{/, "{")
     closed=gsub(/\}/, "}")
     depth+=opened-closed
     if(opened) started=1
     if(started && depth==0) exit
 }
' "$source_root/src/modules/pua/send_subscribe.c" > "$test_dir/subscribe_error_branch.inc"
test -s "$test_dir/subscribe_error_branch.inc"
cc -std=gnu99 -ffreestanding -Wall -Wextra -Wno-unused-function \
    -I"$test_dir" "$source_root/src/modules/pua/test/subscribe_error_test.c" \
    -o "$test_dir/subscribe-error-test"
"$test_dir/subscribe-error-test"
printf '%s\n' 'PASS: PUA failed-unsubscribe and active-retry scenarios'
