#!/bin/sh

ipcrm sem `ipcs -s |grep $(whoami) | awk '{ print $2; }'`
ipcrm shm `ipcs -m | grep $(whoami) |awk '{ print $2; }'` 
