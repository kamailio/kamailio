%define name    ser
%define ver     0.8.10
%define rel     2
%define exclude CVS radius_acc radius_auth snmp mysql jabber

Summary:      SIP Express Router, very fast and flexible SIP Proxy
Name:         %name
Version:      %ver
Release:      %rel
Packager:     Jan Janak <J.Janak@sh.cvut.cz>
Copyright:    GPL
Group:        System Environment/Daemons
Source:       http://iptel.org/ser/stable/%{name}-%{ver}_src.tar.gz
Source2:      ser.init
URL:          http://iptel.org/ser
Vendor:       FhG Fokus
BuildRoot:    /var/tmp/%{name}-%{ver}-root
BuildPrereq:  make flex bison 


%description
Ser or SIP Express Router is a very fast and flexible SIP (RFC3621)
proxy server. Written entirely in C, ser can handle thousands calls
per second even on low-budget hardware. C Shell like scripting language
provides full control over the server's behaviour. It's modular
architecture allows only required functionality to be loaded.
Currently the following modules are available: Digest Authentication,
CPL scripts, Instant Messaging, MySQL support, Presence Agent, Radius
Authentication, Record Routing, SMS Gateway, Jabber Gateway, Transaction 
Module, Registrar and User Location.

%package  mysql
Summary:  MySQL connectivity for the SIP Express Router.
Group:    System Environment/Daemons
Requires: ser
BuildPrereq:  mysql-devel

%description mysql
The ser-mysql package contains MySQL database connectivity that you
need to use digest authentication module or persistent user location
entries.

%package  jabber
Summary:  sip jabber message translation support for the SIP Express Router.
Group:    System Environment/Daemons
Requires: ser
BuildPrereq:  expat-devel

%description jabber
The ser-jabber package contains a sip to jabber message translator.

%prep
%setup

%build
make all exclude_modules="%exclude" cfg-target=/%{_sysconfdir}/ser/
make modules modules=modules/mysql  cfg-target=/%{_sysconfdir}/ser/
make modules modules=modules/jabber cfg-target=/%{_sysconfdir}/ser/


%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make install cfg-prefix=$RPM_BUILD_ROOT/%{_sysconfdir} \
             cfg-dir=ser/ \
	     bin-prefix=$RPM_BUILD_ROOT/%{_sbindir} \
	     bin-dir="" \
	     modules-prefix=$RPM_BUILD_ROOT/%{_libdir}/ser \
	     modules-dir=modules/ \
	     doc-prefix=$RPM_BUILD_ROOT/%{_docdir} \
	     doc-dir=ser/ \
	     man-prefix=$RPM_BUILD_ROOT/%{_mandir} \
	     man-dir="" \
	     cfg-target=/%{_sysconfdir}/ser/ \
	     modules-target=/%{_libdir}/ser/modules/ \
	     exclude_modules="%exclude"
make install-modules modules=modules/mysql \
	    modules-prefix=$RPM_BUILD_ROOT/%{_libdir}/ser \
	    modules-dir=modules/ \
	    doc-prefix=$RPM_BUILD_ROOT/%{_docdir} \
	    doc-dir=ser/ \
	    cfg-target=/%{_sysconfdir}/ser/ \
	    modules-target=/%{_libdir}/ser/modules/ 
make install-modules modules=modules/jabber \
	    modules-prefix=$RPM_BUILD_ROOT/%{_libdir}/ser \
	    modules-dir=modules/ \
	    doc-prefix=$RPM_BUILD_ROOT/%{_docdir} \
	    doc-dir=ser/ \
	    cfg-target=/%{_sysconfdir}/ser/ \
	    modules-target=/%{_libdir}/ser/modules/ 

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
install -m755 $RPM_SOURCE_DIR/ser.init \
              $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/ser


mkdir -p $RPM_BUILD_ROOT/%{_bindir}
install -m755 scripts/harv_ser.sh \
              $RPM_BUILD_ROOT/%{_bindir}/harv_ser.sh

mv $RPM_BUILD_ROOT/%{_sbindir}/gen_ha1 $RPM_BUILD_ROOT/%{_bindir}


%clean
rm -rf "$RPM_BUILD_ROOT"

%post
/sbin/chkconfig --add ser

%preun
if [ $1 = 0 ]; then
    /sbin/service ser stop > /dev/null 2>&1
    /sbin/chkconfig --del ser
fi


%files
%defattr(-,root,root)
%doc README

%dir %{_sysconfdir}/ser
%config(noreplace) %{_sysconfdir}/ser/*
%config %{_sysconfdir}/rc.d/init.d/*

%dir %{_libdir}/ser
%dir %{_libdir}/ser/modules
%{_libdir}/ser/modules/acc.so
%{_libdir}/ser/modules/auth.so
%{_libdir}/ser/modules/exec.so
%{_libdir}/ser/modules/im.so
%{_libdir}/ser/modules/maxfwd.so
%{_libdir}/ser/modules/msilo.so
%{_libdir}/ser/modules/pike.so
%{_libdir}/ser/modules/print.so
%{_libdir}/ser/modules/registrar.so
%{_libdir}/ser/modules/rr.so
%{_libdir}/ser/modules/sl.so
%{_libdir}/ser/modules/sms.so
%{_libdir}/ser/modules/textops.so
%{_libdir}/ser/modules/tm.so
%{_libdir}/ser/modules/usrloc.so

%{_sbindir}/ser
%{_sbindir}/serctl

%{_bindir}/harv_ser.sh
%{_bindir}/gen_ha1

%{_mandir}/man5/*
%{_mandir}/man8/*


%files mysql
%defattr(-,root,root)

%{_libdir}/ser/modules/mysql.so
%{_sbindir}/ser_mysql.sh

%files jabber
%defattr(-,root,root)

%{_libdir}/ser/modules/jabber.so

%changelog
* Thu Nov 14 2002 Jan Janak <J.Janak@sh.cvut.cz>
- Installing harv_ser.sh again
- quick hack to move gen_ha1 to bin directory instead of sbin (should
  be done from the Makefile next time)

* Tue Nov 12 2002 Andrei Pelinescu - Onciul <pelinescu-onciul@fokus.gmd.de>
- added a separate rpm for the jabber modules
- moved all the binaries to sbin
- removed obsolete installs (make install installs everything now)

* Fri Oct  4 2002 Jiri Kuthan <jiri@iptel.org>
- exec module introduced

* Wed Sep 25 2002 Andrei Pelinescu - Onciul  <pelinescu-onciul@fokus.gmd.de>
- modified make install & make: added cfg-target & modules-target

* Sun Sep 08 2002 Jan Janak <J.Janak@sh.cvut.cz>
- Created subpackage containing mysql connectivity support.

* Mon Sep 02 2002 Jan Janak <J.Janak@sh.cvut.cz>
- gen_ha1 utility added, scripts added.

* Tue Aug 28 2002 Jan Janak <J.Janak@sh.cvut.cz>
- Finished the first version of the spec file.

* Sun Aug 12 2002 Jan Janak <J.Janak@sh.cvut.cz>
- First version of the spec file.
