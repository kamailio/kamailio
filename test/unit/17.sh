#!/bin/bash
# load all modules without external dependencies with db_berkeley

if [ ! -e ../modules/db_berkeley/db_berkeley.so ] ; then
	echo "db_berkeley driver not found, not run"
	exit 0
fi ;

CFG=17.cfg

echo "loadmodule \"../modules/db_berkeley/db_berkeley.so\"" >> $CFG
cat 2.cfg >> $CFG
echo "modparam(\"acc|alias_db|auth_db|dialog|dispatcher|domain|domainpolicy|group|imc|lcr|msilo|siptrace|speeddial|uri_db|usrloc|permissions|pdt\", \"db_url\", \"db_berkeley://`pwd`/../scripts/db_berkeley/openser\")" >> $CFG

../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

rm $CFG

exit $ret