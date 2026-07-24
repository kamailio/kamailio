TOPDIR ?= ../../..
CC ?= gcc
CFLAGS ?= -Wall -Wextra -g
DEFS = -DUNIT_TEST -DUSE_PTHREAD_MUTEX
INCLUDES = -I$(TOPDIR)/src -I$(TOPDIR)/src/modules/ims_registrar_pcscf \
	-I$(TOPDIR)/src/lib/ims

TESTS = test_gruu_extract test_path_build

all: $(TESTS)

test_gruu_extract: test_gruu_extract.c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $<

test_path_build: test_path_build.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections $(DEFS) $(INCLUDES) \
		-Wl,--gc-sections -o $@ test_path_build.c \
		$(TOPDIR)/src/modules/ims_registrar_pcscf/path.c -lpthread

test: all
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -f $(TESTS)
