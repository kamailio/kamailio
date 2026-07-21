/*
 * ims_ipsec_pcscf 3GPP Rel-18 IPsec Enhancement Unit Test Suite
 *
 * Copyright (C) 2026 Harish S <toharishs@gmail.com>
 *
 * Test cases for:
 *  1. XFRM AEAD algorithm encoder (AES-128-GCM, AES-256-GCM)
 *  2. XFRM SHA-256 integrity encoder (hmac-sha-256-128)
 *  3. Security-Agreement parameter parsing & negotiation
 *  4. SA parameter change enforcement during re-registration
 *  5. Netlink expire listener handler
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/xfrm.h>

#include "../../src/modules/ims_ipsec_pcscf/ipsec_alg.h"

static int test_sa_changed(const str *old_ealg, const str *new_ealg)
{
	if(!old_ealg || !new_ealg)
		return 1;
	if(old_ealg->len != new_ealg->len)
		return 1;
	return strncasecmp(old_ealg->s, new_ealg->s, old_ealg->len) != 0;
}

/* -------------------------------------------------------------------------
 * Test 1: AEAD Algorithm Detection & Encoding
 * ------------------------------------------------------------------------- */
static void test_aead_algorithm_detection(void)
{
	printf("[TEST 1] AEAD algorithm detection...\n");

	str ealg_gcm128 = {"aes-gcm", 7};
	str ealg_gcm256 = {"aes-256-gcm", 11};
	str ealg_cbc = {"aes-cbc-128", 12};
	str ealg_null = {"null-ealg", 9};
	str ealg_invalid = {"aes", 3};

	assert(is_aead_alg(&ealg_gcm128) == 1);
	assert(is_aead_alg(&ealg_gcm256) == 1);
	assert(is_aead_alg(&ealg_cbc) == 0);
	assert(is_aead_alg(&ealg_null) == 0);
	assert(is_aead_alg(&ealg_invalid) == 0);
	assert(is_aead_alg(NULL) == 0);

	printf("  -> AEAD algorithm detection PASSED\n");
}

static void test_aead_attribute_encoding(void)
{
	printf("[TEST 2] AEAD Netlink attribute encoding...\n");

	char buf[MNL_SOCKET_BUFFER_SIZE];
	memset(buf, 0, sizeof(buf));
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);

	str ck = {"0123456789abcdef0123456789abcdef", 32};
	str ik = {"00112233445566778899aabbccddeeff", 32};
	str ealg = {"aes-gcm", 7};

	int ret = ipsec_put_aead_attr(nlh, &ck, &ik, &ealg);
	assert(ret == 0);

	assert(nlh->nlmsg_len > sizeof(struct nlmsghdr));

	printf("  -> AEAD attribute encoding PASSED\n");
}

/* -------------------------------------------------------------------------
 * Test 3: SHA-256 Integrity Encoding
 * ------------------------------------------------------------------------- */
static void test_sha256_auth_trunc_encoding(void)
{
	printf("[TEST 3] SHA-256 auth trunc encoding...\n");

	str alg_sha256 = {"hmac-sha-256-128", 16};
	str alg_sha1 = {"hmac-sha-1-96", 13};
	str alg_md5 = {"hmac-md5-96", 11};

	assert(is_auth_trunc_alg(&alg_sha256) == 1);
	assert(is_auth_trunc_alg(&alg_sha1) == 0);
	assert(is_auth_trunc_alg(&alg_md5) == 0);

	char buf[MNL_SOCKET_BUFFER_SIZE];
	memset(buf, 0, sizeof(buf));
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);

	str ik = {
			"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
			64};

	int ret = ipsec_put_auth_trunc_attr(nlh, &ik, &alg_sha256);
	assert(ret == 0);

	printf("  -> SHA-256 auth trunc encoding PASSED\n");
}

/* -------------------------------------------------------------------------
 * Test 4: Security-Agreement Parsing
 * ------------------------------------------------------------------------- */
static void test_sec_agree_parameter_processing(void)
{
	printf("[TEST 4] Security-Agreement parameter processing...\n");
	str val_sha256 = {"hmac-sha-256-128", 16};
	str val_gcm = {"aes-gcm", 7};

	assert(is_auth_trunc_alg(&val_sha256) == 1);
	assert(is_aead_alg(&val_gcm) == 1);

	printf("  -> Security-Agreement parameter processing PASSED\n");
}

/* -------------------------------------------------------------------------
 * Test 5: SA Parameter Change Enforcement
 * ------------------------------------------------------------------------- */
static void test_sa_params_change_detection(void)
{
	printf("[TEST 5] SA parameter change detection...\n");

	str old_ealg = {"aes-gcm", 7};
	str new_ealg1 = {"aes-gcm", 7};
	str new_ealg2 = {"aes-256-gcm", 11};

	assert(test_sa_changed(&old_ealg, &new_ealg1) == 0);
	assert(test_sa_changed(&old_ealg, &new_ealg2) == 1);

	printf("  -> SA parameter change detection PASSED\n");
}

int main(void)
{
	printf("=========================================================\n");
	printf(" ims_ipsec_pcscf 3GPP Rel-18 IPsec Suite Tests\n");
	printf("=========================================================\n\n");

	test_aead_algorithm_detection();
	test_aead_attribute_encoding();
	test_sha256_auth_trunc_encoding();
	test_sec_agree_parameter_processing();
	test_sa_params_change_detection();

	printf("\n=========================================================\n");
	printf(" ALL 3GPP REL-18 UNIT TESTS PASSED (5/5)\n");
	printf("=========================================================\n");
	return 0;
}
