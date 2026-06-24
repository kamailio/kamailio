#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../globals.h"
#include "../dprint.h"
#include "../parser/msg_parser.h"
#include "../parser/parse_hname2.h"
#include "../msg_translator.h"

/* check_boundaries() is a core function not exposed via a public header */
int check_boundaries(struct sip_msg *msg, struct dest_info *send_info);

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	ksr_hname_init_index();
	return 0;
}

/* Exercise check_boundaries(), the multipart body rewriting path. The body
 * and the Content-Type boundary value are attacker-controlled, so this drives
 * the boundary list building and the body rebuild where a too-small output
 * buffer previously caused a heap buffer overflow. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	sip_msg_t msg = {0};
	struct dest_info send_info;
	char *buf;

	if(size >= 4 * BUF_SIZE) {
		/* test with larger message than core accepts, but bounded */
		return 0;
	}

	/* parse_msg() mutates and expects a NUL-terminated buffer */
	buf = (char *)malloc(size + 1);
	if(buf == NULL) {
		return 0;
	}
	memcpy(buf, data, size);
	buf[size] = '\0';

	msg.buf = buf;
	msg.len = size;
	if(parse_msg(msg.buf, msg.len, &msg) < 0) {
		goto cleanup;
	}
	if(parse_headers(&msg, HDR_EOH_F, 0) < 0) {
		goto cleanup;
	}
	/* check_boundaries() locates the boundary in the Content-Type header */
	if(msg.content_type == NULL) {
		goto cleanup;
	}

	msg.msg_flags |= FL_BODY_MULTIPART;
	memset(&send_info, 0, sizeof(send_info));
	check_boundaries(&msg, &send_info);

cleanup:
	free_sip_msg(&msg);
	free(buf);
	return 0;
}
