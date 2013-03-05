%define name    kamailio
%define ver     3.3.4
%define rel     0%{dist}



Summary:       Kamailio (OpenSER) - the Open Source SIP Server
Name:          %name
Version:       %ver
Release:       %rel
Packager:      Peter Dunkley <peter@dunkley.me.uk>
License:       GPL
Group:         System Environment/Daemons
Source:        http://kamailio.org/pub/kamailio/%{ver}/src/%{name}-%{ver}_src.tar.gz
URL:           http://kamailio.org/
Vendor:        kamailio.org
BuildRoot:     %{_tmppath}/%{name}-%{ver}-buildroot
Conflicts:     kamailio-mysql < %ver, kamailio-postgresql < %ver
Conflicts:     kamailio-unixODBC < %ver, kamailio-bdb < %ver
Conflicts:     kamailio-sqlite < %ver, kamailio-utils < %ver
Conflicts:     kamailio-cpl < %ver, kamailio-snmpstats < %ver
Conflicts:     kamailio-presence < %ver, kamailio-xmpp < %ver
Conflicts:     kamailio-tls < %ver, kamailio-purple < %ver, kamailio-ldap < %ver
Conflicts:     kamailio-xmlrpc < %ver, kamailio-perl < %ver, kamailio-lua < %ver
Conflicts:     kamailio-python < %ver, kamailio-regex < %ver
Conflicts:     kamailio-dialplan < %ver, kamailio-lcr < %ver
Conflicts:     kamailio-xmlops < %ver
%if 0%{?fedora}
Conflicts:     kamailio-radius < %ver, kamailio-carrierroute < %ver
Conflicts:     kamailio-redis < %ver, kamailio-json < %ver 
Conflicts:     kamailio-mono < %ver, kamailio-GeoIP < %ver
%endif
BuildRequires: bison flex gcc make redhat-rpm-config

%description
Kamailio (former OpenSER) is an Open Source SIP Server released under GPL, able
to handle thousands of call setups per second. Among features: asynchronous TCP,
UDP and SCTP, secure communication via TLS for VoIP (voice, video); IPv4 and
IPv6; SIMPLE instant messaging and presence with embedded XCAP server and MSRP
relay; ENUM; DID and least cost routing; load balancing; routing fail-over;
accounting, authentication and authorization; support for many backend systems
such as MySQL, Postgres, Oracle, Radius, LDAP, Redis, Cassandra; XMLRPC control
interface, SNMP monitoring. It can be used to build large VoIP servicing
platforms or to scale up SIP-to-PSTN gateways, PBX systems or media servers
like Asterisk™, FreeSWITCH™ or SEMS.


%package mysql
Summary:       MySQL database connectivity for Kamailio.
Group:         System Environment/Daemons
Requires:      mysql-libs, kamailio = %ver
BuildRequires: mysql-devel zlib-devel

%description mysql
MySQL database connectivity for Kamailio.


%package postgresql
Summary:       PostgreSQL database connectivity for Kamailio.
Group:         System Environment/Daemons
Requires:      postgresql-libs, kamailio = %ver
BuildRequires: postgresql-devel

%description postgresql
PostgreSQL database connectivity for Kamailio.


%package unixODBC
Summary:       unixODBC database connectivity for Kamailio.
Group:         System Environment/Daemons
Requires:      unixODBC, kamailio = %ver
BuildRequires: unixODBC-devel

%description unixODBC
unixODBC database connectivity for Kamailio.


%package bdb
Summary:       Berkeley database connectivity for Kamailio.
Group:         System Environment/Daemons
Requires:      db4, kamailio = %ver
BuildRequires: db4-devel

%description bdb
Berkeley database connectivity for Kamailio.


%package sqlite
Summary:       SQLite database connectivity for Kamailio.
Group:         System Environment/Daemons
Requires:      sqlite, kamailio = %ver
BuildRequires: sqlite-devel

%description sqlite
SQLite database connectivity for Kamailio.


%package utils
Summary:       Non-SIP utitility functions for Kamailio.
Group:         System Environment/Daemons
Requires:      libcurl, libxml2, kamailio = %ver
BuildRequires: libcurl-devel, libxml2-devel

%description utils
Non-SIP utitility functions for Kamailio.


%package cpl
Summary:       CPL (Call Processing Language) interpreter for Kamailio.
Group:         System Environment/Daemons
Requires:      libxml2, kamailio = %ver
BuildRequires: libxml2-devel

%description cpl
CPL (Call Processing Language) interpreter for Kamailio.


%package snmpstats
Summary:       SNMP management interface (scalar statistics) for Kamailio.
Group:         System Environment/Daemons
%if 0%{?fedora}
Requires:      net-snmp-agent-libs, kamailio = %ver
%else
Requires:      net-snmp-libs, kamailio = %ver
%endif
BuildRequires: net-snmp-devel

%description snmpstats
SNMP management interface (scalar statistics) for Kamailio.


%package presence
Summary:       SIP Presence (and RLS, XCAP, etc) support for Kamailio.
Group:         System Environment/Daemons
Requires:      libxml2, libcurl, kamailio = %ver, kamailio-xmpp = %ver
BuildRequires: libxml2-devel, libcurl-devel

