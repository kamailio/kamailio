/*
 * IMS IPSEC PCSCF module - 3GPP Rel-18 IPsec Netlink algorithm attribute encoder
 *
 * Copyright (C) 2026 Harish S <toharishs@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ipsec_alg.h"
#ifndef _IPSEC_SPI_LIST_TEST
#include "../../core/dprint.h"
#else
#include <stdio.h>
#define LM_ERR(...) fprintf(stderr, __VA_ARGS__)
#endif
#include <string.h>
#include <strings.h>
#include <stdlib.h>

int is_aead_alg(const str *ealg)
{
	if(!ealg || !ealg->s)
		return 0;
	if(ealg->len == 7 && strncasecmp(ealg->s, "aes-gcm", 7) == 0)
		return 1;
	if(ealg->len == 11 && strncasecmp(ealg->s, "aes-256-gcm", 11) == 0)
		return 1;
	return 0;
}

int is_auth_trunc_alg(const str *alg)
{
	if(!alg || !alg->s)
		return 0;
	if(alg->len == 16 && strncasecmp(alg->s, "hmac-sha-256-128", 16) == 0)
		return 1;
	return 0;
}

static int hex2byte(const str *hex, unsigned char *byte, int byte_len)
{
	if(!hex || !hex->s || !byte)
		return -1;
	if(hex->len < byte_len * 2)
		return -1;
	int i;
	for(i = 0; i < byte_len; i++) {
		char b[3];
		b[0] = hex->s[i * 2];
		b[1] = hex->s[i * 2 + 1];
		b[2] = '\0';
		byte[i] = (unsigned char)strtol(b, NULL, 16);
	}
	return 0;
}

int ipsec_put_aead_attr(
		struct nlmsghdr *nlh, const str *ck, const str *ik, const str *ealg)
{
	if(!nlh || !ck || !ealg || !ck->s || !ealg->s)
		return -1;
	if(!is_aead_alg(ealg))
		return -1;

	char aead_buf[512];
	memset(aead_buf, 0, sizeof(aead_buf));
	struct xfrm_algo_aead *aead_algo = (struct xfrm_algo_aead *)aead_buf;

	strcpy(aead_algo->alg_name, "rfc4106(gcm(aes))");
	aead_algo->alg_icv_len = 128; // 16 bytes ICV tag

	int key_bytes = 16;
	if(ealg->len == 11 && strncasecmp(ealg->s, "aes-256-gcm", 11) == 0) {
		key_bytes = 32;
	}

	aead_algo->alg_key_len = key_bytes * 8; // Bit length excluding salt

	int total_key_bytes = key_bytes + 4; // Key + 4-byte salt
	if(sizeof(struct xfrm_algo_aead) + total_key_bytes > sizeof(aead_buf)) {
		LM_ERR("AEAD buffer overflow\n");
		return -1;
	}

	if(ck->len >= total_key_bytes * 2) {
		if(hex2byte(ck, (unsigned char *)aead_algo->alg_key, total_key_bytes)
				< 0) {
			LM_ERR("Failed to decode combined key/salt from ck\n");
			return -1;
		}
	} else {
		if(hex2byte(ck, (unsigned char *)aead_algo->alg_key, key_bytes) < 0) {
			LM_ERR("Failed to decode AEAD key from ck\n");
			return -1;
		}
		if(ik && ik->s && ik->len >= 8) {
			if(hex2byte(ik, (unsigned char *)aead_algo->alg_key + key_bytes, 4)
					< 0) {
				memset(aead_algo->alg_key + key_bytes, 0xA5, 4);
			}
		} else {
			memset(aead_algo->alg_key + key_bytes, 0xA5, 4);
		}
	}

	mnl_attr_put(nlh, XFRMA_ALG_AEAD,
			sizeof(struct xfrm_algo_aead) + total_key_bytes, aead_algo);
	return 0;
}

int ipsec_put_auth_trunc_attr(
		struct nlmsghdr *nlh, const str *ik, const str *alg)
{
	if(!nlh || !ik || !alg || !ik->s || !alg->s)
		return -1;

	char auth_buf[512];
	memset(auth_buf, 0, sizeof(auth_buf));
	struct xfrm_algo_auth *auth_algo = (struct xfrm_algo_auth *)auth_buf;

	int key_bytes = 0;
	if(alg->len == 16 && strncasecmp(alg->s, "hmac-sha-256-128", 16) == 0) {
		strcpy(auth_algo->alg_name, "sha256");
		auth_algo->alg_trunc_len = 128;
		key_bytes = 32;
	} else if(alg->len == 13 && strncasecmp(alg->s, "hmac-sha-1-96", 13) == 0) {
		strcpy(auth_algo->alg_name, "sha1");
		auth_algo->alg_trunc_len = 96;
		key_bytes = 20;
	} else {
		LM_ERR("Unsupported auth trunc algorithm: %.*s\n", alg->len, alg->s);
		return -1;
	}

	if(sizeof(struct xfrm_algo_auth) + key_bytes > sizeof(auth_buf)) {
		LM_ERR("Auth trunc buffer overflow\n");
		return -1;
	}

	auth_algo->alg_key_len = key_bytes * 8;
	if(hex2byte(ik, (unsigned char *)auth_algo->alg_key, key_bytes) < 0) {
		LM_ERR("Failed to decode auth trunc key from ik\n");
		return -1;
	}

	mnl_attr_put(nlh, XFRMA_ALG_AUTH_TRUNC,
			sizeof(struct xfrm_algo_auth) + key_bytes, auth_algo);
	return 0;
}
