%define name  ser
%define ver   0.8.7
%define rel   1

Summary:      SIP Express Router, very fast and flexible SIP Proxy
Name:         %name
Version:      %ver
Release:      %rel
Packager:     Jan Janak <J.Janak@sh.cvut.cz>
Copyright:    GPL
Group:        Applications/Internet
Source:       ftp://ser.iptel.org/stable/%{name}-%{ver}.tar.gz
URL:          http://ser.iptel.org
Vendor:       Fhg Fokus
BuildRoot:    /var/tmp/%{name}-%{ver}-root
BuildPrereq:  make flex bison rpm-devel >= 4 


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

%changelog
* Sun Aug 12 2002 Jan Janak <J.Janak@sh.cvut.cz>
- First version of the spec file.

%prep
%setup

%build
make all

%configure

%install


%clean
rm -rf $RPM_BUILD_ROOT

%files
