#!/bin/bash
# test basic db related openserctl functionality for dbtext

cd ../scripts

# setup config file
cp openserctlrc openserctlrc.bak
sed -i "s/# DBENGINE=MYSQL/DBENGINE=DBTEXT/g" openserctlrc

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