#!/bin/bash
# loads the openser default config

# Needs a default openser database setup for mysql

CFG=5.cfg

# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

exit $ret