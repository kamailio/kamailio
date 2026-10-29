#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail
source_root=${1:-$(cd -- "$(dirname -- "$0")/../../../.." && pwd)}
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
source_file="$source_root/src/modules/ims_registrar_scscf/registrar_notify.c"
awk '
 /^int subscribe_reply\(/ { capture=1 }
 capture { print }
 capture && /^}/ { exit }
' "$source_file" > "$test_dir/subscribe_reply_body.inc"
awk -v output="$test_dir" '
 /^[ \t]*subscribe_reply\(/ { capture=1; buffer="" }
 capture {
     buffer=buffer $0 "\n"
     if($0 ~ /;/) {
         if(buffer ~ /MSG_REG_SUBSCRIBE_OK/)
             printf "%s", buffer > (output "/subscribe_reply_call.inc")
         if(buffer ~ /MSG_REG_UNSUBSCRIBE_OK/)
             printf "%s", buffer > (output "/unsubscribe_reply_call.inc")
         capture=0
     }
 }
' "$source_file"
test -s "$test_dir/subscribe_reply_body.inc"
test -s "$test_dir/subscribe_reply_call.inc"
test -s "$test_dir/unsubscribe_reply_call.inc"
cc -std=gnu99 -ffreestanding -Wall -Wextra -Wno-unused-function \
    -I"$test_dir" "$source_root/src/modules/ims_registrar_scscf/test/subscribe_contact_test.c" \
    -o "$test_dir/subscribe-contact-test"
"$test_dir/subscribe-contact-test"
printf '%s\n' 'PASS: subscribe/unsubscribe responses use the notifier Contact'
