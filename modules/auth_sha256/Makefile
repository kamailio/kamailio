#
# auth example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=auth.so
LIBS=
# DEFS:
#       -DUSE_NC       - compile with nounce count support
#       -DUSE_OT_NONCE - compile with one-time nonces support
DEFS+=-DUSE_NC -DUSE_OT_NONCE

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules
