# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

BUILDER = $(shell which net-snmp-config)
ifeq ($(BUILDER),)
	NAME=
else
	BUILDAGENTLIBS = $(shell net-snmp-config --agent-libs)
endif 

CFLAGS+=
auto_gen=
NAME=snmpstats.so
LIBS=$(BUILDAGENTLIBS)


include ../../Makefile.modules
