#!/bin/sh

ipcrm sem `ipcs -s | awk '{ print $2; }'` 
