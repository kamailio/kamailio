#!/bin/bash
function publish {
	if [[ $# < 4 ]]; then
		echo not enough parameters for subscribe \(event content_type user and host\)
		return 2
	fi
	event=$1
	content_type=$2
	presentity=$3
	host=$4
	tmpfile=`mktemp /tmp/subs.XXXXXXXX`
	cat xml/publish.xml > $tmpfile
	sed s/"\[event\]"/"$event"/ -i $tmpfile
	sed s/"\[content-type\]"/"$content_type"/ -i $tmpfile
	sed s/"\[presentity\]"/"$presentity"/ -i $tmpfile
	rand_value=`head /dev/urandom | md5sum | cut -d ' ' -f 1`
	sed s/"\[rand_tag\]"/"$rand_value"/g -i $tmpfile
	sipp $ADDITIONAL_PARAMETERS -p 5260 -mp 5262 -sf $tmpfile $host -m 1 $OUTPUT
	if [[ $? != $_EXPECTED_RETURN ]]; then
		echo sipp publish went wrong!!
	fi
	return 0
}
