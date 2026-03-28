#include "../config.h"
#include "../parser/digest/digest_parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if(size < 4 || size > 4096) {
		return 0;
	}

	/* Make a mutable copy since the parser may modify the input. */
	char *buf = (char *)malloc(size + 1);
	if(buf == NULL) {
		return 0;
	}
	memcpy(buf, data, size);
	buf[size] = '\0';

	str input;
	input.s = buf;
	input.len = (int)size;

	dig_cred_t cred;
	memset(&cred, 0, sizeof(cred));

	parse_digest_cred(&input, &cred);

	free(buf);
	return 0;
}
