#!/bin/bash
#
# build script for travis CI
# environment based on Debian Stretch
#

set -e

# choose freeradius
export FREERADIUS=1

export JAVA_HOME="/usr/lib/jvm/java-gcj"
EXCLUDED_MODULES=""
EXTRA_EXCLUDED_MODULES="bdb dbtext oracle pa iptrtpproxy mi_xmlrpc"
PACKAGE_GROUPS="mysql postgres berkeley unixodbc radius presence ldap xml perl utils lua memcached \
	snmpstats carrierroute xmpp cpl redis python geoip\
	sqlite json mono ims sctp java \
	purple tls outbound websocket autheph \
	dnssec kazoo cnxcc erlang"

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

if [[ "$CC" =~ clang ]] ; then
	CLANG=$(find /usr/bin -type l -name 'clang-[0-9]*' | sort -r | head -1)
	echo "setting clang to ${CLANG}"
	update-alternatives --install /usr/bin/clang clang $CLANG 1
fi

echo "CC=$CC"
echo "$($CC --version)"

# build flags
export MEMDBG=0
echo "build with MEMDBG=0"
build

export MEMDBG=1
echo "build with MEMDBG=1"
build

if [[ "$CC" =~ gcc ]] ; then
	echo "make install"
	sudo make install
else
	echo "skip make install step"
fi
