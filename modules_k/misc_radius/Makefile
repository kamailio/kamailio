# $Id: Makefile 1515 2007-01-16 21:25:55Z bogdan_iancu $
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=misc_radius.so

# Comment the next two line and uncomment the following two if you want
# to enable Radius support
DEFS+= -DWITH_RADIUS_SUPPORT -I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib $(RADIUS_LIB)

include ../../Makefile.modules
