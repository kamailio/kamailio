%define	EXCLUDE_MODULES	mi_xmlrpc osp

Summary:	Open Source SIP Server
Name:		openser
Version:	1.3.0
Release:	1%{?dist}
License:	GPLv2+
Group:		System Environment/Daemons
Source0:	http://www.openser.org/pub/%{name}/%{version}/src/%{name}-%{version}-tls_src.tar.gz
Source1:	openser.init
Patch1:		openser--acc_radius_enable.diff
Patch3:		openser--openssl-paths.diff
URL:		http://www.openser.org/

BuildRequires:	expat-devel
BuildRequires:	libxml2-devel
BuildRequires: 	bison
BuildRequires: 	flex
# needed by snmpstats
BuildRequires:	radiusclient-ng-devel
BuildRequires:	mysql-devel
BuildRequires:	postgresql-devel
# required by snmpstats module
BuildRequires:	lm_sensors-devel
BuildRequires:	net-snmp-devel
BuildRequires:	unixODBC-devel
BuildRequires:	openssl-devel
BuildRequires:	expat-devel
#BuildRequires:	xmlrpc-c-devel
BuildRequires:	libconfuse-devel
BuildRequires:	db4-devel
BuildRequires:	openldap-devel
BuildRequires:	curl-devel

Requires(post):	/sbin/chkconfig
Requires(preun):/sbin/chkconfig
Requires(preun):/sbin/service
BuildRoot: 	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
OpenSER or OPEN SIP Express Router is a very fast and flexible SIP (RFC3261)
proxy server. Written entirely in C, openser can handle thousands calls
per second even on low-budget hardware. A C Shell like scripting language
provides full control over the server's behaviour. It's modular
architecture allows only required functionality to be loaded.
Currently the following modules are available: digest authentication,
CPL scripts, instant messaging, MySQL and UNIXODBC support, a presence agent,
radius authentication, record routing, an SMS gateway, a jabber gateway, a 
transaction and dialog module, OSP module, statistics support, 
registrar and user location.

%package	acc
Summary:	Accounts transactions information to different backends
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Conflicts:	acc_radius

%description	acc
ACC module is used to account transactions information to different backends 
like syslog, SQL.

%package	acc_radius
Summary:	Accounts transactions information with radius support
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Conflicts:	acc

%description	acc_radius
ACC module is used to account transactions information to different backends
like syslog, SQL, RADIUS, DIAMETER.

%package	auth_diameter
Summary:	Performs authentication using a Diameter server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	auth_diameter
This module implements SIP authentication and authorization with DIAMETER
server, namely DIameter Server Client (DISC).

%package	auth_radius
Summary:	Performs authentication using a Radius server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	auth_radius
This module contains functions that are used to perform authentication using
a Radius server.  Basically the proxy will pass along the credentials to the
radius server which will in turn send a reply containing result of the
authentication. So basically the whole authentication is done in the Radius
server.

%package	avp_radius
Summary:	Allows loading of user's attributes into AVPs from Radius
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	avp_radius
This module allows loading of user's attributes into AVPs from Radius.
User's name and domain can be based on From URI, Request URI, or
authenticated credentials.

%package	carrierroute
Summary:	Routing extension suitable for carriers
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	carrierroute
A module which provides routing, balancing and blacklisting capabilities.

%package	cpl-c
Summary:	Call Processing Language interpreter
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	cpl-c
This module implements a CPL (Call Processing Language) interpreter.
Support for uploading/downloading/removing scripts via SIP REGISTER method
is present.

%package	group_radius
Summary:	Functions necessary for group membership checking over radius
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	group_radius
This module export functions necessary for group membership checking over
radius. There is a database table that contains list of users and groups
they belong to. The table is used by functions of this module.

%package	db_berkeley
Summary:	Berkley DB backend support
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	db_berkeley
This is a module which integrates the Berkeley DB into OpenSER. It implements 
the DB API defined in OpenSER.

%package	h350
Summary:	H350 implementation	
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	h350
The OpenSER H350 module enables an OpenSER SIP proxy server to access SIP 
account data stored in an LDAP [RFC4510] directory  containing H.350 [H.350] 
commObjects. 

%package	jabber
Summary:	Gateway between OpenSER and a jabber server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description 	jabber
Jabber module that integrates XODE XML parser for parsing Jabber messages.

%package	ldap
Summary:	LDAP connector
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	ldap
The LDAP module implements an LDAP search interface for OpenSER.

%package	mysql
Summary:	MySQL Storage Support for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description 	mysql
The %{name}-mysql package contains the MySQL plugin for %{name}, which allows
a MySQL-Database to be used for persistent storage.

%package 	perl
Summary:	Helps implement your own OpenSER extensions in Perl
Group:		System Environment/Daemons
# require perl-devel for >F7 and perl for <=F6
BuildRequires:	perl(ExtUtils::MakeMaker)
Requires:	%{name} = %{version}-%{release}
Requires:	perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%description	perl
The time needed when writing a new OpenSER module unfortunately is quite
high, while the options provided by the configuration file are limited to
the features implemented in the modules. With this Perl module, you can
easily implement your own OpenSER extensions in Perl.  This allows for
simple access to the full world of CPAN modules. SIP URI rewriting could be
implemented based on regular expressions; accessing arbitrary data backends,
e.g. LDAP or Berkeley DB files, is now extremely simple.

%package	perlvdb
Summary:	Perl virtual database engine
Group:		System Environment/Daemons
# require perl-devel for >F7 and perl for <=F6
BuildRequires:	perl(ExtUtils::MakeMaker)
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-perl
Requires:	perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%description	perlvdb
The Perl Virtual Database (VDB) provides a virtualization framework for 
OpenSER's database access. It does not handle a particular database engine 
itself but lets the user relay database requests to arbitrary Perl functions.

%package	postgresql
Summary:	PostgreSQL Storage Support for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	postgresql
The %{name}-postgresql package contains the PostgreSQL plugin for %{name},
which allows a PostgreSQL-Database to be used for persistent storage.

%package	presence
Summary:	Presence server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	presence
This module implements a presence server. It handles PUBLISH and SUBSCRIBE
messages and generates NOTIFY messages. It offers support for aggregation
of published presence information for the same presentity using more devices.
It can also filter the information provided to watchers according to privacy
rules.

%package	presence_mwi
Summary:	Extension to Presence server for Message Waiting Indication
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-presence

%description	presence_mwi
The module does specific handling for notify-subscribe message-summary 
(message waiting indication) events as specified in RFC 3842. It is used 
with the general event handling module, presence. It constructs and adds 
message-summary event to it.

%package	presence_xml
Summary:	SIMPLE Presence extension
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-presence
Requires:	%{name}-xcap_client

%description	presence_xml
The module does specific handling for notify-subscribe events using xml bodies. 
It is used with the general event handling module, presence.

%package	pua
Summary:	Offer the functionality of a presence user agent client
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	pua
This module offer the functionality of a presence user agent client, sending
Subscribe and Publish messages.

%package	pua_bla
Summary:	BLA extension for PUA
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua
Requires:	%{name}-presence

%description	pua_bla
The pua_bla module enables Bridged Line Appearances support according to the 
specifications in draft-anil-sipping-bla-03.txt.

%package	pua_mi
Summary:	Connector between usrloc and MI interface
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua

%description	pua_mi
The pua_mi sends offer the possibility to publish presence information
via MI transports.  Using this module you can create independent
applications/scripts to publish not sip-related information (e.g., system
resources like CPU-usage, memory, number of active subscribers ...)

%package	pua_usrloc
Summary:	Connector between usrloc and pua modules
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua

%description	pua_usrloc
This module is the connector between usrloc and pua modules. It creates the
environment to send PUBLISH requests for user location records, on specific
events (e.g., when new record is added in usrloc, a PUBLISH with status open
(online) is issued; when expires, it sends closed (offline)). Using this
module, phones which have no support for presence can be seen as
online/offline.

%package	pua_xmpp
Summary:	SIMPLE-XMPP Presence gateway 
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua
Requires:	%{name}-presence
Requires:	%{name}-xmpp

%description	pua_xmpp
This module is a gateway for presence between SIP and XMPP. It translates one 
format into another and uses xmpp, pua and presence modules to manage the 
transmition of presence state information.

%package	rls
Summary:	Resource List Server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua
Requires:	%{name}-presence

%description	rls
The modules is a Resource List Server implementation following the 
specification in RFC 4662 and RFC 4826.

%package	seas
Summary:	Transfers the execution logic control to a given external entity
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	seas
SEAS module enables OpenSER to transfer the execution logic control of a sip
message to a given external entity, called the Application Server. When the
OpenSER script is being executed on an incoming SIP message, invocation of
the as_relay_t() function makes this module send the message along with some
transaction information to the specified Application Server. The Application
Server then executes some call-control logic code, and tells OpenSER to take
some actions, ie. forward the message downstream, or respond to the message
with a SIP repy, etc

%package	sms
Summary:	Gateway between SIP and GSM networks via sms
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	sms
This module provides a way of communication between SIP network (via SIP
MESSAGE) and GSM networks (via ShortMessageService). Communication is
possible from SIP to SMS and vice versa.  The module provides facilities
like SMS confirmation--the gateway can confirm to the SIP user if his
message really reached its destination as a SMS--or multi-part messages--if
a SIP messages is too long it will be split and sent as multiple SMS.

%package	snmpstats
Summary:	SNMP management interface for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	snmpstats
The %{name}-snmpstats package provides an SNMP management interface to
OpenSER.  Specifically, it provides general SNMP queryable scalar statistics,
table representations of more complicated data such as user and contact
information, and alarm monitoring capabilities.

%package	tlsops
Summary:	TLS-relating functions for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	tlsops
The %{name}-tlsops package implements TLS related functions to use in the
routing script, and exports pseudo variables with certificate and TLS
parameters.

%package	unixodbc
Summary:	OpenSER unixODBC Storage support
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	unixodbc
The %{name}-unixodbc package contains the unixODBC plugin for %{name}, which
allows a unixODBC to be used for persistent storage

%package	uri_radius
Summary:	URI check using Radius server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	uri_radius
URI check using Radius server

%package	xcap_client
Summary:	XCAP client
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	xcap_client
The modules is an XCAP client for OpenSER that can be used by other modules.
It fetches XCAP elements, either documents or part of them, by sending HTTP 
GET requests. It also offers support for conditional queries. It uses libcurl 
library as a client-side HTTP transfer library.

%package	xmpp
Summary:	Gateway between OpenSER and a jabber server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	xmpp
This modules is a gateway between Openser and a jabber server. It enables
the exchange of instant messages between SIP clients and XMPP(jabber)
clients.

%prep
%setup -q -n %{name}-%{version}-tls
cp -pRf modules/acc modules/acc_radius
%patch1
%patch3

%build
LOCALBASE=/usr CFLAGS="%{optflags}" %{__make} all %{?_smp_mflags} TLS=1 \
  exclude_modules="%EXCLUDE_MODULES" \
  cfg-target=%{_sysconfdir}/openser/ \
  modules-dir=%{_lib}/openser/modules

%install
rm -rf $RPM_BUILD_ROOT
%{__make} install TLS=1 exclude_modules="%EXCLUDE_MODULES" \
  basedir=%{buildroot} prefix=%{_prefix} \
  cfg-prefix=%{buildroot} \
  modules-dir=%{_lib}/openser/modules
cp -pf modules/acc_radius/acc.so \
  %{buildroot}/%{_libdir}/openser/modules/acc_radius.so
chmod 0755 %{buildroot}/%{_libdir}/openser/modules/acc_radius.so

