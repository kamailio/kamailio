#!/bin/bash
# load all modules without external dependencies with dbtext

CFG=15.cfg

echo "loadmodule \"../modules/dbtext/dbtext.so\"" >> $CFG
cat 2.cfg >> $CFG
echo "modparam(\"acc|alias_db|auth_db|dialog|dispatcher|domain|domainpolicy|group|imc|lcr|msilo|siptrace|speeddial|uri_db|usrloc|permissions|pdt\", \"db_url\", \"dbtext://`pwd`/../scripts/dbtext/openser\")" >> $CFG

../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

rm $CFG

exit $ret