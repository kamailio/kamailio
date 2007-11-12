#!/bin/bash
# checks a configuration with 'openser -c'

CFG=2.cfg

# start
../openser -c -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

exit $ret