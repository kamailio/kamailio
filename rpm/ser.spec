%define name  ser
%define ver   0.8.7
%define rel   1

Summary:      SIP Express Router, very fast and flexible SIP Proxy
Name:         %name
Version:      %ver
Release:      %rel
Packager:     Jan Janak <J.Janak@sh.cvut.cz>
Copyright:    GPL
Group:        System Environment/Daemons
Source:       http://iptel.org/ser/stable/%{name}-%{ver}.tar.gz
Source2:      ser.init
URL:          http://ser.iptel.org
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


%prep
%setup

%build
make all
cd utils/gen_ha1
make


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
	     man-dir=""

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
install -m755 $RPM_SOURCE_DIR/ser.init \
              $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/ser

mkdir -p $RPM_BUILD_ROOT/%{_bindir}

install -m755 utils/gen_ha1/gen_ha1 \
	      $RPM_BUILD_ROOT/%{_bindir}/gen_ha1

install -m755 scripts/harv_ser.sh \
	      $RPM_BUILD_ROOT/%{_sbindir}/harv_ser.sh

install -m755 scripts/sc \
	      $RPM_BUILD_ROOT/%{_sbindir}/serctl

install -m755 scripts/ser_mysql.sh \
	      $RPM_BUILD_ROOT/%{_sbindir}/ser_mysql.sh


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
%{_libdir}/ser/modules/*

%{_sbindir}/*
%{_bindir}/*

%{_mandir}/man5/*
%{_mandir}/man8/*


%changelog
* Mon Sep 02 2002 Jan Janak <J.Janak@sh.cvut.cz>
- gen_ha1 utility added, scripts added.

* Tue Aug 28 2002 Jan Janak <J.Janak@sh.cvut.cz>
- Finished the first version of the spec file.

* Sun Aug 12 2002 Jan Janak <J.Janak@sh.cvut.cz>
- First version of the spec file.
