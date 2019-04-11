COREPATH=../../../../../src
include $(COREPATH)/Makefile.defs
include $(COREPATH)/Makefile.targets


.PHONY: all
all: openssl_mutex_shared.so

.PHONY: install-if-newer
install-if-newer: install

.PHONY: install
install: install-modules

.PHONY: install-modules
install-modules: openssl_mutex_shared.so
	mkdir -p $(modules_prefix)/$(lib_dir)/openssl_mutex_shared
	$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/openssl_mutex_shared/openssl_mutex_shared.so
	$(INSTALL_BIN) openssl_mutex_shared.so $(modules_prefix)/$(lib_dir)/openssl_mutex_shared

openssl_mutex_shared.so: openssl_mutex_shared.c
	$(CC) -g -D_GNU_SOURCE -std=c99 -fvisibility=hidden -pthread -o $@ -O3 -Wall -shared -fPIC -ldl $<

.PHONY: clean
clean:
	rm -f openssl_mutex_shared.so
