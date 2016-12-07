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
clear
while [[ true ]]; do

	echo s\:subscribe \| r\:register \| p\:publish \| us\:unsubscribe
	read opt
	if [[ "$opt" == "r" ]]; then
		echo enter username:
		read username
		register $LOCAL_IP $username &> /dev/null
		echo registered $username
	fi
	if [[ "$opt" == "s" ]]; then
		echo enter username
		read username
		echo enter presentity
		read presentity
		subscribe $event $content_type $presentity $LOCAL_IP 3600 $username  &>/dev/null
		echo regeristered watcher $username on event:$event - for resource $presentity &> /dev/null
	fi
	if [[ "$opt" == "p" ]]; then
		notify &> /dev/null
		sleep $SLEEP_STATE
		echo enter presentity
		read presentity
		publish $event $content_type $presentity $LOCAL_IP &>/dev/null
		echo published document for event package $event - resource $presentity
	fi
	if [[ "$opt" == "us" ]]; then
                echo enter username
                read username
                echo enter presentity
                read presentity
                subscribe $event $content_type $presentity $LOCAL_IP 0 $username  &>/dev/null
                echo regeristered watcher $username on event:$event - for resource $presentity &> /dev/null
        fi
	sleep $SLEEP_STATE
	killall sipp &> /dev/null
	clear

done

