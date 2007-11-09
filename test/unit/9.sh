#!/bin/bash
# test basic db related openserctl functionality for mysql

cd ../scripts

# setup config file
cp openserctlrc openserctlrc.bak
sed -i "s/# DBENGINE=MYSQL/DBENGINE=MYSQL/g" openserctlrc

./openserctl avp list > /dev/null

ret=$?

if [ "$ret" -eq 0 ] ; then
	./openserctl domain showdb > /dev/null
	ret=$?
fi ;

if [ "$ret" -eq 0 ] ; then
	./openserctl lcr show > /dev/null
	ret=$?
fi ;

# cleanup
mv openserctlrc.bak openserctlrc

cd ../test
exit $ret