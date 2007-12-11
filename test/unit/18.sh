#!/bin/bash
# runs ../openser with all command line arguments.
# ommited options are -h -v -C -c -D

# the config file
CFG=18.cfg

# setup config
echo -e "debug=3" > $CFG

# start:
../openser -f ./$CFG -l 127.0.0.1 -n 0 -rR -v  -E -d -T -N 0 -b 23 -m 42 -w ./  -u $(id -u)  -g $(id -g) -P ./pid.out -G ./pgid.out  > /dev/null 2>&1

ret=$?

sleep 1

# clean up:
killall -9 openser

rm $CFG
rm pgid.out
rm pid.out

exit $ret
