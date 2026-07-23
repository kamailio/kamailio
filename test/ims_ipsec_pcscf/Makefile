# ims_ipsec_pcscf 3GPP Rel-18 Enhancement Unit Tests
#
# Build and run:
#   make test
#
# Clean:
#   make clean

KAMAILIO_SRC = ../../src
MODULE_DIR = $(KAMAILIO_SRC)/modules/ims_ipsec_pcscf

CC = gcc
CFLAGS = -D_IPSEC_SPI_LIST_TEST -D_GNU_SOURCE -DNAME=\"kamailio\" \
         -DHAVE_GETHOSTBYNAME2 -DFAST_LOCK -DCC_GCC_LIKE_ASM -D__CPU_x86_64 -DSTATISTICS \
         -I../../ -O2 -Wall -Wno-unused-parameter
LDFLAGS = -lmnl

TEST_BIN = test_ims_ipsec_rel18

SOURCES = test_ims_ipsec_rel18.c \
          $(MODULE_DIR)/ipsec_alg.c

.PHONY: all test clean

all: $(TEST_BIN)

$(TEST_BIN): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)
