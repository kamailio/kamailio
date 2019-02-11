#!/bin/bash

OS=`uname -s`
echo $OS
if [ "$OS"  == "Linux" ] 
then
	ipcrm sem `cat /proc/sysvipc/sem | awk '{ print $2; }'`
	ipcrm shm `cat /proc/sysvipc/shm | awk '{ print $2; }'` 
elif [ "$OS" == "SunOS" ]
then	
	whoami=`whoami`
	for r in `ipcs|grep ${whoami}| awk '{ print $2; }' `
	do
		echo "deleting semid $r"
		ipcrm -s $r
	done



fi
