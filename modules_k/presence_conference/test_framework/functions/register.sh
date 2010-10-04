#!/bin/bash
function register {
	if [[ $# < 2 ]]; then
		echo register must take parameters: host, username, expiry
		return 2
	fi
	expiry=$3
	if [[ ! $expiry ]]; then
		expiry=3600
	fi
	tmpfile=`mktemp /tmp/tmp.XXXXXXXX`
	cat xml/register.xml > $tmpfile
	sed s/"\[expiry\]"/"$expiry"/g -i $tmpfile
	sed s/"\[username\]"/"$2"/g -i $tmpfile
	sipp $ADDITIONAL_PARAMETERS -sf $tmpfile "$1" -m 1 $OUTPUT
	result=$?
	if [[ $result != $_EXPECTED_RETURN ]]; then
		echo sipp went wrong while registering
		return 2
	fi
	return 0
}
