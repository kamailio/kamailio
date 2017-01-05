#!/bin/sh
# checks sdpops module function remove_line_by_prefix()
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

CFGFILE=60.cfg
TMPFILE=$(mktemp -t kamailio-test.XXXXXXXXXX)
SIPSAKOPTS="-H localhost -s sip:127.0.0.1 -v"

if ! (check_sipsak && check_kamailio && check_module "sdpops"); then
	exit 0
fi

${BIN} -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE  -w . -f ${CFGFILE} > /dev/null
ret=$?

sleep 1
if [ "${ret}" -ne 0 ] ; then
	echo "start fail"
	kill_kamailio
	exit ${ret}
fi

# Borken SDP should give 500 response
FILE="60-message-sdp0.sip"
sipsak ${SIPSAKOPTS} -f ${FILE} > ${TMPFILE}
ret=$?
if [ "${ret}" -eq 1 ] ; then
	ret=0
else
	echo "invalid SDP not rejected"
	ret=1
	exit ${ret}
fi

# Kamailio replies back with modified SDP
for i in 1 2 3 4 5 6 7; do
	FILE="60-message-sdp${i}.sip"
	TOTALBEFORE=$(awk '/^v=0/,/^$/ {total++; if ($0 ~ /^a=X-cap/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${FILE})
	OTHERBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f1)

	sipsak ${SIPSAKOPTS} -f ${FILE} > ${TMPFILE}
	ret=$?
	if [ "${ret}" -eq 0 ] ; then
		TOTALAFTER=$(awk '/^v=0/,/^$/ {total++; if ($0 ~ /^a=X-cap/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${TMPFILE})
		OTHERAFTER=$(echo ${TOTALBEFORE}|cut -d+ -f1)
		PREFIXAFTER=$(echo ${TOTALAFTER}|cut -d+ -f2)
		if [ ${PREFIXAFTER} -eq 0 ] && [ ${OTHERBEFORE} -eq ${OTHERAFTER} ]; then
			ret=0
		else
			ret=1
			echo "found ${PREFIXAFTER} lines that should be deleted (${FILE})"
		fi
	else
		echo "invalid sipsak return: ${ret}"
	fi
done

# Empty body should get 500 response
FILE="60-message-sdp8.sip"
sipsak ${SIPSAKOPTS} -f ${FILE} > ${TMPFILE}
ret=$?
if [ "${ret}" -eq 1 ] ; then
	ret=0
else
	echo "empty body not rejected"
	ret=1
	exit ${ret}
fi

# Filter only video stream attributes
FILE="60-message-sdp9.sip"
TOTALBEFORE=$(awk '/^v=0/,/^$/ {total++; if ($0 ~ /^a=rtcp/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${FILE})
OTHERBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f1)
PREFIXBEFORE=$(echo ${TOTALBEFORE}|cut -d+ -f2)
sipsak ${SIPSAKOPTS} -f ${FILE} > ${TMPFILE}
ret=$?
if [ "${ret}" -eq 0 ] ; then
	TOTALAFTER=$(awk '/^v=0/,/^$/ {total++; if ($0 ~ /^a=rtcp:/ ) { prefix++;} else { other++} } END {if (prefix) {print other " + " prefix} else { print other " + 0"} }' ${TMPFILE})
	OTHERAFTER=$(echo ${TOTALBEFORE}|cut -d+ -f1)
	PREFIXAFTER=$(echo ${TOTALAFTER}|cut -d+ -f2)
	if [ ${PREFIXAFTER} -eq 1 ] && [ ${OTHERBEFORE} -eq ${OTHERAFTER} ]; then
		ret=0
	else
		ret=1
		echo "found ${PREFIXAFTER} lines with prefix \"a=rtcp\", was expecting 1 (in m=audio)(${FILE})"
	fi
	else
		echo "invalid sipsak return: ${ret}"
fi

kill_kamailio
rm ${TMPFILE}
exit ${ret}

