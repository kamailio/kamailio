%define	EXCLUDE_MODULES	pa mi_xmlrpc osp

Summary:	Open Source SIP Server
Name:		openser
Version:	1.2.2
Release:	6%{?dist}
License:	GPLv2+
Group:		System Environment/Daemons
Source0:	http://www.openser.org/pub/%{name}/%{version}/src/%{name}-%{version}-tls_src.tar.gz
Source1:	openser.init
Patch1:		openser--acc_radius_enable.diff
#Patch2:		openser--mi_xmlrpc-correct_include.diff
Patch3:		openser--openssl-paths.diff
URL:		http://www.openser.org/

BuildRequires:	expat-devel
BuildRequires:	libxml2-devel
BuildRequires: 	bison
BuildRequires: 	flex
BuildRequires:	radiusclient-ng-devel
BuildRequires:	mysql-devel
BuildRequires:	postgresql-devel
%if "%{?fedora}" >= "7"
BuildRequires:	perl-devel
%else
# required by snmpstats module
BuildRequires:	lm_sensors-devel
%endif
BuildRequires:	net-snmp-devel
BuildRequires:	unixODBC-devel
BuildRequires:	openssl-devel
BuildRequires:	expat-devel
#BuildRequires:	xmlrpc-c-devel

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
#Requires:	%{name}-tm
#Requires:	%{name}-rr
#Requires:	database // TODO
Conflicts:	acc_radius

%description	acc
ACC module is used to account transactions information to different backends 
like syslog, SQL.

%package	acc_radius
Summary:	Accounts transactions information with radius support
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-tm
#Requires:	%{name}-rr
#Requires:	database // TODO
Conflicts:	acc

%description	acc_radius
ACC module is used to account transactions information to different backends
like syslog, SQL, RADIUS, DIAMETER.

%package	mysql
Summary:	MySQL Storage Support for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description 	mysql
The %{name}-mysql package contains the MySQL plugin for %{name}, which allows
a MySQL-Database to be used for persistent storage.

%package	postgresql
Summary:	PostgreSQL Storage Support for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	postgresql
The %{name}-postgresql package contains the PostgreSQL plugin for %{name},
which allows a PostgreSQL-Database to be used for persistent storage.

%package	unixodbc
Summary:	unixODBC Storage support for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	unixodbc
The %{name}-unixodbc package contains the unixODBC plugin for %{name}, which
allows a unixODBC to be used for persistent storage

%package	snmpstats
Summary:	SNMP management interface for the OpenSER
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-dialog
#Requires:	%{name}-usrloc

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

%package 	perl
Summary:	Helps implement your own OpenSER extensions in Perl
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-sl

%description	perl
The time needed when writing a new OpenSER module unfortunately is quite
high, while the options provided by the configuration file are limited to
the features implemented in the modules. With this Perl module, you can
easily implement your own OpenSER extensions in Perl.  This allows for
simple access to the full world of CPAN modules. SIP URI rewriting could be
implemented based on regular expressions; accessing arbitrary data backends,
e.g. LDAP or Berkeley DB files, is now extremely simple.

%package	xmpp
Summary:	Gateway between OpenSER and a jabber server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-tm

%description	xmpp
This modules is a gateway between Openser and a jabber server. It enables
the exchange of instant messages between SIP clients and XMPP(jabber)
clients.

%package	jabber
Summary:	Gateway between OpenSER and a jabber server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	database // TODO
#Requires:	%{name}-pa
#Requires:	%{name}-tm

%description 	jabber
Jabber module that integrates XODE XML parser for parsing Jabber messages.

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

%package	auth_radius
Summary:	Performs authentication using a Radius server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-auth

%description	auth_radius
This module contains functions that are used to perform authentication using
a Radius server.  Basically the proxy will pass along the credentials to the
radius server which will in turn send a reply containing result of the
authentication. So basically the whole authentication is done in the Radius
server.

%package	auth_diameter
Summary:	Performs authentication using a Diameter server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-sl

%description	auth_diameter
This module implements SIP authentication and authorization with DIAMETER
server, namely DIameter Server Client (DISC).

%package	group_radius
Summary:	Functions necessary for group membership checking over radius
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	group_radius
This module export functions necessary for group membership checking over
radius. There is a database table that contains list of users and groups
they belong to. The table is used by functions of this module.

%package	seas
Summary:	Transfers the execution logic control to a given external entity
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	%{name}-tm

%description	seas
SEAS module enables OpenSER to transfer the execution logic control of a sip
message to a given external entity, called the Application Server. When the
OpenSER script is being executed on an incoming SIP message, invocation of
the as_relay_t() function makes this module send the message along with some
transaction information to the specified Application Server. The Application
Server then executes some call-control logic code, and tells OpenSER to take
some actions, ie. forward the message downstream, or respond to the message
with a SIP repy, etc