%description presence
SIP Presence (and RLS, XCAP, etc) support for Kamailio.


%package xmpp
Summary:       SIP/XMPP IM gateway for Kamailio.
Group:         System Environment/Daemons
Requires:      expat, kamailio = %ver
BuildRequires: expat-devel

%description xmpp
SIP/XMPP IM gateway for Kamailio.


%package tls
Summary:       TLS transport for Kamailio.
Group:         System Environment/Daemons
Requires:      openssl, kamailio = %ver
BuildRequires: openssl-devel

%description tls
TLS transport for Kamailio.


%package ldap
Summary:       LDAP search interface for Kamailio.
Group:         System Environment/Daemons
Requires:      openldap, kamailio = %ver
BuildRequires: openldap-devel

%description ldap
LDAP search interface for Kamailio.


%package xmlrpc
Summary:       XMLRPC trasnport and encoding for Kamailio RPCs.
Group:         System Environment/Daemons
Requires:      libxml2, kamailio = %ver
BuildRequires: libxml2-devel

%description xmlrpc
XMLRPC trasnport and encoding for Kamailio RPCs.


%package perl
Summary:       Perl extensions and database driver for Kamailio.
Group:         System Environment/Daemons 
Requires:      mod_perl, kamailio = %ver
BuildRequires: mod_perl-devel

%description perl
Perl extensions and database driver for Kamailio.


%package lua
Summary:       Lua extensions for Kamailio.
Group:         System Environment/Daemons
Requires:      kamailio = %ver
BuildRequires: lua-devel

%description lua
Lua extensions for Kamailio.


%package python
Summary:       Python extensions for Kamailio.
Group:         System Environment/Daemons
Requires:      python, kamailio = %ver
BuildRequires: python-devel

%description python
Python extensions for Kamailio.


%package regex
Summary:       PCRE mtaching operations for Kamailio.
Group:         System Environment/Daemons
Requires:      pcre, kamailio = %ver
BuildRequires: pcre-devel

%description regex
PCRE mtaching operations for Kamailio.


%package dialplan
Summary:       String translations based on rules for Kamailio.
Group:         System Environment/Daemons
Requires:      pcre, kamailio = %ver
BuildRequires: pcre-devel

%description dialplan
String translations based on rules for Kamailio.


%package lcr
Summary:       Least cost routing for Kamailio.
Group:         System Environment/Daemons
Requires:      pcre, kamailio = %ver
BuildRequires: pcre-devel

%description lcr
Least cost routing for Kamailio.


%package xmlops
Summary:       XML operation functions for Kamailio.
Group:         System Environment/Daemons
Requires:      libxml2, kamailio = %ver
BuildRequires: libxml2-devel

%description xmlops
XML operation functions for Kamailio.


%package  purple
Summary:  Multi-protocol IM and presence gateway module.
Group:    System Environment/Daemons
%if 0%{?fedora}
Requires: glib, libpurple, libxml2, kamailio = %ver, kamailio-presence = %ver
BuildRequires: glib-devel, libpurple-devel, libxml2-devel
%else
Requires: glib2, libpurple, libxml2, kamailio = %ver, kamailio-presence = %ver
BuildRequires: glib2-devel, libpurple-devel, libxml2-devel
%endif

%description purple
Multi-protocol IM and presence gateway module.

%if 0%{?fedora}
%package radius
Summary:       Radius AAA API for Kamailio.
Group:         System Environment/Daemons
Requires:      radiusclient-ng, kamailio = %ver
BuildRequires: radiusclient-ng-devel

%description radius
Radius AAA API for Kamailio.


%package carrierroute
Summary:       Routing, balancing, and blacklisting for Kamailio.
Group:         System Environment/Daemons
Requires:      libconfuse, kamailio = %ver
BuildRequires: libconfuse-devel

%description carrierroute
Routing, balancing, and blacklisting for Kamailio.


%package redis
Summary:       REDIS NoSQL database connector for Kamailio.
Group:         System Environment/Daemons
Requires:      hiredis, kamailio = %ver
BuildRequires: hiredis-devel

%description redis
REDIS NoSQL database connector for Kamailio.


%package json
Summary:       json string operation and rpc support for Kamailio.
Group:         System Environment/Daemons
Requires:      json-c, libevent, kamailio = %ver
BuildRequires: json-c-devel, libevent-devel

%description json
json string operation and rpc support for Kamailio.


%package mono
Summary:       Mono extensions for Kamailio.
Group:         System Environment/Daemons
Requires:      mono-core, kamailio = %ver
BuildRequires: mono-devel

%description mono
Mono extensions for Kamailio.


%package GeoIP
Summary:       Max Mind GeoIP real-time query support for Kamailio.
Group:         System Environment/Daemons
Requires:      GeoIP, kamailio = %ver
BuildRequires: GeoIP-devel

%description GeoIP
Max Mind GeoIP real-time query support for Kamailio.
%endif



%prep
%setup -n %{name}-%{ver}



%build
make FLAVOUR=kamailio cfg prefix=/usr cfg_prefix=$RPM_BUILD_ROOT\
	basedir=$RPM_BUILD_ROOT cfg_target=/%{_sysconfdir}/kamailio/\
	modules_dirs="modules modules_k"
