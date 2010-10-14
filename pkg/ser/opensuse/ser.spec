%define EXCLUDED_MODULES	mysql jabber auth_radius group_radius uri_radius postgres snmp cpl cpl-c extcmd 
%define MYSQL_MODULES		mysql
%define JABBER_MODULES		jabber
%define RADIUS_MODULES		auth_radius group_radius uri_radius
%define RADIUS_MOD_PATH		modules/auth_radius modules/group_radius modules/uri_radius


Summary:      SIP Express Router, very fast and flexible SIP Proxy
Name:         ser
Version:      0.9.6
Release:      22.1
Packager:     Peter Nixon <peter+rpmspam@suntel.com.tr>
License:      GPL
Group:        Productivity/Telephony/SIP/Servers
Source:       http://iptel.org/ser/stable/%{name}-%{version}_src.tar.bz2
Source2:      ser.init.SuSE
%ifarch x86_64
Patch:        Makefile.defs.patch
%endif
URL:          http://www.iptel.org/ser
Vendor:       FhG Fokus
BuildRoot:    /var/tmp/%{name}-%{version}-root
Conflicts:    ser < %{version}, ser-mysql < %{version}, ser-jabber < %{version}, ser-radius < %{version}
BuildPrereq:  make flex bison 
BuildRequires: mysql-devel, radiusclient, expat

%description
SIP Express Router (SER) is a very fast and flexible SIP (RFC3621)
proxy server. Written entirely in C, SER can handle thousands calls
per second even on low-budget hardware. A C Shell like scripting language
provides full control over the server's behaviour. It's modular
architecture allows only required functionality to be loaded.
Currently the following modules are available: digest authentication,
CPL scripts, instant messaging, MySQL support, a presence agent, radius
authentication, record routing, an SMS Gateway, a jabber gateway, a 
transaction module, registrar and user location.

%package  mysql
Summary:  MySQL connectivity for the SIP Express Router.
Group:    Productivity/Telephony/SIP/Servers
Requires: ser = %{version}
BuildPrereq: mysql-devel zlib-devel

%description mysql
The ser-mysql package contains MySQL database connectivity that you
need to use digest authentication module or persistent user location
entries.

%package  jabber
Summary:  sip jabber message translation support for the SIP Express Router.
Group:    Productivity/Telephony/SIP/Servers
Requires: ser = %{version}
BuildPrereq: expat

%description jabber
The ser-jabber package contains a sip to jabber message translator.

%package  radius
Summary:  ser radius authentication, group and uri check modules.
Group:    Productivity/Telephony/SIP/Servers
Requires: ser = %{version}
BuildPrereq:  radiusclient

%description radius
The ser-radius package contains modules for radius authentication, group
 membership and uri checking.

%prep
%setup
%ifarch x86_64
%patch
%endif

%build
make all skip_modules="%EXCLUDED_MODULES" \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-target=/%{_sysconfdir}/ser/
make modules modules="modules/%MYSQL_MODULES" \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-target=/%{_sysconfdir}/ser/
make modules modules="modules/%JABBER_MODULES" \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-target=/%{_sysconfdir}/ser/
make modules modules="%RADIUS_MOD_PATH" \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-target=/%{_sysconfdir}/ser/

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make install skip_modules="%EXCLUDED_MODULES" \
		basedir=$RPM_BUILD_ROOT \
		prefix=/usr \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-prefix=$RPM_BUILD_ROOT \
		cfg-target=/%{_sysconfdir}/ser/ \
		doc-prefix=$RPM_BUILD_ROOT \
		doc-dir=/%{_docdir}/ser/
make install-modules modules="modules/%MYSQL_MODULES" \
		basedir=$RPM_BUILD_ROOT \
		prefix=/usr \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-prefix=$RPM_BUILD_ROOT \
		cfg-target=/%{_sysconfdir}/ser/ \
		doc-prefix=$RPM_BUILD_ROOT \
		doc-dir=/%{_docdir}/ser/
