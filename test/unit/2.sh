#!/bin/bash
# load all modules without external dependencies with mysql

# Needs a default openser database setup for mysql

if [ ! -e ../modules/mysql/mysql.so ] ; then
	echo "mysql driver not found, not run"
	exit 0
fi ;

CFG=2.cfg
cp $CFG $CFG.bak

touch dispatcher.list

echo "loadmodule \"mysql/mysql.so\"" >> $CFG
echo "modparam(\"dispatcher\", \"list_file\", \"`pwd`/../test/dispatcher.list\")" >> $CFG

# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret