#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail
source_root=${1:-$(cd -- "$(dirname -- "$0")/../../../.." && pwd)}
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
source_file="$source_root/src/modules/ims_registrar_scscf/registrar_notify.c"
awk '
 /^static void cx_notify\(/ { capture=1 }
 capture { print }
 capture && /^}/ { exit }
' "$source_root/src/modules/ims_registrar_scscf/cxdx_callbacks.c" > "$test_dir/rtr_notify_callback.inc"
awk '
 /^static void process_xml_for_explit_dereg_contact\(/ { capture=1 }
 capture { print }
 capture && /^}/ { exit }
' "$source_file" > "$test_dir/rtr_xml_builder.inc"
awk '
 /^static str (r_terminated|r_expired|contact_s_q|contact_e|uri_s|uri_e) =/ { capture=1 }
 capture { print }
 capture && /;/ { capture=0 }
' "$source_file" > "$test_dir/rtr_xml_constants.inc"
test -s "$test_dir/rtr_notify_callback.inc"
test -s "$test_dir/rtr_xml_builder.inc"
test -s "$test_dir/rtr_xml_constants.inc"
cc -std=gnu99 -ffreestanding -Wall -Wextra \
    -I"$test_dir" "$source_root/src/modules/ims_registrar_scscf/test/rtr_notify_test.c" \
    -o "$test_dir/rtr-notify-test"
"$test_dir/rtr-notify-test"
printf '%s\n' 'PASS: RTR retains terminated contact URIs in notification XML'
