/*
 * cryptographic secure pseudo random generation
 *
 * Copyright (C) 2019 Henning Westerholt
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/* wrapper around fortuna cryptographic pseudo random number generator
 * https://en.wikipedia.org/wiki/Fortuna_(PRNG)
 */

#include "cryptorand.h"

#include "fortuna/random.h"
#include "../dprint.h"

/* seed the generator, will also use system randomness */
void cryptorand_seed(const unsigned int seed) {
	u_int8_t bytes[4];
	
	bytes[0] = (seed >> 24) & 0xFF;
	bytes[1] = (seed >> 16) & 0xFF;
	bytes[2] = (seed >> 8)  & 0xFF;
	bytes[3] = seed & 0xFF;

	LM_DBG("seeding cryptorand generator with %u\n", seed);
	sr_add_entropy(bytes, 4);
}

/* generate a 32 bit random number */
unsigned int cryptorand(void) {
	u_int8_t bytes[4];
	u_int32_t result = 0;
	
	sr_get_pseudo_random_bytes(bytes, 4);
	result |= (u_int32_t)bytes[0] << 24;
	result |= (u_int32_t)bytes[1] << 16;
	result |= (u_int32_t)bytes[2] << 8;
	result |= (u_int32_t)bytes[3];
	
	return result;
}