make
%if 0%{?fedora}
make every-module skip_modules="auth_identity db_cassandra iptrtpproxy\
	db_oracle memcached mi_xmlrpc osp" group_include="kstandard kmysql\
	kpostgres kunixodbc kldap kperl kpython klua kutils kpurple ktls kxmpp\
	kcpl ksnmpstats kcarrierroute kpresence kradius kgeoip kregex kdialplan\
	klcr ksqlite kredis kjson kmono kberkeley" include_modules="xmlrpc\
	xmlops malloc_test auth_diameter"
%else
make every-module skip_modules="auth_identity db_cassandra iptrtpproxy\
	db_oracle memcached mi_xmlrpc osp" group_include="kstandard kmysql\
	kpostgres kunixodbc kldap kperl kpython klua kutils kpurple ktls kxmpp\
	kcpl ksnmpstats kpresence kregex kdialplan\
	klcr ksqlite kberkeley" include_modules="xmlrpc\
	xmlops malloc_test auth_diameter"
%endif
make utils



%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make install
%if 0%{?fedora}
make install-modules-all skip_modules="auth_identity db_cassandra iptrtpproxy\
	db_oracle memcached mi_xmlrpc osp" group_include="kstandard kmysql\
	kpostgres kunixodbc kldap kperl kpython klua kutils kpurple ktls kxmpp\
	kcpl ksnmpstats kcarrierroute kpresence kradius kgeoip kregex kdialplan\
	klcr ksqlite kredis kjson kmono kberkeley" include_modules="xmlrpc\
	xmlops malloc_test auth_diameter"

mkdir -p $RPM_BUILD_ROOT/%{_unitdir}
install -m644 pkg/kamailio/fedora/%{?fedora}/kamailio.service \
		$RPM_BUILD_ROOT/%{_unitdir}/kamailio.service

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
install -m644 pkg/kamailio/fedora/%{?fedora}/kamailio.sysconfig \
		$RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/kamailio
%else
make install-modules-all skip_modules="auth_identity db_cassandra iptrtpproxy\
	db_oracle memcached mi_xmlrpc osp" group_include="kstandard kmysql\
	kpostgres kunixodbc kldap kperl kpython klua kutils kpurple ktls kxmpp\
	kcpl ksnmpstats kpresence kregex kdialplan\
	klcr ksqlite kberkeley" include_modules="xmlrpc\
	xmlops malloc_test auth_diameter"

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
install -m755 pkg/kamailio/centos/%{?centos}/kamailio.init \
		$RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/kamailio

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
install -m644 pkg/kamailio/centos/%{?centos}/kamailio.sysconfig \
		$RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/kamailio
%endif



%pre
/usr/sbin/groupadd -r kamailio 2> /dev/null || :
/usr/sbin/useradd -r -g kamailio -s /bin/false -c "Kamailio daemon" -d \
		%{_libdir}/kamailio kamailio 2> /dev/null || :



%clean
rm -rf "$RPM_BUILD_ROOT"



%post
%if 0%{?fedora}
/bin/systemctl --system daemon-reload
%else
/sbin/chkconfig --add kamailio
%endif



%preun
if [ $1 = 0 ]; then
%if 0%{?fedora}
	/bin/systemctl stop kamailio.service
	/bin/systemctl disable kamailio.service 2> /dev/null
%else
	/sbin/service kamailio stop > /dev/null 2>&1
	/sbin/chkconfig --del kamailio
%endif
fi



%postun
%if 0%{?fedora}
/bin/systemctl --system daemon-reload
%endif



%files
%defattr(-,root,root)
%dir %{_docdir}/kamailio
%doc %{_docdir}/kamailio/AUTHORS
%doc %{_docdir}/kamailio/NEWS
%doc %{_docdir}/kamailio/INSTALL
%doc %{_docdir}/kamailio/README
%doc %{_docdir}/kamailio/README-MODULES

%dir %{_docdir}/kamailio/modules
%doc %{_docdir}/kamailio/modules/README.async
%doc %{_docdir}/kamailio/modules/README.auth
%doc %{_docdir}/kamailio/modules/README.avpops
%doc %{_docdir}/kamailio/modules/README.blst
%doc %{_docdir}/kamailio/modules/README.cfg_db
%doc %{_docdir}/kamailio/modules/README.cfg_rpc
%doc %{_docdir}/kamailio/modules/README.counters
%doc %{_docdir}/kamailio/modules/README.ctl
%doc %{_docdir}/kamailio/modules/README.db_flatstore
%doc %{_docdir}/kamailio/modules/README.debugger
%doc %{_docdir}/kamailio/modules/README.enum
%doc %{_docdir}/kamailio/modules/README.ipops
%doc %{_docdir}/kamailio/modules/README.malloc_test
%doc %{_docdir}/kamailio/modules/README.matrix
%doc %{_docdir}/kamailio/modules/README.mediaproxy
%doc %{_docdir}/kamailio/modules/README.mi_rpc
%doc %{_docdir}/kamailio/modules/README.mqueue
%doc %{_docdir}/kamailio/modules/README.msrp
%doc %{_docdir}/kamailio/modules/README.mtree
%doc %{_docdir}/kamailio/modules/README.pdb
%doc %{_docdir}/kamailio/modules/README.pipelimit
%doc %{_docdir}/kamailio/modules/README.prefix_route
%doc %{_docdir}/kamailio/modules/README.ratelimit
%doc %{_docdir}/kamailio/modules/README.rtpproxy
%doc %{_docdir}/kamailio/modules/README.sanity
%doc %{_docdir}/kamailio/modules/README.sdpops
%doc %{_docdir}/kamailio/modules/README.sipcapture
%doc %{_docdir}/kamailio/modules/README.sl
%doc %{_docdir}/kamailio/modules/README.sms
%doc %{_docdir}/kamailio/modules/README.textopsx
%doc %{_docdir}/kamailio/modules/README.tm
%doc %{_docdir}/kamailio/modules/README.tmrec
%doc %{_docdir}/kamailio/modules/README.topoh
%doc %{_docdir}/kamailio/modules/README.xhttp
%doc %{_docdir}/kamailio/modules/README.xhttp_rpc

