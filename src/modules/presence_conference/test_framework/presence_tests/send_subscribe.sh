#!/bin/bash -e

source ../functions/register.sh
source ../functions/subscribe.sh
source ../functions/publish.sh
source ../functions/notify.sh
source ../functions/rand.sh

cat /dev/null > errors.txt
cat /dev/null > ${SUBSCRIBERS_FILE}

EXPECTED_RETURN=0
start_port=8020
event="presence"
content_type="application\/pidf+xml"
control_port=40400
mi_port=20200

for i in `seq 1 $1`
do 
	let start_port=$start_port+1
	let mi_port=$mi_port+100
	let control_port=$control_port+100
	usernames[$i]=$i
	#`rand_md5` is more professional
	local_ports[$i]=$start_port
	mi_ports[$i]=$mi_port
	control_ports[$i]=$control_port
done

k=0
for i in `seq 1 $1`
do
	echo ${usernames[$i]} >> ${SUBSCRIBERS_FILE}
	for j in `seq 1 $1`
	do
		echo subscribing ${usernames[$j]} to ${usernames[$i]} on port ${local_ports[$j]}...
		ADDITIONAL_PARAMETERS=" -mi ${MI_HOST} -mp ${mi_ports[$j]} -cp ${control_ports[$j]} -p ${local_ports[$j]}"
		subscribe $event $content_type ${usernames[$i]} ${KAMAILIO_HOST} 3600 ${usernames[$j]} 2>> errors.txt > /dev/null &
		pids[$k]=$!
		let k=$k+1
	done
	wait
done

wait

