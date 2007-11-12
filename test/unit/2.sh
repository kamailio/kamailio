#!/bin/bash
# load all modules without external dependencies

# Needs a default openser database setup for mysql

CFG=2.cfg
cp $CFG $CFG.bak

touch dispatcher.list

echo "modparam(\"dispatcher\", \"list_file\", \"`pwd`/../test/dispatcher.list\")" >> $CFG

# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret