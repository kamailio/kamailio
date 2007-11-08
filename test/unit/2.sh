#!/bin/bash
# load all modules without external dependencies

# Needs a default openser database setup for mysql

CFG=2.cfg

echo "" > dispatcher.list

# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

rm -f dispatcher.list

exit $ret