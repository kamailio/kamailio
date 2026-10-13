#include "../config.h"
#include "../parser/msg_parser.h"
#include "../parser/parse_hname2.h"
#include "../parser/parse_body.h"
#include "../parser/parse_content.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	ksr_hname_init_index();
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	sip_msg_t msg;
	char *buf;
	int len;

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

	if(parse_msg(buf, size, &msg) == 0) {
		len = 0;
		(void)get_body_part(&msg, TYPE_APPLICATION, SUBTYPE_SDP, &len);
		len = 0;
		(void)get_body_part(&msg, TYPE_TEXT, SUBTYPE_PLAIN, &len);
		len = 0;
		(void)get_body_part(&msg, TYPE_MULTIPART, SUBTYPE_MIXED, &len);
		len = 0;
		(void)get_body_part(&msg, TYPE_APPLICATION, SUBTYPE_CPIM_PIDFXML, &len);
		len = 0;
		(void)get_body_part_by_filter(
				&msg, TYPE_APPLICATION, SUBTYPE_SDP, NULL, NULL, &len);
		len = 0;
		(void)get_body_part_by_filter(
				&msg, TYPE_TEXT, SUBTYPE_PLAIN, "1", NULL, &len);
	}

	free_sip_msg(&msg);
	free(buf);

	return 0;
}