%dir %{_docdir}/kamailio/modules_k
%doc %{_docdir}/kamailio/modules_k/README.acc
%doc %{_docdir}/kamailio/modules_k/README.alias_db
%doc %{_docdir}/kamailio/modules_k/README.auth_db
%doc %{_docdir}/kamailio/modules_k/README.auth_diameter
%doc %{_docdir}/kamailio/modules_k/README.benchmark
%doc %{_docdir}/kamailio/modules_k/README.call_control
%doc %{_docdir}/kamailio/modules_k/README.cfgutils
%doc %{_docdir}/kamailio/modules_k/README.db_cluster
%doc %{_docdir}/kamailio/modules_k/README.db_text
%doc %{_docdir}/kamailio/modules_k/README.dialog
%doc %{_docdir}/kamailio/modules_k/README.dispatcher
%doc %{_docdir}/kamailio/modules_k/README.diversion
%doc %{_docdir}/kamailio/modules_k/README.dmq
%doc %{_docdir}/kamailio/modules_k/README.domain
%doc %{_docdir}/kamailio/modules_k/README.domainpolicy
%doc %{_docdir}/kamailio/modules_k/README.drouting
%doc %{_docdir}/kamailio/modules_k/README.exec
%doc %{_docdir}/kamailio/modules_k/README.group
%doc %{_docdir}/kamailio/modules_k/README.htable
%doc %{_docdir}/kamailio/modules_k/README.imc
%doc %{_docdir}/kamailio/modules_k/README.kex
%doc %{_docdir}/kamailio/modules_k/README.maxfwd
%doc %{_docdir}/kamailio/modules_k/README.mi_datagram
%doc %{_docdir}/kamailio/modules_k/README.mi_fifo
%doc %{_docdir}/kamailio/modules_k/README.msilo
%doc %{_docdir}/kamailio/modules_k/README.nat_traversal
%doc %{_docdir}/kamailio/modules_k/README.nathelper
%doc %{_docdir}/kamailio/modules_k/README.p_usrloc
%doc %{_docdir}/kamailio/modules_k/README.path
%doc %{_docdir}/kamailio/modules_k/README.pdt
%doc %{_docdir}/kamailio/modules_k/README.permissions
%doc %{_docdir}/kamailio/modules_k/README.pike
%doc %{_docdir}/kamailio/modules_k/README.pv
%doc %{_docdir}/kamailio/modules_k/README.qos
%doc %{_docdir}/kamailio/modules_k/README.registrar
%doc %{_docdir}/kamailio/modules_k/README.rr
%doc %{_docdir}/kamailio/modules_k/README.rtimer
%doc %{_docdir}/kamailio/modules_k/README.seas
%doc %{_docdir}/kamailio/modules_k/README.siptrace
%doc %{_docdir}/kamailio/modules_k/README.siputils
%doc %{_docdir}/kamailio/modules_k/README.speeddial
%doc %{_docdir}/kamailio/modules_k/README.sqlops
%doc %{_docdir}/kamailio/modules_k/README.sst
%doc %{_docdir}/kamailio/modules_k/README.statistics
%doc %{_docdir}/kamailio/modules_k/README.textops
%doc %{_docdir}/kamailio/modules_k/README.tmx
%doc %{_docdir}/kamailio/modules_k/README.uac
%doc %{_docdir}/kamailio/modules_k/README.uac_redirect
%doc %{_docdir}/kamailio/modules_k/README.uri_db
%doc %{_docdir}/kamailio/modules_k/README.userblacklist
%doc %{_docdir}/kamailio/modules_k/README.usrloc
%doc %{_docdir}/kamailio/modules_k/README.xlog

