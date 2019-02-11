#!/bin/sh
# checks sdpops module function sdp_remove_line_by_prefix() via Lua
#
# Copyright (C) 2016 mslehto@iki.fi
#
# This file is part of Kamailio, a free SIP server.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

. include/common
. include/require.sh

CFGFILE=61.cfg
TMPFILE=$(mktemp -t kamailio-test.XXXXXXXXXX)
SIPSAKOPTS="-H localhost -s sip:127.0.0.1:5060 -v"

end_test() {
	kill_kamailio
	rm ${TMPFILE}
	exit ${ret}
}

if ! (check_sipsak && check_kamailio && check_module "sdpops" && check_module "app_lua"); then
	exit 0
fi

${BIN} -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f ${CFGFILE} > /dev/null
ret=$?

sleep 1
if [ "${ret}" -ne 0 ] ; then
	end_test
fi

# manipulate whole SDP
FILE="61-message-sdp.sip"
TESTCASE="61-test0"
TOTALBEFORE=$(awk '/^v=0/,/^$/ {total++; if ($0 ~ /^a=rtcp/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${FILE})
OTHERBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f1)
PREFIXBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f2)
sipsak ${SIPSAKOPTS} -f ${FILE} --headers "X-Case: ${TESTCASE}\n" > ${TMPFILE}
ret=$?
if [ "${ret}" -ne 0 ] ; then
	echo "sipsak returned ${ret}, aborting"
	cat ${TMPFILE}
else
	TOTALAFTER=$(awk '/^v=0/,/^$/ {total++; if ($0 ~ /^a=rtcp:/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${TMPFILE})
	OTHERAFTER=$(echo ${TOTALAFTER}|cut -d+ -f1)
	PREFIXAFTER=$(echo ${TOTALAFTER}|cut -d+ -f2)
	if [ ${PREFIXAFTER} -eq 0 ]; then
		ret=0
	else
		ret=1
		echo "test ${TESTCASE} failed"
		echo "found ${PREFIXAFTER} lines with prefix \"a=rtcp\", was expecting 0"
		echo "found ${OTHERAFTER} other lines (was ${OTHERBEFORE} before)"
		end_test
	fi
fi

# manipulate m=audio only
FILE="61-message-sdp.sip"
TESTCASE="61-test1"
TOTALBEFORE=$(awk '/^m=audio/,/^m=video/ {total++; if ($0 ~ /^a=rtcp/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${FILE})
OTHERBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f1)
PREFIXBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f2)
sipsak ${SIPSAKOPTS} -f ${FILE} --headers "X-Case: ${TESTCASE}\n" > ${TMPFILE}
ret=$?
if [ "${ret}" -ne 0 ] ; then
	echo "sipsak returned ${ret}, aborting"
	cat ${TMPFILE}
else
	TOTALAFTER=$(awk '/^m=audio/,/^m=video/ {total++; if ($0 ~ /^a=rtcp:/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${TMPFILE})
	OTHERAFTER=$(echo ${TOTALAFTER}|cut -d+ -f1)
	PREFIXAFTER=$(echo ${TOTALAFTER}|cut -d+ -f2)
	if [ ${PREFIXAFTER} -eq 0 ]; then
		ret=0
	else
		ret=1
		echo "test ${TESTCASE} failed"
		echo "found ${PREFIXAFTER} lines with prefix \"a=rtcp\", was expecting 0"
		echo "found ${OTHERAFTER} other lines (was ${OTHERBEFORE} before)"
		end_test
	fi
fi

# manipulate m=video only
FILE="61-message-sdp.sip"
TESTCASE="61-test2"
TOTALBEFORE=$(awk '/^m=video/,/^$/ {total++; if ($0 ~ /^a=rtcp/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${FILE})
OTHERBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f1)
PREFIXBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f2)
sipsak ${SIPSAKOPTS} -f ${FILE} --headers "X-Case: ${TESTCASE}\n" > ${TMPFILE}
ret=$?
if [ "${ret}" -ne 0 ] ; then
	echo "sipsak returned ${ret}, aborting"
	cat ${TMPFILE}
else
	TOTALAFTER=$(awk '/^m=video/,/^$/ {total++; if ($0 ~ /^a=rtcp:/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${TMPFILE})
	OTHERAFTER=$(echo ${TOTALAFTER}|cut -d+ -f1)
	PREFIXAFTER=$(echo ${TOTALAFTER}|cut -d+ -f2)
	if [ ${PREFIXAFTER} -eq 0 ]; then
		ret=0
	else
		ret=1
		echo "test ${TESTCASE} failed"
		echo "found ${PREFIXAFTER} lines with prefix \"a=rtcp\", was expecting 0"
		echo "found ${OTHERAFTER} other lines (was ${OTHERBEFORE} before)"
		end_test
	fi
fi

end_test

