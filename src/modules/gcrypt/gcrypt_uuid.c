/*
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * \brief Gcrypt :: Fast enough high entropy Call-ID generator
 *
 * Fast enough high entropy Call-ID generator. The Call-ID generated
 * is an RFC 4122 version 4 UUID using high quality entropy from
 * libgcrypt. This entropy is extracted only once at startup and is
 * then combined in each child with the process ID and a counter. The
 * result is whitened with SHA1 and formatted per RFC 4122.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gcrypt.h>
#include "../../core/dprint.h"
#include "../../core/srapi.h"
#include "gcrypt_uuid.h"

#define CTR_LEN 16
static unsigned char gcrypt_callid_counter[CTR_LEN] = {0};


/**
 * \brief Convert value to hex character
 * \param x unsigned char byte
 * \return lowercase hex charater
 */
static inline char gcrypt_byte2hex(unsigned char x)
{
	return x < 10 ? '0' + x : 'a' + (x - 10);
}

/**
 * \brief Convert array of bytes to hexidecimal string
 * \param sbuf output character array
 * \param sbuf_len allocated size of sbuf, must be 2x buf_len
 * \param buf input byte array
 * \param buf_len number of bytes of buf
 * \return 0 on success, -1 on error
 */
static inline int gcrypt_bytes2hex(
		char *sbuf, size_t sbuf_len, unsigned char *buf, size_t buf_len)
{
	size_t i, j;
	if(sbuf_len < 2 * buf_len)
		return -1;
	for(i = 0, j = (2 * buf_len) - 1; i < sbuf_len; i++, j--) {
		sbuf[i] = gcrypt_byte2hex((buf[j / 2] >> (j % 2 ? 0 : 4)) % 0x0f);
		if(j == 0)
			break;
	}
	return 0;
}


/**
 * \brief Increment a counter
 * \param ctr input array of bytes
 * \param len length of byte array
 * \return void
 */
static inline void gcrypt_inc_counter(unsigned char *ctr, size_t len)
{
	size_t i;
	for(i = 0; i < len; i++) {
		ctr[i] += 1;
		if(ctr[i])
			break;
	}
}


#define UUID_LEN 36

/**
 * \brief Convert array of bytes to RFC 4122 UUID (version 4)
 * \param sbuf output character array
 * \param sbuf_len allocated size of sbuf, must be at least 36
 * \param buf input byte array
 * \param buf_len number of bytes of buf, must be at least 16
 * \return 0 on success, -1 on error
 */
static inline int gcrypt_format_rfc4122_uuid(
		char *sbuf, size_t sbuf_len, unsigned char *buf, size_t buf_len)
{
	size_t i, j;
	if(sbuf_len < UUID_LEN)
		return -1;
	if(buf_len < UUID_LEN / 2)
		return -1;
	buf[6] &= 0x0f;
	buf[6] |= 4 << 4;
	buf[8] &= 0x3f;
	buf[8] |= 2 << 6;
	for(i = 0, j = 0; i < UUID_LEN; i++) {
		if(i == 8 || i == 13 || i == 18 || i == 23) {
			sbuf[i] = '-';
			continue;
		}
		sbuf[i] = gcrypt_byte2hex((buf[j / 2] >> (j % 2 ? 0 : 4)) % 0x0f);
		if(!(++j / 2 < buf_len))
			break;
	}
	return 0;
}

#ifndef SHA_DIGEST_LENGTH
#define SHA_DIGEST_LENGTH 20
#endif

/**
 * \brief Get a unique Call-ID
 * \param callid returned Call-ID
 */
void gcrypt_generate_callid(str *callid)
{
#define RAND_BUF_SIZE 16
	static char rand_buf[RAND_BUF_SIZE];
	static unsigned char sha1_buf[SHA_DIGEST_LENGTH] = {0};
	static char uuid_buf[UUID_LEN] = {0};
	gcry_md_hd_t hd;
	gcry_error_t err;
	int lpid;

	if(!gcry_control(GCRYCTL_ANY_INITIALIZATION_P)) {
		/* before calling any other functions */
		gcry_check_version(NULL);
	}

	err = gcry_md_open(&hd, GCRY_MD_SHA1, 0);
	if(err) {
		LM_ERR("cannot get new context\n");
		callid->s = NULL;
		callid->len = 0;
		return;
	}

	gcry_randomize(rand_buf, RAND_BUF_SIZE, GCRY_STRONG_RANDOM);
	gcry_md_write(hd, rand_buf, RAND_BUF_SIZE);
	lpid = my_pid();
	gcry_md_write(hd, &lpid, sizeof(int));
	gcrypt_inc_counter(gcrypt_callid_counter, CTR_LEN);
	gcry_md_write(hd, gcrypt_callid_counter, CTR_LEN);
	memcpy(sha1_buf, gcry_md_read(hd, GCRY_MD_SHA1), SHA_DIGEST_LENGTH);
	gcry_md_close(hd);

	gcrypt_format_rfc4122_uuid(uuid_buf, UUID_LEN, sha1_buf, SHA_DIGEST_LENGTH);

	callid->s = uuid_buf;
	callid->len = UUID_LEN;
}


/**
 *
 */
int gcrypt_register_callid_func(void)
{
	if(sr_register_callid_func(gcrypt_generate_callid) < 0) {
		LM_ERR("unable to register callid func\n");
		return -1;
	}
	return 0;
}
