#!/bin/sh

ipcrm sem `cat /proc/sysvipc/sem | awk '{ print $2; }'`
ipcrm shm `ipcs -m | awk '{ print $2; }'` 
