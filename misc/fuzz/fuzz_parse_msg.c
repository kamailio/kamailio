#include "../config.h"
#include "../parser/sdp/sdp.h"
#include "../parser/parse_uri.c"
#include "../parser/parse_hname2.h"
#include "../parser/contact/parse_contact.h"
#include "../parser/parse_from.h"
#include "../parser/parse_to.h"
#include "../parser/parse_rr.h"
#include "../parser/parse_refer_to.h"
#include "../parser/parse_ppi_pai.h"
#include "../parser/parse_privacy.h"
#include "../parser/parse_diversion.h"
#include "../parser/parse_identityinfo.h"
#include "../parser/parse_disposition.h"
#include "../tcp_conn.h"
#include "../tcp_read.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	ksr_hname_init_index();
	return 0;
}

int ksr_fuzz_tcp_read(char *buf, size_t bsize)
{
	char *p;
	size_t rsize;
	struct tcp_connection c;
	rd_conn_flags_t read_flags;

	if(bsize >= (1 << 24)) {
		/* limit the size */
		return 0;
	}

	rsize = bsize + 5;
	p = (char *)malloc(rsize + 1);
	if(p == NULL) {
		return -1;
	}
	memcpy(p, "MSRP ", 5);
	memcpy(p + 5, buf, bsize);
	p[rsize] = '\0';

	memset(&c, 0, sizeof(struct tcp_connection));
	init_tcp_req(&c.req, p + 5, bsize);
	c.req.pos += bsize;
	c.s = -1;
	c.fd = -1;
	c.state = S_CONN_OK;
	c.type = PROTO_TCP;
	c.rcv.proto = PROTO_TCP;
	c.flags |= F_CONN_NORECV;
	read_flags = 0;
	tcp_read_headers(&c, &read_flags, 1);

	memset(&c, 0, sizeof(struct tcp_connection));
	init_tcp_req(&c.req, p, rsize);
	c.req.pos += rsize;
	c.s = -1;
	c.fd = -1;
	c.state = S_CONN_OK;
	c.type = PROTO_TCP;
	c.rcv.proto = PROTO_TCP;
	c.flags |= F_CONN_NORECV;
	read_flags = 0;
	tcp_read_headers(&c, &read_flags, 1);

	free(p);

	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	sip_msg_t orig_inv = {};

	ksr_fuzz_tcp_read((char *)data, size);

	orig_inv.buf = (char *)data;
	orig_inv.len = size;

	if(size >= 4 * BUF_SIZE) {
		/* test with larger message than core accepts, but not indefinitely large */
		return 0;
	}

	if(parse_msg(orig_inv.buf, orig_inv.len, &orig_inv) < 0) {
		goto cleanup;
	}

	parse_headers(&orig_inv, HDR_EOH_F, 0);

	parse_sdp(&orig_inv);

	parse_from_header(&orig_inv);

	parse_from_uri(&orig_inv);

	parse_to_header(&orig_inv);

	parse_to_uri(&orig_inv);

	parse_contact_headers(&orig_inv);

	parse_refer_to_header(&orig_inv);

	parse_pai_header(&orig_inv);

	parse_diversion_header(&orig_inv);

	parse_privacy(&orig_inv);

	parse_content_disposition(&orig_inv);

	parse_identityinfo_header(&orig_inv);

	parse_record_route_headers(&orig_inv);

	parse_route_headers(&orig_inv);

	str uri;
	get_src_uri(&orig_inv, 0, &uri);

	str ssock;
	get_src_address_socket(&orig_inv, &ssock);

cleanup:
	free_sip_msg(&orig_inv);

	return 0;
}
