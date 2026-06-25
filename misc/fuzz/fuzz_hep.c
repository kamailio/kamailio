/*
 * libFuzzer / OSS-Fuzz target for the HEPv3 capture-packet chunk parser in
 * src/modules/sipcapture/hep.c.
 *
 * The HEPv3 parsers (parsing_hepv3_message, hepv3_message_parse,
 * hepv3_get_chunk) are sipcapture module functions that pull in the whole
 * module and the SIP receive path, so they cannot be linked into the core
 * OSS-Fuzz harness the way the parse_* targets are. This target is therefore
 * self-contained: it walks the chunk list of a fuzzed HEP datagram using the
 * same control-header and per-chunk bounds checks the module uses
 * (hepv3_total_length / hepv3_chunk_ok) and dereferences each chunk the way
 * the parser does, so AddressSanitizer flags any read past the datagram. Keep
 * the three helpers below in sync with src/modules/sipcapture/hep.c.
 *
 * Standalone build:
 *   clang -fsanitize=address,fuzzer misc/fuzz/fuzz_hep.c -o fuzz_hep
 */
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

typedef struct
{
	uint16_t vendor_id;
	uint16_t type_id;
	uint16_t length;
} __attribute__((packed)) hep_chunk_t;

typedef struct
{
	hep_chunk_t chunk;
	uint8_t data;
} __attribute__((packed)) hep_chunk_uint8_t;

typedef struct
{
	hep_chunk_t chunk;
	uint16_t data;
} __attribute__((packed)) hep_chunk_uint16_t;

typedef struct
{
	hep_chunk_t chunk;
	uint32_t data;
} __attribute__((packed)) hep_chunk_uint32_t;

typedef struct
{
	hep_chunk_t chunk;
	struct in_addr data;
} __attribute__((packed)) hep_chunk_ip4_t;

typedef struct
{
	hep_chunk_t chunk;
	struct in6_addr data;
} __attribute__((packed)) hep_chunk_ip6_t;

typedef struct
{
	char id[4];
	uint16_t length;
} __attribute__((packed)) hep_ctrl_t;

/* --- mirror of the helpers in src/modules/sipcapture/hep.c --- */
static unsigned int hepv3_chunk_minlen(int chunk_type)
{
	switch(chunk_type) {
		case 1:
		case 2:
		case 11:
			return sizeof(hep_chunk_uint8_t);
		case 3:
		case 4:
			return sizeof(hep_chunk_ip4_t);
		case 5:
		case 6:
			return sizeof(hep_chunk_ip6_t);
		case 7:
		case 8:
		case 13:
			return sizeof(hep_chunk_uint16_t);
		case 9:
		case 10:
		case 12:
			return sizeof(hep_chunk_uint32_t);
		default:
			return sizeof(hep_chunk_t);
	}
}

static int hepv3_chunk_ok(const char *buf, unsigned int blen, unsigned int off)
{
	const hep_chunk_t *chunk;
	unsigned int minlen;
	int chunk_vendor, chunk_length;

	if(off + sizeof(hep_chunk_t) > blen)
		return -1;
	chunk = (const hep_chunk_t *)(buf + off);
	chunk_vendor = ntohs(chunk->vendor_id);
	chunk_length = ntohs(chunk->length);
	minlen = sizeof(hep_chunk_t);
	if(chunk_vendor == 0)
		minlen = hepv3_chunk_minlen(ntohs(chunk->type_id));
	if(chunk_length < (int)minlen)
		return -1;
	if(off + (unsigned int)chunk_length > blen)
		return -1;
	return 0;
}

static int hepv3_total_length(const char *buf, unsigned int len)
{
	int total_length;

	if(len < sizeof(hep_ctrl_t))
		return -1;
	total_length = ntohs(((const hep_ctrl_t *)buf)->length);
	if((unsigned int)total_length > len)
		return -1;
	return total_length;
}

/* volatile sinks so the typed dereferences are not optimized away */
static volatile unsigned int g_u;
static volatile unsigned char g_addr[16];

/* Walk the chunks the way parsing_hepv3_message() does, dereferencing the
 * fixed-width value of each known chunk type. With the bounds checks in place
 * every dereference must stay inside the datagram. */
static void hepv3_walk(const char *buf, unsigned int len)
{
	int i, total_length, chunk_vendor, chunk_type, chunk_length;
	const char *tmp;

	total_length = hepv3_total_length(buf, len);
	if(total_length < 0)
		return;

	i = sizeof(hep_ctrl_t);
	while(i < total_length) {
		if(hepv3_chunk_ok(buf, (unsigned int)total_length, (unsigned int)i) < 0)
			return;

		tmp = buf + i;
		chunk_vendor = ntohs(((const hep_chunk_t *)tmp)->vendor_id);
		chunk_type = ntohs(((const hep_chunk_t *)tmp)->type_id);
		chunk_length = ntohs(((const hep_chunk_t *)tmp)->length);

		if(chunk_vendor == 0) {
			switch(chunk_type) {
				case 1:
				case 2:
				case 11:
					g_u += ((const hep_chunk_uint8_t *)tmp)->data;
					break;
				case 3:
				case 4:
					g_u += ((const hep_chunk_ip4_t *)tmp)->data.s_addr;
					break;
				case 5:
				case 6:
					memcpy((void *)g_addr,
							&((const hep_chunk_ip6_t *)tmp)->data, 16);
					break;
				case 7:
				case 8:
				case 13:
					g_u += ntohs(((const hep_chunk_uint16_t *)tmp)->data);
					break;
				case 9:
				case 10:
				case 12:
					g_u += ntohl(((const hep_chunk_uint32_t *)tmp)->data);
					break;
				default:
					break;
			}
		}
		i += chunk_length;
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if(size < sizeof(hep_ctrl_t) || size > 65535) {
		/* HEP datagrams are bounded by a 16-bit length field */
		return 0;
	}
	hepv3_walk((const char *)data, (unsigned int)size);
	return 0;
}
