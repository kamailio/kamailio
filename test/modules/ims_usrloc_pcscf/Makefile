CC = gcc
SRC_MOD = ../../../src/modules/ims_usrloc_pcscf
STAGE = stubs/modules/ims_usrloc_pcscf

# Production ims_usrloc_pcscf/*.c no longer guard real Kamailio includes
# (dprint.h, usrloc.h, parser/parse_uri.h) behind #ifdef UNIT_TEST - they
# always use the real headers, as required for production code. Those
# real headers pull in the full core config/module framework and cannot
# be linked into a standalone test binary.
#
# To still compile the unmodified production .c files here, pcontact_index.c
# and impu_match.c (+impu_match.h, whose own "usrloc.h" include must also
# resolve to the stub) are staged (byte-for-byte copied) into $(STAGE)/,
# which mirrors src/modules/ims_usrloc_pcscf/'s position under src/. From
# that staged location, their relative "../../core/..." includes and bare
# "usrloc.h" include resolve to the stub headers below instead of the real
# ones. pcontact_index.h has no such heavy include and is *not* staged, so
# it (and everything else) still resolves to the real, unmodified header.
INCLUDES = -I$(STAGE) -I$(SRC_MOD) -I../../../src
CFLAGS = -Wall -Wextra $(INCLUDES)

.PHONY: all test clean stage
all: test_db_layout test_pcontact_serialize test_pcontact_index test_temp_gruu_lru test_impu_match

test: all
	@for t in test_db_layout test_pcontact_serialize test_pcontact_index test_temp_gruu_lru test_impu_match; do ./$$t || exit 1; done

stage: $(STAGE)/pcontact_index.c $(STAGE)/impu_match.c $(STAGE)/impu_match.h

$(STAGE)/%.c: $(SRC_MOD)/%.c
	@mkdir -p $(STAGE)
	cp $< $@

$(STAGE)/%.h: $(SRC_MOD)/%.h
	@mkdir -p $(STAGE)
	cp $< $@

test_db_layout: test_db_layout.c
	$(CC) $(CFLAGS) -o $@ test_db_layout.c

test_pcontact_serialize: test_pcontact_serialize.c
	$(CC) $(CFLAGS) -o $@ test_pcontact_serialize.c $(SRC_MOD)/pcontact_serialize.c

test_pcontact_index: test_pcontact_index.c stage
	$(CC) $(CFLAGS) -o $@ test_pcontact_index.c $(STAGE)/pcontact_index.c

test_temp_gruu_lru: test_temp_gruu_lru.c stage
	$(CC) $(CFLAGS) -o $@ test_temp_gruu_lru.c $(STAGE)/pcontact_index.c

test_impu_match: test_impu_match.c test_parse_uri_stub.c stage
	$(CC) $(CFLAGS) -o $@ test_impu_match.c test_parse_uri_stub.c $(STAGE)/impu_match.c

clean:
	rm -f test_db_layout test_pcontact_serialize test_pcontact_index \
		test_temp_gruu_lru test_impu_match test_parse_uri_stub
	rm -rf stubs/modules/ims_usrloc_pcscf/pcontact_index.c \
		stubs/modules/ims_usrloc_pcscf/impu_match.c \
		stubs/modules/ims_usrloc_pcscf/impu_match.h
