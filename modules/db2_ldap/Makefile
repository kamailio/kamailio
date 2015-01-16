#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db2_ldap.so

#DEFS += -DLD_TEST

DEFS += -Wall -DOPENLDAP23 -DLDAP_DEPRECATED

# Uncomment this if you have a recent version of libldap with
# LD_SCOPE_CHILDREN defined
#DEFS += -DHAVE_SCOPE_CHILDREN

LIBS=-L$(LOCALBASE)/lib -L /usr/lib -lldap

DEFS+=-DSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb2/srdb2
include ../../Makefile.modules
