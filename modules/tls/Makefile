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
LIBS+=	-L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
		-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
		-lssl  -lcrypto \
		$(TLS_EXTRA_LIBS)
# NOTE: depending on the way in which libssl was compiled you might
#       have to add -lz -lkrb5   (zlib and kerberos5).
#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"
MOD_INSTALL_CFGS=tls.cfg

DEFS+=-DOPENSER_MOD_INTERFACE

include ../../Makefile.modules


install-tls-cert: $(cfg_prefix)/$(cfg_dir)
	MAIN_NAME=$(MAIN_NAME) ./$(SCR_NAME)_cert.sh -d $(cfg_prefix)/$(cfg_dir)

install-cfg:  install-tls-cert
