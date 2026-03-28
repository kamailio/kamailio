#include "../config.h"
#include "../strutils.h"
#include "../parser/parse_uri.c"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if(size < 2 || size > 8192) {
		return 0;
	}

	/* Use first byte to select operation, rest is input data. */
	uint8_t op = data[0];
	const char *input = (const char *)data + 1;
	size_t input_len = size - 1;

	/* Allocate output buffers large enough for any escaping. */
	size_t out_len = input_len * 4 + 16;
	char *dst = (char *)malloc(out_len);
	if(dst == NULL) {
		return 0;
	}

	str sin, sout;
	sin.s = (char *)input;
	sin.len = (int)input_len;
	sout.s = dst;
	sout.len = (int)out_len;

	switch(op % 10) {
		case 0:
			/* escape/unescape common */
			escape_common(dst, (char *)input, (int)input_len);
			unescape_common(dst, (char *)input, (int)input_len);
			break;
		case 1:
			/* escape/unescape user */
			escape_user(&sin, &sout);
			unescape_user(&sin, &sout);
			break;
		case 2:
			/* escape/unescape param */
			escape_param(&sin, &sout);
			unescape_param(&sin, &sout);
			break;
		case 3:
			/* escape/unescape CRLF */
			escape_crlf(&sin, &sout);
			unescape_crlf(&sin, &sout);
			break;
		case 4:
			/* escape CSV */
			escape_csv(&sin, &sout);
			break;
		case 5:
			/* URL encode/decode */
			urlencode(&sin, &sout);
			urldecode(&sin, &sout);
			break;
		case 6: {
			/* URI comparison: split input in half, parse each as URI,
			   then compare. */
			size_t half = input_len / 2;
			if(half < 4)
				break;

			str s1, s2;
			s1.s = (char *)input;
			s1.len = (int)half;
			s2.s = (char *)input + half;
			s2.len = (int)(input_len - half);

			cmp_uri_str(&s1, &s2);
			cmp_uri_light_str(&s1, &s2);
			cmp_aor_str(&s1, &s2);
			break;
		}
		case 7: {
			/* String comparisons */
			size_t half = input_len / 2;
			if(half < 2)
				break;

			str s1, s2;
			s1.s = (char *)input;
			s1.len = (int)half;
			s2.s = (char *)input + half;
			s2.len = (int)(input_len - half);

			cmp_str(&s1, &s2);
			cmpi_str(&s1, &s2);
			cmp_hdrname_str(&s1, &s2);
			break;
		}
		case 8: {
			/* JSON escaping */
			int emode = 0;
			ksr_str_json_escape(&sin, &sout, &emode);
			break;
		}
		case 9: {
			/* Standalone URI parse and compare */
			struct sip_uri uri1, uri2;
			size_t half = input_len / 2;
			if(half < 8)
				break;

			if(parse_uri((char *)input, (int)half, &uri1) == 0
					&& parse_uri((char *)input + half, (int)(input_len - half),
							   &uri2)
							   == 0) {
				cmp_uri(&uri1, &uri2);
				cmp_uri_light(&uri1, &uri2);
				cmp_aor(&uri1, &uri2);
			}
			break;
		}
	}

	free(dst);
	return 0;
}