make install-modules modules="modules/%JABBER_MODULES" \
		basedir=$RPM_BUILD_ROOT \
		prefix=/usr \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-prefix=$RPM_BUILD_ROOT \
		cfg-target=/%{_sysconfdir}/ser/ \
		doc-prefix=$RPM_BUILD_ROOT \
		doc-dir=/%{_docdir}/ser/
make install-modules modules="%RADIUS_MOD_PATH" \
		basedir=$RPM_BUILD_ROOT \
		prefix=/usr \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		cfg-prefix=$RPM_BUILD_ROOT \
		cfg-target=/%{_sysconfdir}/ser/ \
		doc-prefix=$RPM_BUILD_ROOT \
		doc-dir=/%{_docdir}/ser/
make install-doc modules="modules/%JABBER_MODULES %RADIUS_MOD_PATH" \
		basedir=$RPM_BUILD_ROOT \
		prefix=/usr \
%ifarch x86_64
		modules-dir=/lib64/ser/modules/ \
%endif
		doc-prefix=$RPM_BUILD_ROOT \
		doc-dir=/%{_docdir}/ser/


mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/init.d
install -m755 $RPM_SOURCE_DIR/ser.init.SuSE \
              $RPM_BUILD_ROOT/%{_sysconfdir}/init.d/ser
ln -sf ../../etc/init.d/ser $RPM_BUILD_ROOT/usr/sbin/rcser

%clean
rm -rf "$RPM_BUILD_ROOT"

%post
sbin/insserv etc/init.d/ser

%preun
if [ $1 = 0 ]; then
    etc/init.d/ser stop > /dev/null 2>&1
fi

%postun
sbin/insserv etc/init.d/

