#include "../config.h"
#include "../pvar.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if(size < 2 || size > 4096) {
		return 0;
	}

	/* Make a mutable copy. */
	char *buf = (char *)malloc(size + 1);
	if(buf == NULL) {
		return 0;
	}
	memcpy(buf, data, size);
	buf[size] = '\0';

	str input;
	input.s = buf;
	input.len = (int)size;

	pv_elem_p elements = NULL;

	pv_parse_format(&input, &elements);

	if(elements) {
		pv_elem_free_all(elements);
	}

	free(buf);
	return 0;
}
