#!/bin/sh

ipcrm -s `ipcs -s |grep $(whoami) | awk '{ print $2; }'`
ipcrm -m `ipcs -m | grep $(whoami) |awk '{ print $2; }'` 
