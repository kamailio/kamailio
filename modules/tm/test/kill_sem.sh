#!/bin/sh

ipcrm sem `cat /proc/sysvipc/sem | awk '{ print $2; }'`
ipcrm shm `cat /proc/sysvipc/shm | awk '{ print $2; }'` 
