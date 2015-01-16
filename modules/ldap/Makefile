#
# LDAP_H350
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

#DEFS += -DLDAP_PERF

auto_gen=
NAME=ldap.so

LIBS=-L$(LOCALBASE)/lib -lldap
DEFS+=-I$(LOCALBASE)/include

ifeq ($(OS),solaris)
       DEFS+=-I$(LOCALBASE)/include -I/usr/sfw/include -I/usr/local/include -I/opt/sfw/include -I/opt/csw/include
       LIBS=-L$(LOCALBASE)/lib -L/usr/sfw/lib -L/usr/local/lib -L/opt/sfw/lib -I/opt/csw/lib -lldap 
endif


DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
