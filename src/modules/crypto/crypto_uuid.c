/*
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2016 Travis Cross <tc@traviscross.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Crypto :: Fast enough high entropy Call-ID generator
 *
 * Fast enough high entropy Call-ID generator. The Call-ID generated
 * is an RFC 4122 version 4 UUID using high quality entropy from
 * OpenSSL.  This entropy is extracted only once at startup and is
 * then combined in each child with the process ID and a counter.  The
 * result is whitened with SHA1 and formatted per RFC 4122.
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include "../../core/dprint.h"
#include "../../core/srapi.h"
#include "crypto_uuid.h"

#define SEED_LEN 16
#define CTR_LEN 16
static unsigned char crypto_callid_seed[SEED_LEN] = {0};
static unsigned char crypto_callid_counter[CTR_LEN] = {0};


/**
 * \brief Convert value to hex character
 * \param x unsigned char byte
 * \return lowercase hex charater
 */
static inline char crypto_byte2hex(unsigned char x) {
	return x < 10 ? '0' + x : 'a' + (x-10);
}

/**
 * \brief Convert array of bytes to hexidecimal string
 * \param sbuf output character array
 * \param sbuf_len allocated size of sbuf, must be 2x buf_len
 * \param buf input byte array
 * \param buf_len number of bytes of buf
 * \return 0 on success, -1 on error
 */
static inline int crypto_bytes2hex(char *sbuf, size_t sbuf_len,
							unsigned char *buf, size_t buf_len) {
	size_t i, j;
	if (sbuf_len < 2*buf_len) return -1;
	for (i=0, j=(2*buf_len)-1; i<sbuf_len; i++, j--) {
		sbuf[i] = crypto_byte2hex((buf[j/2] >> (j%2 ? 0 : 4)) % 0x0f);
		if (j == 0) break;
	}
	return 0;
}


/**
 * \brief Initialize the Call-ID generator
 * \return 0 on success, -1 on error
 */
int crypto_init_callid(void)
{
	static char crypto_callid_seed_str[2*SEED_LEN] = {0};
	if (!(RAND_bytes(crypto_callid_seed,sizeof(crypto_callid_seed)))) {
		LOG(L_ERR, "ERROR: Unable to get random bytes for Call-ID seed\n");
		return -1;
	}
	crypto_bytes2hex(crypto_callid_seed_str, sizeof(crypto_callid_seed_str),
			crypto_callid_seed, sizeof(crypto_callid_seed));
	DBG("Call-ID initialization: '0x%.*s'\n", 2*SEED_LEN,
			crypto_callid_seed_str);
	return 0;
}


/**
 * \brief Child initialization, permute seed with pid
 * \param rank not used
 * \return 0 on success, -1 on error
 */
int crypto_child_init_callid(int rank)
{
	static char crypto_callid_seed_str[2*SEED_LEN] = {0};
	unsigned int pid = my_pid();
	if (SEED_LEN < 2) {
		LOG(L_CRIT, "BUG: Call-ID seed is too short\n");
		return -1;
	}
	crypto_callid_seed[0] ^= (pid >> 0) % 0xff;
	crypto_callid_seed[1] ^= (pid >> 8) % 0xff;
	crypto_bytes2hex(crypto_callid_seed_str, sizeof(crypto_callid_seed_str),
			crypto_callid_seed, sizeof(crypto_callid_seed));
	DBG("Call-ID initialization: '0x%.*s'\n", 2*SEED_LEN,
			crypto_callid_seed_str);
	return 0;
}


/**
 * \brief Increment a counter
 * \param ctr input array of bytes
 * \param len length of byte array
 * \return void
 */
static inline void crypto_inc_counter(unsigned char* ctr, size_t len)
{
	size_t i;
	for (i=0; i < len; i++) {
		ctr[i] += 1;
		if (ctr[i]) break;
	}
}


/**
 * \brief Convert array of bytes to RFC 4122 UUID (version 4)
 * \param sbuf output character array
 * \param sbuf_len allocated size of sbuf, must be at least 36
 * \param buf input byte array
 * \param buf_len number of bytes of buf, must be at least 16
 * \return 0 on success, -1 on error
 */
#define UUID_LEN 36
static inline int crypto_format_rfc4122_uuid(char *sbuf, size_t sbuf_len,
		unsigned char *buf, size_t buf_len)
{
	size_t i, j;
	if (sbuf_len < UUID_LEN) return -1;
	if (buf_len < UUID_LEN/2) return -1;
	buf[6] &= 0x0f;
	buf[6] |= 4 << 4;
	buf[8] &= 0x3f;
	buf[8] |= 2 << 6;
	for (i=0, j=0; i<UUID_LEN; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			sbuf[i] = '-';
			continue;
		}
		sbuf[i] = crypto_byte2hex((buf[j/2] >> (j%2 ? 0 : 4)) % 0x0f);
		if (!(++j/2 < buf_len)) break;
	}
	return 0;
}


/**
 * \brief Get a unique Call-ID
 * \param callid returned Call-ID
 */
void crypto_generate_callid(str* callid)
{
	static SHA_CTX crypto_ctx = {0};
	static unsigned char crypto_buf[SHA_DIGEST_LENGTH] = {0};
	static char crypto_sbuf[UUID_LEN] = {0};
	crypto_inc_counter(crypto_callid_counter, CTR_LEN);
	SHA1_Init(&crypto_ctx);
	SHA1_Update(&crypto_ctx, crypto_callid_seed, SEED_LEN);
	SHA1_Update(&crypto_ctx, crypto_callid_counter, CTR_LEN);
	SHA1_Final(crypto_buf, &crypto_ctx);
	crypto_format_rfc4122_uuid(crypto_sbuf, sizeof(crypto_sbuf),
			crypto_buf, sizeof(crypto_buf));
	callid->s = crypto_sbuf;
	callid->len = sizeof(crypto_sbuf);
}


/**
 *
 */
int crypto_register_callid_func(void)
{
	if(sr_register_callid_func(crypto_generate_callid)<0) {
		LM_ERR("unable to register callid func\n");
		return -1;
	}
	return 0;
}


/**
 * \brief generate SHA1 hash over a given input string
 * \param str to apply hash over
 * \param SHA1 hash
 */
int crypto_generate_SHA1(str* in, str* hash)
{
	static unsigned char crypto_buf[SHA_DIGEST_LENGTH];

	if (in == NULL || in->s == NULL) {
		LM_ERR("Invalid input string!\n");
		return -1;
	}

	if (hash == NULL) {
		LM_ERR("Invalid output hash str!\n");
		return -1;
	}

	void* ret;
	if ((ret=SHA1((unsigned char *)in->s, in->len, crypto_buf)) != crypto_buf) {
		LM_ERR("SHA1 algorithm failed!\n");
		LM_DBG("return value from library %p\n", ret);
		return -1;
	}

	hash->s = (char *)crypto_buf;
	hash->len = sizeof(crypto_buf);

	return 0;
}
