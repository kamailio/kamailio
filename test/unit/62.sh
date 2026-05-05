#!/bin/sh
# checks phonenum with short code, normal numbers and invalid edge cases

. include/common
. include/require.sh

if [ -n "$KAMAILIO_BIN" ]; then
	BIN="$KAMAILIO_BIN"
fi

if [ -n "$KAMAILIO_MOD_DIR" ]; then
	MOD_DIR="$KAMAILIO_MOD_DIR"
fi

CFG=62.cfg
TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`
SIPSAKOPTS="-H localhost -s sip:127.0.0.1:5060 -v"

run_case() {
	CASE_NAME="$1"
	EXPECT="$2"

	sipsak ${SIPSAKOPTS} --headers "X-Case: ${CASE_NAME}\n" > "$TMPFILE"
	ret=$?
	if [ "$ret" -ne 0 ] ; then
		echo "${CASE_NAME}: sipsak returned ${ret}"
		return 1
	fi

	grep "$EXPECT" "$TMPFILE" > /dev/null
	ret=$?
	if [ "$ret" -ne 0 ] ; then
		echo "${CASE_NAME}: expected response not found: ${EXPECT}"
		return 1
	fi

	echo "${CASE_NAME}: ok"
	return 0
}

if ! (check_sipsak && check_kamailio && check_module "phonenum"); then
	exit 0
fi;

echo "Starting kamailio with config ${CFG}"
$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG  -E
ret=$?
echo "kamailio started with return code ${ret}"
sleep 1

if [ "$ret" -eq 0 ] ; then
	ret=0
	run_case "short_de_115" "200 short_de_115 OK" || ret=1
	run_case "normal_us" "200 normal_us OK" || ret=1
	run_case "normal_e164" "200 normal_e164 OK" || ret=1
	run_case "short_without_region" "200 short_without_region Expected Invalid" || ret=1
	run_case "invalid_alpha" "200 invalid_alpha Expected Invalid" || ret=1
fi;

echo "kamailio killed. tmp file: ${TMPFILE} (can be removed)."
kill_kamailio
rm -f "$TMPFILE"

exit $ret
