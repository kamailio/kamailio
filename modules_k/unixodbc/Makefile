# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=unixodbc.so

# sql.h locations (freebsd,openbsd  solaris)
DEFS += -I/usr/local/include

# libodbc locations on RH/Suse, Solaris /OpenBSD, FreeBSD
# (Debian does the right thing and puts it in /usr/lib)
LIBS= -lodbc -lz

include ../../Makefile.modules
