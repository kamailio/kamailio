#!/bin/bash
# load all modules without external dependencies with postgres

# Needs a default openser database setup for postgres

if [ ! -e ../modules/postgres/postgres.so ] ; then
	echo "postgres driver not found, not run"
	exit 0
fi ;

CFG=2.cfg
cp $CFG $CFG.bak

echo "loadmodule \"postgres/postgres.so\"" >> $CFG
echo "modparam(\"acc|alias_db|auth_db|dialog|dispatcher|domain|domainpolicy|group|imc|lcr|msilo|siptrace|speeddial|uri_db|usrloc|permissions|pdt\", \"db_url\", \"postgres://openserro:openserro@localhost/openser\")" >> $CFG


# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret