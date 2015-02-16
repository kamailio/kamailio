#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ctl.so
LIBS=
MOD_INSTALL_UTILS=../../utils/kamcmd
# fifo support
DEFS+=-DUSE_FIFO
# use system malloc
DEFS+=-DCTL_SYSTEM_MALLOC

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules

