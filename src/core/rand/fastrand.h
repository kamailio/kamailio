/*
 * fast pseudo random generation 
 *
 * Copyright (C) 2007 iptelorg GmbH
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
 *
 *  2007-06-15  wrapper around isaac (see 
 *              http://www.burtleburtle.net/bob/rand/isaacafa.html) (andrei)
 */

#ifndef _fastrand_h
#define _fastrand_h


/* side effect: seeds also random w/ seed */
void fastrand_seed(unsigned int seed);
/* generate a 32 bit random number */
unsigned int fastrand(void);
/* generate a random number between 0 and max inclusive ( 0 <= r <= max)
 * should not be used for cryptography */
unsigned int fastrand_max(unsigned int max);

#endif
