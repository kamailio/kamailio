#include "../config.h"
#include "../basex.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static int initialized = 0;
	if(!initialized) {
		init_basex();
		initialized = 1;
	}

	if(size < 1)
		return 0;

	uint8_t type = data[0] % 5;
	const uint8_t *input = data + 1;
	size_t input_len = size - 1;

	char *dst = NULL;
	int dst_len = 0;
	int res;

	switch(type) {
		case 0: {
			/* Base16 */
			dst_len = base16_enc_len(input_len) + 1;
			dst = (char *)malloc(dst_len);
			if(!dst)
				return 0;
			res = base16_enc((unsigned char *)input, input_len,
					(unsigned char *)dst, dst_len);
			if(res > 0) {
				char *dec = (char *)malloc(input_len + 1);
				if(dec) {
					base16_dec((unsigned char *)dst, res, (unsigned char *)dec,
							input_len + 1);
					free(dec);
				}
			}
			break;
		}
		case 1: {
			/* Base64 */
			dst_len = base64_enc_len(input_len) + 1;
			dst = (char *)malloc(dst_len);
			if(!dst)
				return 0;
			res = base64_enc((unsigned char *)input, input_len,
					(unsigned char *)dst, dst_len);
			if(res > 0) {
				char *dec = (char *)malloc(input_len + 1);
				if(dec) {
					base64_dec((unsigned char *)dst, res, (unsigned char *)dec,
							input_len + 1);
					free(dec);
				}
			}
			break;
		}
		case 2: {
			/* Q Base64 */
			dst_len = base64_enc_len(input_len) + 1;
			dst = (char *)malloc(dst_len);
			if(!dst)
				return 0;
			res = q_base64_enc((unsigned char *)input, input_len,
					(unsigned char *)dst, dst_len);
			if(res > 0) {
				char *dec = (char *)malloc(input_len + 1);
				if(dec) {
					q_base64_dec((unsigned char *)dst, res,
							(unsigned char *)dec, input_len + 1);
					free(dec);
				}
			}
			break;
		}
		case 3: {
			/* Base58 */
			int b58sz = input_len * 138 / 100 + 2;
			dst = (char *)malloc(b58sz);
			if(!dst)
				return 0;
			if(b58_encode(dst, &b58sz, (char *)input, input_len)) {
				int outbsz = input_len + 1;
				char *dec = (char *)malloc(outbsz);
				if(dec) {
					b58_decode(dec, &outbsz, dst, b58sz);
					free(dec);
				}
			}
			break;
		}
		case 4: {
			/* Base64URL */
			dst_len = ((input_len + 2) / 3 * 4) + 1;
			dst = (char *)malloc(dst_len);
			if(!dst)
				return 0;
			res = base64url_enc((char *)input, input_len, dst, dst_len);
			if(res >= 0) {
				int out_max = (res * 3 / 4) + 1;
				char *dec = (char *)malloc(out_max);
				if(dec) {
					base64url_dec(dst, res, dec, out_max);
					free(dec);
				}
			}
			break;
		}
	}

	if(dst)
		free(dst);
	return 0;
}
