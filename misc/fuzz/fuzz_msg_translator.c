#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../parser/msg_parser.h"
#include "../msg_translator.h"
#include "../mem/mem.h"

static int msg_is_usable(sip_msg_t *msg)
{
	if(parse_headers(msg, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F | HDR_CSEQ_F, 0)
			< 0) {
		return 0;
	}
	if(msg->via1 == NULL || msg->via1->error != PARSE_OK) {
		return 0;
	}
	if(msg->callid == NULL || msg->cseq == NULL || msg->from == NULL
			|| msg->to == NULL) {
		return 0;
	}
	return 1;
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	ksr_hname_init_index();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	sip_msg_t msg;
	struct bookmark bmark;
	unsigned int len;
	char *buf;
	char *out;
	str reason = STR_STATIC_INIT("Not Found");

	if(size < 16 || size > 65535) {
		return 0;
	}

	buf = (char *)malloc(size + 1);
	if(buf == NULL) {
		return 0;
	}
	memcpy(buf, data, size);
	buf[size] = '\0';

	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = buf;
	msg.len = size;

	if(parse_msg(buf, size, &msg) == 0 && msg_is_usable(&msg)) {
		if(msg.first_line.type == SIP_REPLY) {
			out = build_res_buf_from_sip_res(&msg, &len, 0);
			if(out != NULL) {
				pkg_free(out);
			}
			out = generate_res_buf_from_sip_res(&msg, &len, 0);
			if(out != NULL) {
				pkg_free(out);
			}
		} else if(msg.first_line.type == SIP_REQUEST) {
			memset(&bmark, 0, sizeof(struct bookmark));
			out = build_res_buf_from_sip_req(
					404, &reason, NULL, &msg, &len, &bmark);
			if(out != NULL) {
				pkg_free(out);
			}
		}

		out = id_builder(&msg, &len);
		if(out != NULL) {
			pkg_free(out);
		}

		(void)received_test(&msg);
		(void)received_via_test(&msg);
	}

	free_sip_msg(&msg);
	free(buf);

	return 0;
}