%files
%defattr(-,root,root)
%dir %{_docdir}/ser
%doc %{_docdir}/ser/*

%dir %{_sysconfdir}/ser
%config(noreplace) %{_sysconfdir}/ser/*
%config %{_sysconfdir}/init.d/*

#%ifarch x86_64
#%else
%dir %{_libdir}/ser
%dir %{_libdir}/ser/modules
%{_libdir}/ser/modules/acc.so
%{_libdir}/ser/modules/auth.so
%{_libdir}/ser/modules/auth_db.so
%{_libdir}/ser/modules/auth_diameter.so
%{_libdir}/ser/modules/dbtext.so
%{_libdir}/ser/modules/domain.so
%{_libdir}/ser/modules/enum.so
%{_libdir}/ser/modules/exec.so
%{_libdir}/ser/modules/group.so
%{_libdir}/ser/modules/mangler.so
%{_libdir}/ser/modules/maxfwd.so
%{_libdir}/ser/modules/mediaproxy.so
%{_libdir}/ser/modules/msilo.so
%{_libdir}/ser/modules/nathelper.so
%{_libdir}/ser/modules/pdt.so
%{_libdir}/ser/modules/permissions.so
%{_libdir}/ser/modules/pike.so
%{_libdir}/ser/modules/print.so
%{_libdir}/ser/modules/registrar.so
%{_libdir}/ser/modules/rr.so
%{_libdir}/ser/modules/sl.so
%{_libdir}/ser/modules/sms.so
%{_libdir}/ser/modules/textops.so
%{_libdir}/ser/modules/tm.so
%{_libdir}/ser/modules/uri.so
%{_libdir}/ser/modules/usrloc.so
%{_libdir}/ser/modules/xlog.so

%{_libdir}/ser/modules/avp.so
%{_libdir}/ser/modules/avp_db.so
%{_libdir}/ser/modules/avpops.so
%{_libdir}/ser/modules/dispatcher.so
%{_libdir}/ser/modules/diversion.so
%{_libdir}/ser/modules/flatstore.so
%{_libdir}/ser/modules/gflags.so
%{_libdir}/ser/modules/options.so
%{_libdir}/ser/modules/speeddial.so
%{_libdir}/ser/modules/uri_db.so
#%endif
%{_sbindir}/ser
%{_sbindir}/serctl
%{_sbindir}/serunix
/usr/sbin/rcser

%{_mandir}/man5/*
%{_mandir}/man8/*


%files mysql
%defattr(-,root,root)

#%ifarch x86_64
#%else
%{_libdir}/ser/modules/mysql.so
#%endif
%{_sbindir}/ser_mysql.sh

%files jabber
%defattr(-,root,root)
#%ifarch x86_64
#%else
%{_libdir}/ser/modules/jabber.so
#%endif
%doc %{_docdir}/ser/README.jabber

%files radius
%defattr(-,root,root)
#%{_libdir}/ser/modules/auth_radius.so
#%{_libdir}/ser/modules/group_radius.so
#%{_libdir}/ser/modules/uri_radius.so
%doc %{_docdir}/ser/README.auth_radius
%doc %{_docdir}/ser/README.group_radius
%doc %{_docdir}/ser/README.uri_radius




%changelog

* Tue Jul 26 2005 Peter Nixon - Suntel Communications <peter+rpmspam@suntel.com.tr>
- include SUSE version in the rpm filename(s)
- changed version to 0.9.3 (new upstream release)
- update rpm package group to match SUSE 9.3
- removed files no longer in tarball
- enabled radius modules
- add symlink for rcser

* Tue Jul 27 2004 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.fraunhofer.de>
- changed vesion to 0.8.14 (new upstream release)
- added ext to the modules list, removed the radius modules (they depend on radiusclient-ng now)

* Fri Nov 14 2003 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.fraunhofer.de>
- changed vesion to 0.8.12 (new upstream release)
- added auth_diameter, pdt & mangler to the modules list

* Wed Aug 28 2003 Nils Ohlmeier <nils@iptel.org>
- replaced modules Conflicts with required Ser version
- fixed doc installation for SuSE pathes
- added doc for jabber and radius

* Wed Aug 28 2003 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.fraunhofer.de>
- added doc (READMEs, NEWS, AUTHORS a.s.o)
- added xlog to the modules list

* Wed Aug 27 2003 Nils Ohlmeier <nils@iptel.org>
- fixed module dependencys
- added Conflicts for modules

* Wed Aug 27 2003 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.fraunhofer.de>
- changed vesion to 0.8.11
- gen_ha1 is now left in _sbindir
- removed harv_ser.sh
- added Conflicts

* Mon Jun 2 2003 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.fraunhofer.de>
- added a separate rpm for the radius modules
- updated to the new makefile variables (removed lots of unnecessary stuff)

* Tue Nov 12 2002 Nils Ohlmeier <ohlmeier@fokus.fhg.de>
- replaced expat-devel with expat
- removed leading + from a few lines

* Tue Nov 12 2002 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.gmd.de>
- added a separate rpm for the jabber modules
- moved all the binaries to sbin
- removed obsolete installs (make install installs everything now)

* Mon Oct 28 2002 Nils Ohlmeier <ohlmeier@fokus.fhg.de>
- Added mysql and mysql-devel to the Req for the ser-mysql rpm.

* Thu Sep 26 2002 Nils Ohlmeier <ohlmeier@fokus.fhg.de>
- Added library path to mysql/Makefile to build on SuSE 8.0

* Thu Sep 26 2002 Nils Ohlmeier <ohlmeier@fokus.fhg.de>
- Added 'make [modules|modules-install]' to adapted Makefile changes

* Wed Sep 25 2002 Andrei Pelinescu - Onciul  <pelinescu-onciul@fokus.gmd.de>
- modified make install & make: added cfg-target & modules-target

* Wed Sep 25 2002 Nils Ohlmeier <ohlmeier@fokus.fhg.de>
- Copyed mysql connectivity subpackage from orig rpm.spec.

* Fri Sep 06 2002 Nils Ohlmeier <ohlmeier@fokus.fhg.de>
- Adaptation to SuSE.

* Mon Sep 02 2002 Jan Janak <J.Janak@sh.cvut.cz>
- gen_ha1 utility added, scripts added.

* Tue Aug 28 2002 Jan Janak <J.Janak@sh.cvut.cz>
- Finished the first version of the spec file.

* Sun Aug 12 2002 Jan Janak <J.Janak@sh.cvut.cz>
- First version of the spec file.
