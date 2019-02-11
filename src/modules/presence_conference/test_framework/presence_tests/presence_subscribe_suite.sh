#!/bin/bash

source config.sh

# some basic checking for dependencies
if [[ `whoami` != "root" ]]; then
	echo "you must be root to run the suite..."
	exit $EXIT_FAILURE;
fi
if [[ ! `which sipp` ]]; then
	echo "you do not have sipp installed..."
	exit $EXIT_FAILURE;
fi

if [[ ! `which ngrep` ]]; then
	echo "you do not have ngrep installed..."
	exit $EXIT_FAILURE;
fi

if [[ ! $1 ]]; then
	echo "please choose the number of subscribers..."
	exit $EXIT_FAILURE;
else
	subscribers_no=$1
fi

echo "starting suite..."

# truncating log file
cat /dev/null > ${NGREP_LOG_FILE}
# killing sipp
killall -9 sipp &> /dev/null
# running ngrep
ngrep -d any -W byline port 5060 &> ${NGREP_LOG_FILE} &

./clean_subscribers.sh && ./send_subscribe.sh ${subscribers_no} && ./send_publish.sh

sleep ${SUBSCRIBE_WAIT_SECONDS}

# killing ngrep
killall -9 ngrep &> /dev/null

# get the number of notifies sent
notify_response=`grep 'NOTIFY sip' ${NGREP_LOG_FILE} | wc -l`

echo received ${notify_response} responses...

let responses=${subscribers_no}*${subscribers_no}+${subscribers_no}
if [[ $responses == ${notify_response} ]]; then
	echo that look\'s ok...
else
	echo should have received $responses responses...
fi

# cleaning up the sipp stderr output file
sed s/'Resolving remote host.*'// -i errors.txt
sed s/'Done.$'// -i errors.txt
sed '/^$/d' -i errors.txt
