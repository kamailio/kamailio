# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
#

# define path to netfilter/iptables source directory
iptdir = ~/iptables
include ../../Makefile.defs
auto_gen=
NAME=iptrtpproxy.so
#LIBS += $(iptdir)/iptables.o -L $(iptdir)/libiptc -liptc \
#		-L $(iptdir)/extensions -lext
# look for libipt_RTPPROXY first under /lib/xtables (default install dir)
# and if not found in $(iptdir)/extensions (compile dir)
LIBS = -L/lib/xtables -L$(iptdir)/extensions -lipt_RTPPROXY -ldl
INCLUDES = -I$(iptdir)/include

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules

