/*
 * random.c
 *		Acquire randomness from system, for seeding RNG.
 *		Get pseudo random numbers from RNG.
 *
 * Copyright (c) 2001 Marko Kreen
 * Copyright (c) 2019 Henning Westerholt
 * All rights reserved.
 *
 * Based on https://github.com/waitman/libfortuna, refactoring
 * done in this version: https://github.com/henningw/libfortuna
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

#include "../../dprint.h"

#include "random.h"
#include "fortuna.h"

/* how many bytes to ask from system random provider */
#define RND_BYTES  32

/*
 * Try to read from /dev/urandom or /dev/random on these OS'es.
 *
 * The list can be pretty liberal, as the device not existing
 * is expected event.
 */

#include <fcntl.h>
#include <unistd.h>

static time_t seed_time = 0;
static time_t check_time = 0;

int sr_get_pseudo_random_bytes(u_int8_t *dst, unsigned count);

/* private functions */

static int safe_read(int fd, void *buf, size_t count)
{
	int done = 0;
	char *p = buf;
	int res;

	while (count)
	{
		res = read(fd, p, count);
		if (res <= 0)
		{
			if (errno == EINTR)
				continue;
			return -10;
		}
		p += res;
		done += res;
		count -= res;
	}
	return done;
}

static u_int8_t * try_dev_random(u_int8_t *dst)
{
	int fd;
	int res;

	fd = open("/dev/urandom", O_RDONLY, 0);
	if (fd == -1)
	{
		fd = open("/dev/random", O_RDONLY, 0);
		if (fd == -1)
			return dst;
	}
	res = safe_read(fd, dst, RND_BYTES);
	close(fd);
	if (res > 0)
		dst += res;
	return dst;
}

/*
 * try to extract some randomness for initial seeding
 *
 * dst should have room for 1024 bytes.
 */
static unsigned acquire_system_randomness(u_int8_t *dst)
{
	u_int8_t *p = dst;

	p = try_dev_random(p);
	return p - dst;
}

static void system_reseed(void)
{
	u_int8_t buf[1024];
	int n;
	time_t t;
	int skip = 1;

	t = time(NULL);

	if (seed_time == 0)
		skip = 0;
	else if ((t - seed_time) < SYSTEM_RESEED_MIN)
		skip = 1;
	else if ((t - seed_time) > SYSTEM_RESEED_MAX)
		skip = 0;
	else if (check_time == 0 ||
			(t - check_time) > SYSTEM_RESEED_CHECK_TIME)
	{
		check_time = t;

		/* roll dice */
		sr_get_pseudo_random_bytes(buf, 1);
		skip = buf[0] >= SYSTEM_RESEED_CHANCE;
	}
	/* clear 1 byte */
	memset(buf, 0, sizeof(buf));

	if (skip)
		return;

	n = acquire_system_randomness(buf);
	if (n > 0) {
		fortuna_add_entropy(buf, n);
		LM_DBG("cryptographic PRNG reseed done with %u bytes\n", n);
	}

	seed_time = t;
	memset(buf, 0, sizeof(buf));
}


/* public functions */

int sr_get_pseudo_random_bytes(u_int8_t *dst, unsigned count)
{
	system_reseed();
	fortuna_get_bytes(count, dst);
	return 0;
}


int sr_add_entropy(const u_int8_t *data, unsigned count)
{
	system_reseed();
	LM_DBG("additional %u bytes entropy added to cryptographic PRNG\n", count);
	fortuna_add_entropy(data, count);
	return 0;
}
