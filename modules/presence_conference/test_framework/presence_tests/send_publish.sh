#!/bin/bash

source ../functions/register.sh
source ../functions/subscribe.sh
source ../functions/publish.sh
source ../functions/notify.sh
source ../functions/rand.sh

EXPECTED_RETURN=0
event="presence"
start_port=8020
content_type="application\/pidf+xml"
usernames_txt="${SUBSCRIBERS_FILE}"
if [[ $1 ]]; then
	usernames_txt=$1
fi
line=`cat $usernames_txt | wc -l`
line_no=`head -1 /dev/urandom | od -N 1 | awk '{ print $2 }'`
let line_no=$line_no%$line
let line_no=$line_no+1
username=`cat $usernames_txt | sed -n ${line_no}p`
for i in `seq 1 $line`; do
	let start_port=$start_port+1
	ADDITIONAL_PARAMETERS="-p $start_port -timeout 20"
	notify > /dev/null 2>> errors.txt
done
sleep 4
publish $event $content_type $username ${KAMAILIO_HOST}
wait
