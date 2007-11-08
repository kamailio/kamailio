#!/bin/bash
# test basic fifo functionality

CFG=4.cfg

# setup config
echo -e "loadmodule \"../modules/mi_fifo/mi_fifo.so\"" > $CFG
echo -e "modparam(\"mi_fifo\", \"fifo_name\", \"/tmp/openser_fifo\")" >> $CFG

../openser -f $CFG > /dev/null
ret=$?

cd ../scripts

if [ "$ret" -eq 0 ] ; then
	sleep 1
	./openserctl ps > /dev/null
	ret=$?
fi ;

cd ../test

killall -9 openser

rm -f $CFG

exit $ret