# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
#

# define path to netfilter/iptables directory
iptdir = ~/work/netfilter/iptables
include ../../Makefile.defs
auto_gen=
NAME=iptrtpproxy.so
LIBS += $(iptdir)/iptables.o -L $(iptdir)/libiptc -liptc -L $(iptdir)/extensions -lext
DEFS += -I $(iptdir)/include

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules

