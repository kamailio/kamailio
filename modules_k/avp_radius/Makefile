# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=avp_radius.so

# Comment the next two line and uncomment the following two if you want
# to enable Radius support
DEFS+= -DWITH_RADIUS_SUPPORT -I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib -lradiusclient-ng

include ../../Makefile.modules