# clean some things
mkdir -p $RPM_BUILD_ROOT/%{perl_vendorlib}
mv $RPM_BUILD_ROOT/%{_libdir}/openser/perl/* \
  $RPM_BUILD_ROOT/%{perl_vendorlib}/
mv $RPM_BUILD_ROOT/%{_sysconfdir}/openser/tls/README \
  $RPM_BUILD_ROOT/%{_docdir}/openser/README.tls
rm -f $RPM_BUILD_ROOT%{_docdir}/openser/INSTALL
mv $RPM_BUILD_ROOT/%{_docdir}/openser docdir

# recode documentation
for i in docdir/*; do
  mv -f $i $i.old
  iconv -f iso8859-1 -t UTF-8 $i.old > $i
  rm -f $i.old
done

mkdir -p $RPM_BUILD_ROOT%{_initrddir}
%{__install} -p -D -m 755 %{SOURCE1} \
  $RPM_BUILD_ROOT%{_initrddir}/openser
echo -e "\nETCDIR=\"%{_sysconfdir}/openser\"\n" \
  >> $RPM_BUILD_ROOT%{_sysconfdir}/%{name}/kamctlrc

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/chkconfig --add openser

%preun
if [ $1 = 0 ]; then
 /sbin/service openser stop > /dev/null 2>&1
 /sbin/chkconfig --del openser
fi

%files
%defattr(-,root,root,-)
%{_sbindir}/openser
%{_sbindir}/kamctl
%{_sbindir}/kamdbctl
%{_sbindir}/kamunix

%dir %{_sysconfdir}/openser
%dir %{_sysconfdir}/openser/tls
%dir %{_libdir}/openser/
%dir %{_libdir}/openser/modules/
%dir %{_libdir}/openser/kamctl/

%attr(755,root,root) %{_initrddir}/openser

%config(noreplace) %{_sysconfdir}/openser/dictionary.radius
%config(noreplace) %{_sysconfdir}/openser/kamailio.cfg
%config(noreplace) %{_sysconfdir}/openser/kamctlrc

%config(noreplace) %{_sysconfdir}/openser/tls/ca.conf
%config(noreplace) %{_sysconfdir}/openser/tls/request.conf
%config(noreplace) %{_sysconfdir}/openser/tls/rootCA/cacert.pem
%config(noreplace) %{_sysconfdir}/openser/tls/rootCA/certs/01.pem
%config(noreplace) %{_sysconfdir}/openser/tls/rootCA/index.txt
%config(noreplace) %{_sysconfdir}/openser/tls/rootCA/private/cakey.pem
%config(noreplace) %{_sysconfdir}/openser/tls/rootCA/serial
%config(noreplace) %{_sysconfdir}/openser/tls/user.conf
%config(noreplace) %{_sysconfdir}/openser/tls/user/user-calist.pem
%config(noreplace) %{_sysconfdir}/openser/tls/user/user-cert.pem
%config(noreplace) %{_sysconfdir}/openser/tls/user/user-cert_req.pem
%config(noreplace) %{_sysconfdir}/openser/tls/user/user-privkey.pem

%{_libdir}/openser/kamctl/kamctl.*
%{_libdir}/openser/kamctl/kamdbctl.base
%{_libdir}/openser/kamctl/kamdbctl.dbtext

%{_datadir}/openser/dbtext/openser/*

%{_mandir}/man5/kamailio.cfg.5*
%{_mandir}/man8/openser.8*
%{_mandir}/man8/kamctl.8*
%{_mandir}/man8/kamunix.8*

%doc docdir/AUTHORS
%doc docdir/NEWS
%doc docdir/README
%doc docdir/README-MODULES
%doc docdir/README.tls

%{_libdir}/openser/modules/alias_db.so
%{_libdir}/openser/modules/auth.so
%{_libdir}/openser/modules/auth_db.so
%{_libdir}/openser/modules/avpops.so
%{_libdir}/openser/modules/benchmark.so
%{_libdir}/openser/modules/cfgutils.so
%{_libdir}/openser/modules/dbtext.so
%{_libdir}/openser/modules/dialog.so
%{_libdir}/openser/modules/dispatcher.so
%{_libdir}/openser/modules/diversion.so
%{_libdir}/openser/modules/domain.so
%{_libdir}/openser/modules/domainpolicy.so
%{_libdir}/openser/modules/enum.so
%{_libdir}/openser/modules/exec.so
%{_libdir}/openser/modules/flatstore.so
%{_libdir}/openser/modules/gflags.so
%{_libdir}/openser/modules/group.so
%{_libdir}/openser/modules/imc.so
%{_libdir}/openser/modules/lcr.so
%{_libdir}/openser/modules/mangler.so
%{_libdir}/openser/modules/maxfwd.so
%{_libdir}/openser/modules/mediaproxy.so
%{_libdir}/openser/modules/mi_fifo.so
%{_libdir}/openser/modules/mi_datagram.so
%{_libdir}/openser/modules/msilo.so
%{_libdir}/openser/modules/nathelper.so
%{_libdir}/openser/modules/options.so
%{_libdir}/openser/modules/path.so
%{_libdir}/openser/modules/pdt.so
%{_libdir}/openser/modules/permissions.so
%{_libdir}/openser/modules/pike.so
%{_libdir}/openser/modules/registrar.so
%{_libdir}/openser/modules/rr.so
%{_libdir}/openser/modules/siptrace.so
%{_libdir}/openser/modules/sl.so
%{_libdir}/openser/modules/speeddial.so
%{_libdir}/openser/modules/sst.so
%{_libdir}/openser/modules/statistics.so
%{_libdir}/openser/modules/textops.so
%{_libdir}/openser/modules/tm.so
%{_libdir}/openser/modules/uac.so
%{_libdir}/openser/modules/uac_redirect.so
%{_libdir}/openser/modules/uri.so
%{_libdir}/openser/modules/uri_db.so
%{_libdir}/openser/modules/usrloc.so
%{_libdir}/openser/modules/xlog.so

%doc docdir/README.alias_db
%doc docdir/README.auth
%doc docdir/README.auth_db
%doc docdir/README.avpops
%doc docdir/README.benchmark
%doc docdir/README.cfgutils
%doc docdir/README.dbtext
%doc docdir/README.dialog
%doc docdir/README.dispatcher
%doc docdir/README.diversion
%doc docdir/README.domain
%doc docdir/README.domainpolicy
%doc docdir/README.enum
%doc docdir/README.exec
%doc docdir/README.flatstore
%doc docdir/README.gflags
%doc docdir/README.group
%doc docdir/README.imc
%doc docdir/README.lcr
%doc docdir/README.mangler
%doc docdir/README.maxfwd
%doc docdir/README.mediaproxy
%doc docdir/README.mi_fifo
%doc docdir/README.mi_datagram
%doc docdir/README.msilo
%doc docdir/README.nathelper
%doc docdir/README.options
%doc docdir/README.path
%doc docdir/README.pdt
%doc docdir/README.permissions
%doc docdir/README.pike
%doc docdir/README.registrar
%doc docdir/README.rr
%doc docdir/README.siptrace
%doc docdir/README.sl
%doc docdir/README.speeddial
%doc docdir/README.sst
%doc docdir/README.statistics
%doc docdir/README.textops
%doc docdir/README.tm
%doc docdir/README.uac
%doc docdir/README.uac_redirect
%doc docdir/README.uri
%doc docdir/README.uri_db
%doc docdir/README.usrloc
%doc docdir/README.xlog

%files acc
%defattr(-,root,root,-)
%{_libdir}/openser/modules/acc.so
%doc docdir/README.acc

%files acc_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/acc_radius.so
%doc docdir/README.acc_radius

%files auth_diameter
%defattr(-,root,root,-)
%{_libdir}/openser/modules/auth_diameter.so
%doc docdir/README.auth_diameter

%files auth_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/auth_radius.so
%doc docdir/README.auth_radius

%files avp_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/avp_radius.so
%doc docdir/README.avp_radius

%files carrierroute
%defattr(-,root,root,-)
%{_libdir}/openser/modules/carrierroute.so
%doc docdir/README.carrierroute

%files cpl-c
%defattr(-,root,root,-)
%{_libdir}/openser/modules/cpl-c.so
%doc docdir/README.cpl-c

%files db_berkeley
%defattr(-,root,root,-)
%{_sbindir}/bdb_recover
%{_libdir}/openser/modules/db_berkeley.so
%{_libdir}/openser/kamctl/kamdbctl.db_berkeley
%{_datadir}/openser/db_berkeley/openser/*
%doc docdir/README.db_berkeley

%files group_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/group_radius.so
%doc docdir/README.group_radius

%files h350
%defattr(-,root,root,-)
%{_libdir}/openser/modules/h350.so
%doc docdir/README.h350

%files jabber
%defattr(-,root,root,-)
%{_libdir}/openser/modules/jabber.so
%doc docdir/README.jabber

%files ldap
%defattr(-,root,root,-)
%{_libdir}/openser/modules/ldap.so
%doc docdir/README.ldap

%files mysql
%defattr(-,root,root,-)
%{_libdir}/openser/modules/mysql.so
%{_libdir}/openser/kamctl/kamdbctl.mysql
%{_datadir}/openser/mysql/*.sql
%doc docdir/README.mysql

%files perl
%defattr(-,root,root,-)
%dir %{perl_vendorlib}/OpenSER
%dir %{perl_vendorlib}/OpenSER/LDAPUtils
%dir %{perl_vendorlib}/OpenSER/Utils
%{_libdir}/openser/modules/perl.so
%{perl_vendorlib}/OpenSER.pm
%{perl_vendorlib}/OpenSER/Constants.pm
%{perl_vendorlib}/OpenSER/LDAPUtils/LDAPConf.pm
%{perl_vendorlib}/OpenSER/LDAPUtils/LDAPConnection.pm
%{perl_vendorlib}/OpenSER/Message.pm
%{perl_vendorlib}/OpenSER/Utils/PhoneNumbers.pm
%{perl_vendorlib}/OpenSER/Utils/Debug.pm
%doc docdir/README.perl

%files perlvdb
%defattr(-,root,root,-)
%dir %{perl_vendorlib}/OpenSER/VDB
%dir %{perl_vendorlib}/OpenSER/VDB/Adapter
%{_libdir}/openser/modules/perlvdb.so
%{perl_vendorlib}/OpenSER/VDB.pm
%{perl_vendorlib}/OpenSER/VDB/Adapter/AccountingSIPtrace.pm
%{perl_vendorlib}/OpenSER/VDB/Adapter/Alias.pm
%{perl_vendorlib}/OpenSER/VDB/Adapter/Auth.pm
%{perl_vendorlib}/OpenSER/VDB/Adapter/Describe.pm
%{perl_vendorlib}/OpenSER/VDB/Adapter/Speeddial.pm
%{perl_vendorlib}/OpenSER/VDB/Adapter/TableVersions.pm
%{perl_vendorlib}/OpenSER/VDB/Column.pm
%{perl_vendorlib}/OpenSER/VDB/Pair.pm
%{perl_vendorlib}/OpenSER/VDB/ReqCond.pm
%{perl_vendorlib}/OpenSER/VDB/Result.pm
%{perl_vendorlib}/OpenSER/VDB/VTab.pm
%{perl_vendorlib}/OpenSER/VDB/Value.pm
%doc docdir/README.perlvdb

%files postgresql
%defattr(-,root,root,-)
%{_libdir}/openser/modules/postgres.so
%{_libdir}/openser/kamctl/kamdbctl.pgsql
%{_datadir}/openser/postgres/*.sql
%doc docdir/README.postgres

%files presence
%defattr(-,root,root,-)
%{_libdir}/openser/modules/presence.so
%doc docdir/README.presence

%files presence_mwi
%defattr(-,root,root,-)
%{_libdir}/openser/modules/presence_mwi.so
%doc docdir/README.presence_mwi

%files presence_xml
%defattr(-,root,root,-)
%{_libdir}/openser/modules/presence_xml.so
%doc docdir/README.presence_xml

%files pua
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua.so
%doc docdir/README.pua

%files pua_bla
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua_bla.so
%doc docdir/README.pua_bla

%files pua_mi
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua_mi.so
%doc docdir/README.pua_mi

%files pua_usrloc
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua_usrloc.so
%doc docdir/README.pua_usrloc

%files pua_xmpp
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua_xmpp.so
%doc docdir/README.pua_xmpp

%files rls
%defattr(-,root,root,-)
%{_libdir}/openser/modules/rls.so
%doc docdir/README.rls

%files seas
%defattr(-,root,root,-)
%{_libdir}/openser/modules/seas.so
%doc docdir/README.seas

%files sms
%defattr(-,root,root,-)
%{_libdir}/openser/modules/sms.so
%doc docdir/README.sms

%files snmpstats
%defattr(-,root,root,-)
%{_libdir}/openser/modules/snmpstats.so
%doc docdir/README.snmpstats
%{_datadir}/snmp/mibs/OPENSER-MIB
%{_datadir}/snmp/mibs/OPENSER-REG-MIB
%{_datadir}/snmp/mibs/OPENSER-SIP-COMMON-MIB
%{_datadir}/snmp/mibs/OPENSER-SIP-SERVER-MIB
%{_datadir}/snmp/mibs/OPENSER-TC

%files tlsops
%defattr(-,root,root,-)
%{_libdir}/openser/modules/tlsops.so
%doc docdir/README.tlsops

%files uri_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/uri_radius.so
%doc docdir/README.uri_radius

%files unixodbc
%defattr(-,root,root,-)
%{_libdir}/openser/modules/unixodbc.so
%doc docdir/README.unixodbc

%files xcap_client
%defattr(-,root,root,-)
%{_libdir}/openser/modules/xcap_client.so
%doc docdir/README.xcap_client

%files xmpp
%defattr(-,root,root,-)
%{_libdir}/openser/modules/xmpp.so
%doc docdir/README.xmpp

%changelog

* Thu Dec 13 2007 Peter Lemenkov <lemenkov@gmail.com> 1.3.0-1
- Final ver. 1.3.0
- Removed some leftovers from spec-file

* Wed Dec 12 2007 Peter Lemenkov <lemenkov@gmail.com> 1.3.0-0.1.pre1
- Latest snapshot - 1.3.0pre1

* Mon Dec 10 2007 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> 1.2.2-11
- added ETCDIR into kamctlrc (need openser-1.3 to work)

* Mon Sep 24 2007 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> 1.2.2-10
- perl scripts moved to perl_vendorlib directory
- added LDAPUtils and Utils subdirectories
- changed perl module BuildRequires

* Mon Sep 24 2007 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> 1.2.2-9
- added reload section to init script
- init script specified with initrddir macro
- documentation converted to UTF-8
- added doc macro for documentation
- documentation moved do proper place (/usr/share/doc/NAME-VERSION/)
- which removed from BuildRequires, it's in guidelines exceptions
- unixodbc subpackage summary update

* Thu Sep  6 2007 Peter Lemenkov <lemenkov@gmail.com> 1.2.2-8
- Added another one missing BR - which (needs by snmpstats module)
- Cosmetic: dropped commented out 'Requires'
 
* Thu Sep 06 2007 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> 1.2.2-7
- added attr macro for init script
- added -p to install arguments to preserve timestamp
- parallel make used

* Sun Aug 26 2007 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> 1.2.2-6
- Fedora Core 6 build updates
- changed attributes for openser.init to be rpmlint more silent

* Sun Aug 26 2007 Peter Lemenkov <lemenkov@gmail.com> 1.2.2-5
- fixed paths for openssl libs and includes

* Sun Aug 26 2007 Peter Lemenkov <lemenkov@gmail.com> 1.2.2-4
- Introduced acc and acc_radius modules (Jan Ondrej)
- Dropped radius_accounting condition

* Sat Aug 25 2007 Peter Lemenkov <lemenkov@gmail.com> 1.2.2-3
- Changed license according to Fedora's policy
- Make rpmlint more silent

* Fri Aug 24 2007 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> 1.2.2-2
- added openser.init script
- removed Patch0: openser--Makefile.diff and updated build section
- spec file is 80 characters wide
- added radius_accounting condition

* Wed Aug 22 2007 Peter Lemenkov <lemenkov@gmail.com> 1.2.2-1
- Ver. 1.2.2

* Tue Jul 24 2007 Peter Lemenkov <lemenkov@gmail.com> 1.2.1-1
- Initial spec.
