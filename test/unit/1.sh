#!/bin/bash
# load a minimal config

CFG=1.cfg

# setup config
echo -e "debug=3" > $CFG

../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

rm -f $CFG

exit $ret