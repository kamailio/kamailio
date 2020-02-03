/*
 * random.h
 *              Acquire randomness from system.  For seeding RNG.
 *              Get pseudo random numbers from RNG.
 *
 * Copyright (c) 2001 Marko Kreen
 * Copyright (c) 2019 Henning Westerholt
 * All rights reserved.
 *
 * Based on https://github.com/waitman/libfortuna, several cleanups
 * done in this version: https://github.com/henningw/libfortuna
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.      IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * contrib/pgcrypto/random.c
 */

#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <sys/types.h>

/*
 * System reseeds should be separated at least this much.
 */
#define SYSTEM_RESEED_MIN                       (20*60)         /* 20 min */
/*
 * How often to roll dice.
 */
#define SYSTEM_RESEED_CHECK_TIME        (10*60)         /* 10 min */
/*
 * The chance is x/256 that the reseed happens.
 */
#define SYSTEM_RESEED_CHANCE            (4) /* 256/4 * 10min ~ 10h */

/*
 * If this much time has passed, force reseed.
 */
#define SYSTEM_RESEED_MAX                       (12*60*60)      /* 12h */

int sr_get_pseudo_random_bytes(u_int8_t *dst, unsigned count);

int sr_add_entropy(const u_int8_t *data, unsigned count);


#endif
