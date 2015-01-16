#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=snmpstats.so

ifeq ($(CROSS_COMPILE),)
BUILDER = $(shell which net-snmp-config)
endif

ifeq ($(BUILDER),)
	DEFS +=-I$(LOCALBASE)/include
	BUILDAGENTLIBS =-L$(LOCALBASE)/lib -lnetsnmpmibs -lnetsnmpagent \
		-lnetsnmphelpers -lnetsnmp
	INSTALLMIBDIR =  $(share_prefix)/share/snmp/mibs
else
	BUILDAGENTLIBS = $(shell net-snmp-config --netsnmp-agent-libs)
	INSTALLMIBDIR = $(basedir)$(shell net-snmp-config --prefix)/share/snmp/mibs
endif 

ifeq ($(EMBEDDED_PERL),1)
#
# Enable this part if you compile on Centos 5 or another system that has a
# packaged snmpd with embedded perl and libwrap support
#
BUILDAGENTLIBS += -Wl,-E -Wl,-rpath,/usr/lib/perl5/5.8.8/i386-linux-thread-multi/CORE \
                -L/usr/local/lib /usr/lib/perl5/5.8.8/i386-linux-thread-multi/auto/DynaLoader/DynaLoader.a \
                -L/usr/lib/perl5/5.8.8/i386-linux-thread-multi/CORE \
                -lperl -lresolv -lnsl -ldl -lm -lcrypt -lutil -lpthread -lc \
                -lwrap \
                -lsensors \
                -lrpmdb -lrpm
endif

LIBS=$(BUILDAGENTLIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules

install-cfg:
	echo "installing mibs ..."
	mkdir -p $(INSTALLMIBDIR)
	$(INSTALL_CFG) mibs/KAMAILIO* $(INSTALLMIBDIR)