%dir %attr(-,kamailio,kamailio) %{_sysconfdir}/kamailio
%config(noreplace) %{_sysconfdir}/kamailio/*
%if 0%{?fedora}
%config %{_unitdir}/*
%else
%config %{_sysconfdir}/rc.d/init.d/*
%endif
%config %{_sysconfdir}/sysconfig/*

%dir %{_libdir}/kamailio
%{_libdir}/kamailio/libbinrpc.so
%{_libdir}/kamailio/libbinrpc.so.0
%{_libdir}/kamailio/libbinrpc.so.0.1
%{_libdir}/kamailio/libkcore.so
%{_libdir}/kamailio/libkcore.so.1
%{_libdir}/kamailio/libkcore.so.1.0
%{_libdir}/kamailio/libkmi.so
%{_libdir}/kamailio/libkmi.so.1
%{_libdir}/kamailio/libkmi.so.1.0
%{_libdir}/kamailio/libsrdb1.so
%{_libdir}/kamailio/libsrdb1.so.1
%{_libdir}/kamailio/libsrdb1.so.1.0
%{_libdir}/kamailio/libsrdb2.so
%{_libdir}/kamailio/libsrdb2.so.1
%{_libdir}/kamailio/libsrdb2.so.1.0
%{_libdir}/kamailio/libsrutils.so
%{_libdir}/kamailio/libsrutils.so.1
%{_libdir}/kamailio/libsrutils.so.1.0
%{_libdir}/kamailio/libtrie.so
%{_libdir}/kamailio/libtrie.so.1
%{_libdir}/kamailio/libtrie.so.1.0

%dir %{_libdir}/kamailio/modules
%{_libdir}/kamailio/modules/auth.so
%{_libdir}/kamailio/modules/async.so
%{_libdir}/kamailio/modules/avpops.so
%{_libdir}/kamailio/modules/blst.so
%{_libdir}/kamailio/modules/cfg_db.so
%{_libdir}/kamailio/modules/cfg_rpc.so
%{_libdir}/kamailio/modules/counters.so
%{_libdir}/kamailio/modules/ctl.so
%{_libdir}/kamailio/modules/db_flatstore.so
%{_libdir}/kamailio/modules/debugger.so
%{_libdir}/kamailio/modules/enum.so
%{_libdir}/kamailio/modules/ipops.so
%{_libdir}/kamailio/modules/malloc_test.so
%{_libdir}/kamailio/modules/matrix.so
%{_libdir}/kamailio/modules/mediaproxy.so
%{_libdir}/kamailio/modules/mi_rpc.so
%{_libdir}/kamailio/modules/mqueue.so
%{_libdir}/kamailio/modules/msrp.so
%{_libdir}/kamailio/modules/mtree.so
%{_libdir}/kamailio/modules/pdb.so
%{_libdir}/kamailio/modules/pipelimit.so
%{_libdir}/kamailio/modules/prefix_route.so
%{_libdir}/kamailio/modules/ratelimit.so
%{_libdir}/kamailio/modules/rtpproxy.so
%{_libdir}/kamailio/modules/sanity.so
%{_libdir}/kamailio/modules/sipcapture.so
%{_libdir}/kamailio/modules/sl.so
%{_libdir}/kamailio/modules/sdpops.so
%{_libdir}/kamailio/modules/sms.so
%{_libdir}/kamailio/modules/tm.so
%{_libdir}/kamailio/modules/tmrec.so
%{_libdir}/kamailio/modules/textopsx.so
%{_libdir}/kamailio/modules/topoh.so
%{_libdir}/kamailio/modules/xhttp.so
%{_libdir}/kamailio/modules/xhttp_rpc.so

%dir %{_libdir}/kamailio/modules_k
%{_libdir}/kamailio/modules_k/acc.so
%{_libdir}/kamailio/modules_k/alias_db.so
%{_libdir}/kamailio/modules_k/auth_db.so
%{_libdir}/kamailio/modules_k/auth_diameter.so
%{_libdir}/kamailio/modules_k/benchmark.so
%{_libdir}/kamailio/modules_k/call_control.so
%{_libdir}/kamailio/modules_k/cfgutils.so
%{_libdir}/kamailio/modules_k/db_cluster.so
%{_libdir}/kamailio/modules_k/db_text.so
%{_libdir}/kamailio/modules_k/dialog.so
%{_libdir}/kamailio/modules_k/dispatcher.so
%{_libdir}/kamailio/modules_k/diversion.so
%{_libdir}/kamailio/modules_k/dmq.so
%{_libdir}/kamailio/modules_k/domain.so
%{_libdir}/kamailio/modules_k/domainpolicy.so
%{_libdir}/kamailio/modules_k/drouting.so
%{_libdir}/kamailio/modules_k/exec.so
%{_libdir}/kamailio/modules_k/group.so
%{_libdir}/kamailio/modules_k/htable.so
%{_libdir}/kamailio/modules_k/imc.so
%{_libdir}/kamailio/modules_k/kex.so
%{_libdir}/kamailio/modules_k/maxfwd.so
%{_libdir}/kamailio/modules_k/mi_datagram.so
%{_libdir}/kamailio/modules_k/mi_fifo.so
%{_libdir}/kamailio/modules_k/msilo.so
%{_libdir}/kamailio/modules_k/nat_traversal.so
%{_libdir}/kamailio/modules_k/nathelper.so
%{_libdir}/kamailio/modules_k/p_usrloc.so
%{_libdir}/kamailio/modules_k/path.so
%{_libdir}/kamailio/modules_k/pdt.so
%{_libdir}/kamailio/modules_k/permissions.so
%{_libdir}/kamailio/modules_k/pike.so
%{_libdir}/kamailio/modules_k/pv.so
%{_libdir}/kamailio/modules_k/qos.so
%{_libdir}/kamailio/modules_k/registrar.so
%{_libdir}/kamailio/modules_k/rr.so
%{_libdir}/kamailio/modules_k/rtimer.so
%{_libdir}/kamailio/modules_k/seas.so
%{_libdir}/kamailio/modules_k/siptrace.so
%{_libdir}/kamailio/modules_k/siputils.so
%{_libdir}/kamailio/modules_k/speeddial.so
%{_libdir}/kamailio/modules_k/sqlops.so
%{_libdir}/kamailio/modules_k/sst.so
%{_libdir}/kamailio/modules_k/statistics.so
%{_libdir}/kamailio/modules_k/textops.so
%{_libdir}/kamailio/modules_k/tmx.so
%{_libdir}/kamailio/modules_k/uac.so
%{_libdir}/kamailio/modules_k/uac_redirect.so
%{_libdir}/kamailio/modules_k/uri_db.so
%{_libdir}/kamailio/modules_k/userblacklist.so
%{_libdir}/kamailio/modules_k/usrloc.so
%{_libdir}/kamailio/modules_k/xlog.so

%{_sbindir}/kamailio
%{_sbindir}/kamctl
%{_sbindir}/kamdbctl
%{_sbindir}/sercmd

%dir %{_libdir}/kamailio/kamctl
%{_libdir}/kamailio/kamctl/kamctl.base
%{_libdir}/kamailio/kamctl/kamctl.ctlbase
%{_libdir}/kamailio/kamctl/kamctl.dbtext
%{_libdir}/kamailio/kamctl/kamctl.fifo
%{_libdir}/kamailio/kamctl/kamctl.ser
%{_libdir}/kamailio/kamctl/kamctl.ser_mi
%{_libdir}/kamailio/kamctl/kamctl.sqlbase
%{_libdir}/kamailio/kamctl/kamctl.unixsock
%{_libdir}/kamailio/kamctl/kamdbctl.base
%{_libdir}/kamailio/kamctl/kamdbctl.dbtext

%dir %{_libdir}/kamailio/kamctl/dbtextdb
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.py
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.pyc
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.pyo

%{_mandir}/man5/*
%if 0%{?fedora}
%{_mandir}/man7/auth.7.gz
%endif
%{_mandir}/man8/*

%dir %{_datadir}/kamailio
%dir %{_datadir}/kamailio/dbtext
%dir %{_datadir}/kamailio/dbtext/kamailio
%{_datadir}/kamailio/dbtext/kamailio/*


%files mysql
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_mysql
%{_libdir}/kamailio/modules/db_mysql.so
%{_libdir}/kamailio/kamctl/kamctl.mysql
%{_libdir}/kamailio/kamctl/kamdbctl.mysql
%dir %{_datadir}/kamailio/mysql
%{_datadir}/kamailio/mysql/*


%files postgresql
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_postgres
%{_libdir}/kamailio/modules/db_postgres.so
%{_libdir}/kamailio/kamctl/kamctl.pgsql
%{_libdir}/kamailio/kamctl/kamdbctl.pgsql
%dir %{_datadir}/kamailio/postgres
%{_datadir}/kamailio/postgres/*


%files unixODBC
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.db_unixodbc
%{_libdir}/kamailio/modules_k/db_unixodbc.so


%files bdb
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_berkeley
%{_sbindir}/kambdb_recover
%{_libdir}/kamailio/modules/db_berkeley.so
%{_libdir}/kamailio/kamctl/kamctl.db_berkeley
%{_libdir}/kamailio/kamctl/kamdbctl.db_berkeley
%dir %{_datadir}/kamailio/db_berkeley
%{_datadir}/kamailio/db_berkeley/*


%files sqlite
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.db_sqlite
%{_libdir}/kamailio/modules_k/db_sqlite.so
%{_libdir}/kamailio/kamctl/kamctl.sqlite
%{_libdir}/kamailio/kamctl/kamdbctl.sqlite
%dir %{_datadir}/kamailio/db_sqlite
%{_datadir}/kamailio/db_sqlite/*


%files utils
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.utils
%{_libdir}/kamailio/modules/utils.so


%files cpl
%defattr(-,root,root)
%{_docdir}/kamailio/modules_k/README.cpl-c
%{_libdir}/kamailio/modules_k/cpl-c.so


%files snmpstats
%defattr(-,root,root)
%{_docdir}/kamailio/modules_k/README.snmpstats
%{_libdir}/kamailio/modules_k/snmpstats.so


%files presence
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.presence
%doc %{_docdir}/kamailio/modules_k/README.presence_conference
%doc %{_docdir}/kamailio/modules_k/README.presence_dialoginfo
%doc %{_docdir}/kamailio/modules_k/README.presence_mwi
%doc %{_docdir}/kamailio/modules_k/README.presence_profile
%doc %{_docdir}/kamailio/modules_k/README.presence_reginfo
%doc %{_docdir}/kamailio/modules_k/README.presence_xml
%doc %{_docdir}/kamailio/modules_k/README.pua
%doc %{_docdir}/kamailio/modules_k/README.pua_bla
%doc %{_docdir}/kamailio/modules_k/README.pua_dialoginfo
%doc %{_docdir}/kamailio/modules_k/README.pua_mi
%doc %{_docdir}/kamailio/modules_k/README.pua_reginfo
%doc %{_docdir}/kamailio/modules_k/README.pua_usrloc
%doc %{_docdir}/kamailio/modules_k/README.pua_xmpp
%doc %{_docdir}/kamailio/modules_k/README.rls
%doc %{_docdir}/kamailio/modules_k/README.xcap_client
%doc %{_docdir}/kamailio/modules_k/README.xcap_server
%{_libdir}/kamailio/modules_k/presence.so
%{_libdir}/kamailio/modules_k/presence_conference.so
%{_libdir}/kamailio/modules_k/presence_dialoginfo.so
%{_libdir}/kamailio/modules_k/presence_mwi.so
%{_libdir}/kamailio/modules_k/presence_profile.so
%{_libdir}/kamailio/modules_k/presence_reginfo.so
%{_libdir}/kamailio/modules_k/presence_xml.so
%{_libdir}/kamailio/modules_k/pua.so
%{_libdir}/kamailio/modules_k/pua_bla.so
%{_libdir}/kamailio/modules_k/pua_dialoginfo.so
%{_libdir}/kamailio/modules_k/pua_mi.so
%{_libdir}/kamailio/modules_k/pua_reginfo.so
%{_libdir}/kamailio/modules_k/pua_usrloc.so
%{_libdir}/kamailio/modules_k/pua_xmpp.so
%{_libdir}/kamailio/modules_k/rls.so
%{_libdir}/kamailio/modules_k/xcap_client.so
%{_libdir}/kamailio/modules_k/xcap_server.so


%files xmpp
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.xmpp
%{_libdir}/kamailio/modules_k/xmpp.so


%files tls
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.tls
%{_libdir}/kamailio/modules/tls.so


%files purple
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.purple
%{_libdir}/kamailio/modules_k/purple.so


%files ldap
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.h350
%doc %{_docdir}/kamailio/modules_k/README.ldap
%{_libdir}/kamailio/modules_k/h350.so
%{_libdir}/kamailio/modules_k/ldap.so


%files xmlrpc
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmlrpc
%{_libdir}/kamailio/modules/xmlrpc.so


%files perl
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.perl
%doc %{_docdir}/kamailio/modules_k/README.perlvdb
%{_libdir}/kamailio/modules_k/perl.so
%{_libdir}/kamailio/modules_k/perlvdb.so
%dir %{_libdir}/kamailio/perl
%{_libdir}/kamailio/perl/OpenSER.pm
%dir %{_libdir}/kamailio/perl/OpenSER
%{_libdir}/kamailio/perl/OpenSER/Constants.pm
%{_libdir}/kamailio/perl/OpenSER/Message.pm
%{_libdir}/kamailio/perl/OpenSER/VDB.pm
%dir %{_libdir}/kamailio/perl/OpenSER/LDAPUtils
%{_libdir}/kamailio/perl/OpenSER/LDAPUtils/LDAPConf.pm
%{_libdir}/kamailio/perl/OpenSER/LDAPUtils/LDAPConnection.pm
%dir %{_libdir}/kamailio/perl/OpenSER/Utils
%{_libdir}/kamailio/perl/OpenSER/Utils/Debug.pm
%{_libdir}/kamailio/perl/OpenSER/Utils/PhoneNumbers.pm
%dir %{_libdir}/kamailio/perl/OpenSER/VDB
%{_libdir}/kamailio/perl/OpenSER/VDB/Column.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Pair.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/ReqCond.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Result.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/VTab.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Value.pm
%dir %{_libdir}/kamailio/perl/OpenSER/VDB/Adapter
%{_libdir}/kamailio/perl/OpenSER/VDB/Adapter/AccountingSIPtrace.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Adapter/Alias.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Adapter/Auth.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Adapter/Describe.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Adapter/Speeddial.pm
%{_libdir}/kamailio/perl/OpenSER/VDB/Adapter/TableVersions.pm


%files lua
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_lua
%{_libdir}/kamailio/modules/app_lua.so


%files python
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_python
%{_libdir}/kamailio/modules/app_python.so


%files regex
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules_k/README.regex
%{_libdir}/kamailio/modules_k/regex.so


%files dialplan
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.dialplan
%{_libdir}/kamailio/modules/dialplan.so


%files lcr
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.lcr
%{_libdir}/kamailio/modules/lcr.so


%files xmlops
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmlops
%{_libdir}/kamailio/modules/xmlops.so


%if 0%{?fedora}
%files radius
%defattr(-,root,root)
%{_docdir}/kamailio/modules_k/README.acc_radius
%{_docdir}/kamailio/modules_k/README.auth_radius
%{_docdir}/kamailio/modules_k/README.misc_radius
%{_docdir}/kamailio/modules/README.peering
%{_libdir}/kamailio/modules_k/acc_radius.so
%{_libdir}/kamailio/modules_k/auth_radius.so
%{_libdir}/kamailio/modules_k/misc_radius.so
%{_libdir}/kamailio/modules/peering.so


%files carrierroute
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.carrierroute
%{_libdir}/kamailio/modules/carrierroute.so


%files redis
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.ndb_redis
%{_libdir}/kamailio/modules/ndb_redis.so


%files json
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.json
%doc %{_docdir}/kamailio/modules/README.jsonrpc-c
%{_libdir}/kamailio/modules/json.so
%{_libdir}/kamailio/modules/jsonrpc-c.so


%files mono
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_mono
%{_libdir}/kamailio/modules/app_mono.so


%files GeoIP
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.geoip
%{_libdir}/kamailio/modules/geoip.so
%endif



%changelog
* Tue Mar 5 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated ver to 3.3.4
* Wed Dec 12 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated ver to 3.3.3 in preparation for next release
* Fri Oct 20 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Set ownership of /etc/kamailio to kamailio.kamailio
  - Added installation of auth.7.gz for Fedora now that manpages are built for
    Fedora
  - Added "make utils" to the build section (when it's not there utils get
    built during the install - which isn't right)
* Tue Oct 16 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated ver to 3.3.2
* Fri Aug 3 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated ver to 3.3.1
* Sat Jun 30 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Removed %_sharedir and replaced with standard macro %_datadir
* Mon Jun 17 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to 0 in preparation for 3.3.0 release
* Mon Jun 11 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to rc0
* Mon Jun 4 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added a number of %dir lines to make sure the RPMs are properly cleaned up
    on uninstall
* Sat Jun 2 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added %postun section to reload systemd on Fedora after uninstall
  - Added build requirement for redhat-rpm-config so debuginfo RPMs get built
* Fri Jun 1 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Tweak to the pkg/kamailio/fedora directory structure
  - Tested with Fedora 17
* Thu May 31 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to pre3
  - Combined Fedora/CentOS .spec in preparation for Fedora 17
* Sun May 20 2012 Peter Dunkley <peter@dunkley.me.uk>
  - First version created for Kamailio 3.3.0. Based on spec-file for Fedora
    created by myself (in turn based on an older spec-file for CentOS created
    by Ovidiu Sas).
  - Tested with CentOS 6.2 x86_64.
  - Builds all Kamailio 3.3.0 modules (modules/modules_k) except:
    - modules/app_mono: Requires mono which is not in the CentOS 6 repo
    - modules/auth_identity: Conflicts with TLS unless statically linked (which
      requires changes to Makefile and is impractical for generic RPM building)
    - modules/db_cassandra: Requires Cassandra and Thrift which are not in the
      CentOS 6 repo
    - modules/geoip: Requires GeoIP which is not in the CentOS 6 repo
    - modules/iptrtpproxy: Needs local copy of iptables source to build
      (impractical for generic RPM building)
    - modules/json: Requires json-c whish is not in the CentOS 6 repo
    - modules/jsonrpc-c: Requires json-c whish is not in the CentOS 6 repo
    - modules/ndb_redis: Requires hiredis which is not in the CentOS 6 repo
    - modules/peering: Requires radiusclient-ng which is not in the CentOS 6
      repo
    - modules_k/acc_radius: Requires radiusclient-ng which is not in the CentOS
      6 repo
    - modules_k/auth_radius: Required radiusclient-ng which is not in the
      CentOS 6 repo
    - modules_k/carrierroute: Requires libconfuse which is not in the CentOS 6
      repo
    - modules_k/db_oracle: Requires Oracle which is not in the CentOS 6 repo
      (and is closed-source)
    - modules_k/memcached: Module compilation appears to require an older
      version of libmemcached-devel than the one in the CentOS 6 repo
    - modules_k/mi_xmlrpc: Requires libxmlrpc-c3 which is not in the CentOS 6
      repo
    - modules_k/misc_radius: Requires radiusclient-ng which is not in the
      CentOS 6 repo
    - modules_k/osp: Requires OSP Toolkit which is not in the CentOS 6 repo
* Fri May 18 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added missing BuildRequires (gcc).
  - Added .fc16 to rel.  This makes it easy to tell which distribution the RPMs
    are built for.
* Thu May 17 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to pre2.
* Mon May 7 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Changed to use systemd instead of SysV init.
* Sun May 6 2012 Peter Dunkley <peter@dunkley.me.uk>
  - First version created for Kamailio 3.3.0. Based on spec-file for CentOS
    created by Ovidiu Sas.
  - Tested with Fedora 16 x86_64.
  - Builds all Kamailio 3.3.0 modules (modules/modules_k) except:
    - modules/auth_identity: Conflicts with TLS unless statically linked (which
      requires changes to Makefile and is impractical for generic RPM building)
    - modules/db_cassandra: Requires Thrift which is not in the F16 repo
    - modules/iptrtpproxy: Needs local copy of iptables source to build
      (impractical for generic RPM building)
    - modules_k/db_oracle: Requires Oracle which is not in the F16 repo
      (and is closed-source)
    - modules_k/memcached: Module compilation appears to require an older
      version of libmemcached-devel than the one in the F16 repo
    - modules_k/mi_xmlrpc: The F16 repo contains an unsupported version of
      libxmlrpc-c3, and there is an compilation error due to the module code
      using an unknown type ('TString')
    - modules_k/osp: Requires OSP Toolkit which is not in the F16 repo
