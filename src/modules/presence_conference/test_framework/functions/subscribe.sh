#!/bin/bash
function subscribe {
	if [[ $# < 6 ]]; then
		echo not enough parameters for subscribe \(event content_type user host expiry and username\)
		return 2
	fi
	event=$1
	content_type=$2
	presentity=$3
	host=$4
	tmpfile=`mktemp /tmp/subs.XXXXXXXX`
	cat xml/subscribe.xml > $tmpfile
	sed s/"\[event\]"/"$event"/ -i $tmpfile
	sed s/"\[content-type\]"/"$content_type"/ -i $tmpfile
	sed s/"\[presentity\]"/"$presentity"/ -i $tmpfile
	sed s/"\[username\]"/"$6"/ -i $tmpfile
	rand_value=`head /dev/urandom | md5sum | cut -d ' ' -f 1`
	sed s/"\[rand_tag\]"/"$rand_value"/g -i $tmpfile
	sed s/"\[expiry\]"/"$5"/g -i $tmpfile
	sipp $ADDITIONAL_PARAMETERS -sf $tmpfile $host -m 1 $OUTPUT
	if [[ $? != $_EXPECTED_RETURN ]]; then
		echo sipp subscribe went wrong!!
	fi
	return 0
}
