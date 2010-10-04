#!/bin/bash
SLEEP_STATE=0.5
LOCAL_IP=127.0.0.1
ADDITIONAL_PARAMETERS="-mp 40220 -mi $LOCAL_IP -p 5068 -t u1 -i $LOCAL_IP"
_EXPECTED_RETURN=99
OUTPUT=" -bg "
source functions/register.sh
source functions/subscribe.sh
source functions/publish.sh
source functions/notify.sh
event="conference"
while [[ $# > 0 ]]; do

	if [[ $1 == "-event" ]]; then
		event=$2
		shift
	fi
	shift
done

content_type=""
if [[ $event == "presence" ]]; then
	content_type="application\/pidf+xml"
fi
if [[ $event == "conference" ]]; then
	content_type="application\/conference-info+xml"
fi

if [[ $content_type == "" ]]; then
	echo unknown event: $event
	exit 2
fi

###deregister sipp
echo deregistering sipp...
register $LOCAL_IP sipp 0 &> /dev/null
sleep $SLEEP_STATE

###deregister conference
echo deregistering conference...
register $LOCAL_IP conference 0 &> /dev/null
sleep $SLEEP_STATE

###unsubscribe to conference
echo unsubscribing to conference event package...
subscribe $event "application\/conference-info+xml" conference $LOCAL_IP 0 sipp &> /dev/null
sleep $SLEEP_STATE

###kill remaining sipp bg processes
sleep $SLEEP_STATE;sleep $SLEEP_STATE;
killall sipp &> /dev/null

echo all done!!
