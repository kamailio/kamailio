#!/bin/sh

ipcrm sem `ipcs -s | awk '{ print $2; }'`
ipcrm shm `ipcs -m | awk '{ print $2; }'` 