%package	avp_radius
Summary:	Allows loading of user's attributes into AVPs from Radius
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	avp_radius
This module allows loading of user's attributes into AVPs from Radius.
User's name and domain can be based on From URI, Request URI, or
authenticated credentials.

%package	uri_radius
Summary:	URI check using Radius server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}

%description	uri_radius
URI check using Radius server

%package	cpl-c
Summary:	Call Processing Language interpreter
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	database // TODO
#Requires:	%{name}-tm
#Requires:	%{name}-sl
#Requires:	%{name}-usrloc

%description	cpl-c
This module implements a CPL (Call Processing Language) interpreter.
Support for uploading/downloading/removing scripts via SIP REGISTER method
is present.

%package	pua_usrloc
Summary:	Connector between usrloc and pua modules
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua
#Requires:	%{name}-usrloc

%description	pua_usrloc
This module is the connector between usrloc and pua modules. It creates the
environment to send PUBLISH requests for user location records, on specific
events (e.g., when new record is added in usrloc, a PUBLISH with status open
(online) is issued; when expires, it sends closed (offline)). Using this
module, phones which have no support for presence can be seen as
online/offline.

%package	pua_mi
Summary:	Connector between usrloc and MI interface
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
Requires:	%{name}-pua
#Requires:	%{name}-mi_fifo

%description	pua_mi
The pua_mi sends offer the possibility to publish presence information
via MI transports.  Using this module you can create independent
applications/scripts to publish not sip-related information (e.g., system
resources like CPU-usage, memory, number of active subscribers ...)

%package	pua
Summary:	Offer the functionality of a presence user agent client
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	database // TODO
#Requires:	%{name}-tm

%description	pua
This module offer the functionality of a presence user agent client, sending
Subscribe and Publish messages.

%package	presence
Summary:	Presence server
Group:		System Environment/Daemons
Requires:	%{name} = %{version}-%{release}
#Requires:	database // TODO
#Requires:	%{name}-sl
#Requires:	%{name}-tm

%description	presence
This module implements a presence server. It handles PUBLISH and SUBSCRIBE
messages and generates NOTIFY messages. It offers support for aggregation
of published presence information for the same presentity using more devices.
It can also filter the information provided to watchers according to privacy
rules.

%prep
%setup -q -n %{name}-%{version}-tls
cp -pRf modules/acc modules/acc_radius
%patch1
#%patch2
%patch3

