#!/bin/bash
# load all modules without external dependencies with dbtext

CFG=2.cfg
cp $CFG $CFG.bak

echo "loadmodule \"dbtext/dbtext.so\"" >> $CFG
echo "modparam(\"acc|alias_db|auth_db|dialog|dispatcher|domain|domainpolicy|group|imc|lcr|msilo|siptrace|speeddial|uri_db|usrloc|permissions|pdt\", \"db_url\", \"dbtext://`pwd`/../scripts/dbtext/openser\")" >> $CFG
# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret