#!/bin/bash
#
# build script for travis CI
# environment based on Ubuntu 12.04 LTS (precise)
#
export JAVA_HOME="/usr/lib/jvm/java-gcj"
EXCLUDED_MODULES=""
EXTRA_EXCLUDED_MODULES="bdb dbtext oracle pa iptrtpproxy mi_xmlrpc dnssec kazoo cnxcc"
PACKAGE_GROUPS="mysql postgres berkeley unixodbc radius presence ldap xml perl utils lua memcached \
	snmpstats carrierroute xmpp cpl redis python geoip\
	sqlite json mono ims sctp java \
	purple tls outbound websocket autheph"
export TESTS_EXCLUDE="3 12 17 19 20 23 25 26 30 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 50"

function build {
	echo "make distclean"
	make distclean
	echo "make cfg"
	make FLAVOUR=kamailio cfg \
		skip_modules="${EXCLUDED_MODULES} ${EXTRA_EXCLUDED_MODULES}" \
		group_include="kstandard"
	echo "make all"
	make all
	echo "make groups"
	for grp in ${PACKAGE_GROUPS}; do
		make every-module group_include="k${grp}"
	done
}

# build flags
export MEMDBG=0
echo "build with MEMDBG=0"
build

export MEMDBG=1
echo "build with MEMDBG=1"
build

#echo "unit tests"
#make -C test/unit
if [[ "$CC" =~ gcc ]] ; then
	echo "make install"
	sudo make install
else
	echo "skip make install step"
fi
