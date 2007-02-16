
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen    =
NAME        = oracle.so

ORACLE_ROOT = /usr/lib/oracle/xe/app/oracle/product/10.2.0/client
ORACLE_LIBS = $(ORACLE_ROOT)/lib
ORACLE_INCLUDES = $(ORACLE_ROOT)/rdbms/public

# ORACLE_ROOT = /usr/local/instantclient
# ORACLE_LIBS = $(ORACLE_ROOT)
# ORACLE_INCLUDES = $(ORACLE_ROOT)/sdk/include

DEFS += -I$(ORACLE_INCLUDES)
LIBS += -L$(ORACLE_LIBS) -lclntsh

include ../../Makefile.modules
