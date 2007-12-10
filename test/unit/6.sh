#!/bin/bash
# checks a configuration with 'openser -c' and 'openser -C'

CFG=2.cfg

# start
../openser -c -f $CFG > /dev/null 2>&1
ret=$?

if [ "$ret" -eq 0 ] ; then
	../openser -C -f $CFG > /dev/null 2>&1
	ret=$?
fi ;

exit $ret