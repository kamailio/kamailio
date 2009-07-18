# Makefile v 1.0 2002/12/27
#
# TLS module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls.so

DEFS+= -I$(LOCALBASE)/ssl/include
LIBS+= -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib -lssl  -lcrypto \
	$(TLS_EXTRA_LIBS)
# NOTE: depending on the way in which libssl was compiled you might
#       have to add -lz -lkrb5   (zlib and kerberos5).
#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"

$(cfg_prefix)/$(cfg_dir):
		mkdir -p $(cfg_prefix)/$(cfg_dir)

install-cfg: $(cfg_prefix)/$(cfg_dir)
	$(INSTALL_CFG) tls.cfg $(cfg_prefix)/$(cfg_dir)
	./$(SCR_NAME)_cert.sh -d $(cfg_prefix)/$(cfg_dir)

install: install-cfg


DEFS+=-DOPENSER_MOD_INTERFACE

include ../../Makefile.modules
