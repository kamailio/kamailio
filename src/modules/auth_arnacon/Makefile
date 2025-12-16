#
# Arnacon Authentication Module Makefile
#

include ../../Makefile.defs
auto_gen=
NAME=auth_arnacon.so
LIBS=

DEFS+=-DKAMAILIO_MOD -DDISABLE_NAGLE

# Link with curl and secp256k1
LIBS+=-lcurl -lsecp256k1

include ../../Makefile.modules
