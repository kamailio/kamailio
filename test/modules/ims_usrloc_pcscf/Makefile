CC = gcc
CFLAGS = -Wall -Wextra -I../../../src/modules/ims_usrloc_pcscf -DUNIT_TEST
SRC = test_db_layout.c
TARGET = test_db_layout

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) test_pcontact_serialize test_pcontact_index test_temp_gruu_lru
	rm -f test_impu_match test_parse_uri_stub

test_pcontact_serialize: test_pcontact_serialize.c
	$(CC) $(CFLAGS) -o test_pcontact_serialize test_pcontact_serialize.c ../../../src/modules/ims_usrloc_pcscf/pcontact_serialize.c

test_pcontact_index: test_pcontact_index.c
	$(CC) $(CFLAGS) -o test_pcontact_index test_pcontact_index.c ../../../src/modules/ims_usrloc_pcscf/pcontact_index.c

test_temp_gruu_lru: test_temp_gruu_lru.c
	$(CC) $(CFLAGS) -o test_temp_gruu_lru test_temp_gruu_lru.c ../../../src/modules/ims_usrloc_pcscf/pcontact_index.c

test_impu_match: test_impu_match.c test_parse_uri_stub.c
	$(CC) $(CFLAGS) -o test_impu_match test_impu_match.c test_parse_uri_stub.c ../../../src/modules/ims_usrloc_pcscf/impu_match.c

test_parse_uri_stub: test_parse_uri_stub.c
	$(CC) $(CFLAGS) -o test_parse_uri_stub test_parse_uri_stub.c

