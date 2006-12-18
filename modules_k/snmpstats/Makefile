# $Id$
#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

BUILDAGENTLIBS=`net-snmp-config --agent-libs`


CFLAGS+=$(BUILDAGENTLIBS)
auto_gen=
NAME=snmpstats.so
LIBS=$(BUILDAGENTLIBS)


include ../../Makefile.modules