# fix wrong permissions to avoid rpmlint complaining
chmod 644 ./tls/*.[ch] ./tls/TODO.TLS

%build
LOCALBASE=/usr CFLAGS="%{optflags}" %{__make} all TLS=1 \
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

# clean some things
mv $RPM_BUILD_ROOT/%{_sysconfdir}/openser/tls/README \
  $RPM_BUILD_ROOT/%{_docdir}/openser/README.tls
rm -f $RPM_BUILD_ROOT%{_docdir}/openser/INSTALL

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d/
install -D -m 755 %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d/openser

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
%{_sbindir}/openserctl
%{_sbindir}/openserunix
%{_sbindir}/openser_mysql.sh
%{_sbindir}/openser_postgresql.sh

%dir %{_sysconfdir}/openser
%dir %{_sysconfdir}/openser/tls
%dir %{_libdir}/openser/
%dir %{_libdir}/openser/modules/
%dir %{_libdir}/openser/openserctl/

%{_sysconfdir}/rc.d/init.d/openser

%config(noreplace) %{_sysconfdir}/openser/dictionary.radius
%config(noreplace) %{_sysconfdir}/openser/openser.cfg
%config(noreplace) %{_sysconfdir}/openser/openserctlrc

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

%{_libdir}/openser/openserctl/openserctl.*
%{_mandir}/man5/openser.cfg.5*
%{_mandir}/man8/openser.8*
%{_mandir}/man8/openserctl.8*
%{_mandir}/man8/openserunix.8*
%{_docdir}/openser/AUTHORS
%{_docdir}/openser/NEWS
%{_docdir}/openser/README
%{_docdir}/openser/README-MODULES
%{_docdir}/openser/README.tls

%{_libdir}/openser/modules/alias_db.so
%{_libdir}/openser/modules/auth.so
%{_libdir}/openser/modules/auth_db.so
%{_libdir}/openser/modules/avpops.so
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

%{_docdir}/openser/README.alias_db
%{_docdir}/openser/README.auth
%{_docdir}/openser/README.auth_db
%{_docdir}/openser/README.avpops
%{_docdir}/openser/README.dbtext
%{_docdir}/openser/README.dialog
%{_docdir}/openser/README.dispatcher
%{_docdir}/openser/README.diversion
%{_docdir}/openser/README.domain
%{_docdir}/openser/README.domainpolicy
%{_docdir}/openser/README.enum
%{_docdir}/openser/README.exec
%{_docdir}/openser/README.flatstore
%{_docdir}/openser/README.gflags
%{_docdir}/openser/README.group
%{_docdir}/openser/README.imc
%{_docdir}/openser/README.lcr
%{_docdir}/openser/README.mangler
%{_docdir}/openser/README.maxfwd
%{_docdir}/openser/README.mediaproxy
%{_docdir}/openser/README.mi_fifo
%{_docdir}/openser/README.msilo
%{_docdir}/openser/README.nathelper
%{_docdir}/openser/README.options
%{_docdir}/openser/README.path
%{_docdir}/openser/README.pdt
%{_docdir}/openser/README.permissions
%{_docdir}/openser/README.pike
%{_docdir}/openser/README.registrar
%{_docdir}/openser/README.rr
%{_docdir}/openser/README.siptrace
%{_docdir}/openser/README.sl
%{_docdir}/openser/README.speeddial
%{_docdir}/openser/README.sst
%{_docdir}/openser/README.statistics
%{_docdir}/openser/README.textops
%{_docdir}/openser/README.tm
%{_docdir}/openser/README.uac
%{_docdir}/openser/README.uac_redirect
%{_docdir}/openser/README.uri
%{_docdir}/openser/README.uri_db
%{_docdir}/openser/README.usrloc
%{_docdir}/openser/README.xlog

%files acc
%defattr(-,root,root,-)
%{_libdir}/openser/modules/acc.so
%{_docdir}/openser/README.acc

%files acc_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/acc_radius.so
%{_docdir}/openser/README.acc_radius

%files mysql
%defattr(-,root,root,-)
%{_libdir}/openser/modules/mysql.so
%{_docdir}/openser/README.mysql

%files postgresql
%defattr(-,root,root,-)
%{_libdir}/openser/modules/postgres.so
%{_docdir}/openser/README.postgres

%files unixodbc
%defattr(-,root,root,-)
%{_libdir}/openser/modules/unixodbc.so
%{_docdir}/openser/README.unixodbc

%files snmpstats
%defattr(-,root,root,-)
%{_libdir}/openser/modules/snmpstats.so
%{_docdir}/openser/README.snmpstats
%{_datadir}/snmp/mibs/OPENSER-MIB
%{_datadir}/snmp/mibs/OPENSER-REG-MIB
%{_datadir}/snmp/mibs/OPENSER-SIP-COMMON-MIB
%{_datadir}/snmp/mibs/OPENSER-SIP-SERVER-MIB
%{_datadir}/snmp/mibs/OPENSER-TC

%files tlsops
%defattr(-,root,root,-)
%{_libdir}/openser/modules/tlsops.so
%{_docdir}/openser/README.tlsops

%files perl
%defattr(-,root,root,-)
%dir %{_libdir}/openser/perl/
%dir %{_libdir}/openser/perl/OpenSER/
%{_libdir}/openser/modules/perl.so
%{_libdir}/openser/perl/OpenSER.pm
%{_libdir}/openser/perl/OpenSER/Constants.pm
%{_libdir}/openser/perl/OpenSER/LDAPUtils/LDAPConf.pm
%{_libdir}/openser/perl/OpenSER/LDAPUtils/LDAPConnection.pm
%{_libdir}/openser/perl/OpenSER/Message.pm
%{_libdir}/openser/perl/OpenSER/Utils/PhoneNumbers.pm
%{_docdir}/openser/README.perl

%files xmpp
%defattr(-,root,root,-)
%{_libdir}/openser/modules/xmpp.so
%{_docdir}/openser/README.xmpp

%files jabber
%defattr(-,root,root,-)
%{_libdir}/openser/modules/jabber.so
%{_docdir}/openser/README.jabber

%files sms
%defattr(-,root,root,-)
%{_libdir}/openser/modules/sms.so
%{_docdir}/openser/README.sms

%files auth_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/auth_radius.so
%{_docdir}/openser/README.auth_radius

%files auth_diameter
%defattr(-,root,root,-)
%{_libdir}/openser/modules/auth_diameter.so
%{_docdir}/openser/README.auth_diameter

%files group_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/group_radius.so
%{_docdir}/openser/README.group_radius

%files seas
%defattr(-,root,root,-)
%{_libdir}/openser/modules/seas.so
%{_docdir}/openser/README.seas

%files avp_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/avp_radius.so
%{_docdir}/openser/README.avp_radius

%files uri_radius
%defattr(-,root,root,-)
%{_libdir}/openser/modules/uri_radius.so
%{_docdir}/openser/README.uri_radius

%files	cpl-c
%defattr(-,root,root,-)
%{_libdir}/openser/modules/cpl-c.so
%{_docdir}/openser/README.cpl-c

%files	pua_usrloc
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua_usrloc.so
%{_docdir}/openser/README.pua_usrloc

%files pua_mi
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua_mi.so
%{_docdir}/openser/README.pua_mi

%files pua
%defattr(-,root,root,-)
%{_libdir}/openser/modules/pua.so
%{_docdir}/openser/README.pua

%files	presence
%defattr(-,root,root,-)
%{_libdir}/openser/modules/presence.so
%{_docdir}/openser/README.presence

%changelog

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

