#
# iprtpproxy module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
#

# select if xtables.h (=0) or iptables.h (=1)  header files are installed 
IPT_RTPPROXY_IPTABLES_API=1

# define path to netfilter/iptables source directory
iptdir = ~/iptables
#kerneldir=/usr/src/kernels/2.6.18-1.2849prerel3.3.0_63.1.0-i686/include 
#kerneldir=~/iptables-1.4.2/include
xtablesmoduledir=/lib/xtables

include ../../Makefile.defs
auto_gen=
NAME=iptrtpproxy.so
#LIBS += $(iptdir)/iptables.o -L $(iptdir)/libiptc -liptc \
#		-L $(iptdir)/extensions -lext
# look for libipt_RTPPROXY first under /lib/xtables (default install dir)
# and if not found in $(iptdir)/extensions (compile dir)
LIBS = -L $(iptdir)/libiptc -L $(iptdir)/iptables/extensions -lipt_RTPPROXY
LIBS+= -L $(xtablesmoduledir)
INCLUDES = -I $(iptdir)/linux-2.6/include -I $(kerneldir) -I $(iptdir)/iptables/include

ifeq ($(IPT_RTPPROXY_IPTABLES_API),1)
DEFS+= -DIPT_RTPPROXY_IPTABLES_API=1
endif

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules

