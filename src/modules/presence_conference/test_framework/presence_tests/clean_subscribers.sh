#!/bin/bash
killall -9 sipp &> /dev/null

mysql_username="-u${MYSQL_USERNAME}"
if [[ ${MYSQL_PASSWORD} ]]; then
	mysql_pass="-p$2"
	echo "da"
else
	mysql_pass=""
fi

mysql ${mysql_username} ${mysql_pass} ${MYSQL_DATABASE} -e 'truncate table watchers'
mysql ${mysql_username} ${mysql_pass} ${MYSQL_DATABASE} -e 'truncate table active_watchers'
