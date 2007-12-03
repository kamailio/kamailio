#!/bin/bash
# checks a configuration with 'openser -c'

CFG=2.cfg

# start
../openser -c -f $CFG > /dev/null 2>&1
ret=$?

sleep 1

exit $ret