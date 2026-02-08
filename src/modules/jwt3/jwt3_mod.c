/**
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2026 Wolfgang Kampichler (Frequentis AG)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>

#include <jwt.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/trim.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_param.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_jwt_generate_4(
		sip_msg_t *msg, char *pkey, char *palg, char *pclaims, char *pheaders);
static int w_jwt_generate_3(
		sip_msg_t *msg, char *pkey, char *palg, char *pclaims);
static int w_jwt_verify(sip_msg_t *msg, char *pkeypath, char *palg,
		char *pclaims, char *pjwtval);
static int w_jwt_verify_key(
		sip_msg_t *msg, char *pkey, char *palg, char *pclaims, char *pjwtval);

static int _jwt_leeway_sec = -1;

static str _jwt_result = STR_NULL;
static unsigned int _jwt_verify_status = 0;

/* clang-format off */
static cmd_export_t cmds[] = {
	{"jwt3_generate", (cmd_function)w_jwt_generate_4, 4, fixup_spve_all, fixup_free_spve_all,
			ANY_ROUTE},
	{"jwt3_generate", (cmd_function)w_jwt_generate_3, 3, fixup_spve_all, fixup_free_spve_all,
			ANY_ROUTE},
	{"jwt3_verify", (cmd_function)w_jwt_verify, 4, fixup_spve_all, fixup_free_spve_all,
			ANY_ROUTE},
	{"jwt3_verify_key", (cmd_function)w_jwt_verify_key, 4, fixup_spve_all, fixup_free_spve_all,
			ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"leeway_sec", PARAM_INT, &_jwt_leeway_sec},
	{0, 0, 0}
};

static int jwt_pv_get(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int jwt_pv_parse_name(pv_spec_t *sp, str *in);
static pv_export_t mod_pvs[] = {
	{{"jwt3", sizeof("jwt3") - 1}, PVT_OTHER, jwt_pv_get, 0,
			jwt_pv_parse_name, 0, 0, 0},
	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"jwt3",			 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* cmd (cfg function) exports */
	params,			 /* param exports */
	0,				 /* RPC method exports */
	mod_pvs,		 /* pseudo-variables exports */
	0,				 /* response handling function */
	mod_init,		 /* module init function */
	child_init,		 /* per-child init function */
	mod_destroy		 /* module destroy function */
};
/* clang-format on */

/**
 * @brief Destroy JWT string
 */
static void jwt_free_str(char *s)
{
	if(s)
		free(s);
}

/**
 * @brief Initialize crypto module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize crypto module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * @brief Destroy module function
 */
static void mod_destroy(void)
{
	if(_jwt_result.s != NULL) {
		jwt_free_str(_jwt_result.s);
		_jwt_result.s = NULL;
		_jwt_result.len = 0;
	}
	return;
}

/**
 * @brief Encode URL base64
 */
static char *jwt_base64url_encode(const unsigned char *input, int length)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;
	char *buff;
	int i, j;

	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	buff = (char *)pkg_malloc(bptr->length + 1);
	if(!buff) {
		BIO_free_all(b64);
		return NULL;
	}

	memcpy(buff, bptr->data, bptr->length);
	buff[bptr->length] = 0;
	BIO_free_all(b64);

	for(i = 0, j = 0; buff[i]; i++) {
		if(buff[i] == '+')
			buff[j++] = '-';
		else if(buff[i] == '/')
			buff[j++] = '_';
		else if(buff[i] != '=')
			buff[j++] = buff[i];
	}
	buff[j] = '\0';
	return buff;
}

/**
 * @brief Convert big number to binary
 */
static int jwt_bn_to_bin(const BIGNUM *bn, unsigned char *out, int size)
{
	int num_bytes = BN_num_bytes(bn);
	if(num_bytes > size)
		return -1;
	memset(out, 0, size);
	BN_bn2bin(bn, out + (size - num_bytes));
	return 0;
}

/**
 * @brief Convert PEM to JWKS (openssl 3.0 compliant)
 */
static char *jwt_pkey_to_jwks(EVP_PKEY *pkey, const char *kid)
{
	BIGNUM *x_bn = NULL, *y_bn = NULL, *d_bn = NULL;
	unsigned char *bin_x = NULL, *bin_y = NULL, *bin_d = NULL;
	char *b64_x = NULL, *b64_y = NULL, *b64_d = NULL;
	char *json_out = NULL;
	int order_len = 0;
	int is_private = 0;

	/* check if it is an EC Key */
	if(!EVP_PKEY_is_a(pkey, "EC")) {
		LM_ERR("Key is not an Elliptic Curve Key\n");
		return NULL;
	}

	/* extract Public Key X, Y */
	if(!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_EC_PUB_X, &x_bn)
			|| !EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_EC_PUB_Y, &y_bn)) {
		LM_ERR("Failed to extract Public Key coordinates (X, Y)\n");
		goto cleanup;
	}

	/* try to extract Private Key D */
	if(EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &d_bn)) {
		is_private = 1;
	}

	/* calculate sizes */
	int bits = EVP_PKEY_get_bits(pkey);
	order_len = (bits + 7) / 8;

	bin_x = pkg_malloc(order_len);
	bin_y = pkg_malloc(order_len);
	if(is_private)
		bin_d = pkg_malloc(order_len);

	if(!bin_x || !bin_y || (is_private && !bin_d)) {
		LM_ERR("No memory for binary key conversion\n");
		goto cleanup;
	}

	jwt_bn_to_bin(x_bn, bin_x, order_len);
	jwt_bn_to_bin(y_bn, bin_y, order_len);
	if(is_private)
		jwt_bn_to_bin(d_bn, bin_d, order_len);

	b64_x = jwt_base64url_encode(bin_x, order_len);
	b64_y = jwt_base64url_encode(bin_y, order_len);
	if(is_private)
		b64_d = jwt_base64url_encode(bin_d, order_len);

	if(!b64_x || !b64_y || (is_private && !b64_d)) {
		LM_ERR("Base64 encoding failed\n");
		goto cleanup;
	}

	int len = 1024 + strlen(b64_x) + strlen(b64_y)
			  + (is_private ? strlen(b64_d) : 0);
	json_out = pkg_malloc(len);

	if(!json_out) {
		LM_ERR("No memory for JSON string\n");
		goto cleanup;
	}

	if(is_private) {
		snprintf(json_out, len,
				"{\"keys\":[{\"kty\":\"EC\",\"crv\":\"P-256\",\"use\":\"sig\","
				"\"kid\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"d\":\"%s\"}]}",
				kid, b64_x, b64_y, b64_d);
	} else {
		snprintf(json_out, len,
				"{\"keys\":[{\"kty\":\"EC\",\"crv\":\"P-256\",\"use\":\"sig\","
				"\"kid\":\"%s\",\"x\":\"%s\",\"y\":\"%s\"}]}",
				kid, b64_x, b64_y);
	}

cleanup:
	BN_free(x_bn);
	BN_free(y_bn);
	BN_free(d_bn);
	if(bin_x)
		pkg_free(bin_x);
	if(bin_y)
		pkg_free(bin_y);
	if(bin_d)
		pkg_free(bin_d);
	if(b64_x)
		pkg_free(b64_x);
	if(b64_y)
		pkg_free(b64_y);
	if(b64_d)
		pkg_free(b64_d);
	return json_out;
}

/**
 * @brief Load PEM from file and convert to JWKS
 */
static char *jwt_pem_to_jwks(const char *pem_path, const char *kid)
{
	FILE *fp = fopen(pem_path, "r");
	EVP_PKEY *pkey = NULL;
	char *json = NULL;

	if(!fp) {
		LM_ERR("Cannot open PEM file: %s\n", pem_path);
		return NULL;
	}

	pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
	if(!pkey) {
		rewind(fp);
		pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
	}
	fclose(fp);

	if(!pkey) {
		LM_ERR("Failed to parse PEM file: %s\n", pem_path);
		return NULL;
	}

	json = jwt_pkey_to_jwks(pkey, kid);
	EVP_PKEY_free(pkey);
	return json;
}

/**
 * @brief Convert raw PEM string to JWKS
 */
static char *jwt_raw_to_jwks(const char *pem_data, int pem_len, const char *kid)
{
	/* create a read-only memory BIO from the string */
	BIO *bio = BIO_new_mem_buf(pem_data, pem_len);
	EVP_PKEY *pkey = NULL;
	char *json = NULL;

	if(!bio) {
		LM_ERR("Failed to create memory BIO\n");
		return NULL;
	}

	/* try reading as Private Key */
	pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	if(!pkey) {
		/* on failure, reset BIO and try Public Key */
		BIO_reset(bio);
		pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
	}

	BIO_free(bio);

	if(!pkey) {
		LM_ERR("Failed to parse Raw PEM string\n");
		return NULL;
	}

	json = jwt_pkey_to_jwks(pkey, kid);
	EVP_PKEY_free(pkey);
	return json;
}

/**
 * @brief Load key based on string content
 */
static jwk_set_t *jwt_load_keys(str *key_in)
{
	char *dot = strchr(key_in->s, '.');
	char *dot_last = strrchr(key_in->s, '.');
	jwk_set_t *jwks = NULL;

	if(!dot) {
		/* raw PEM (content) */
		LM_DBG("No dot found. Treating as Raw PEM content.\n");
		char *json = jwt_raw_to_jwks(key_in->s, key_in->len, "legacy-raw");
		if(json) {
			jwks = jwks_load(NULL, json);
			pkg_free(json);
		}
	} else if(dot_last && strcmp(dot_last, ".json") == 0) {
		/* native JWKS (file) */
		LM_DBG("Found .json extension. Treating as Native JWKS file.\n");
		jwks = jwks_load_fromfile(NULL, key_in->s);
	} else {
		/* legacy PEM (file) */
		LM_DBG("Found dot but not .json. Treating as Legacy PEM file.\n");
		char *json = jwt_pem_to_jwks(key_in->s, "legacy-file");
		if(json) {
			jwks = jwks_load(NULL, json);
			pkg_free(json);
		}
	}

	return jwks;
}

/**
 *
 */
static int ki_jwt_generate_hdrs(
		sip_msg_t *msg, str *key, str *alg, str *claims, str *headers)
{
	str dupclaims = STR_NULL;
	str dupheaders = STR_NULL;
	str sparams = STR_NULL;
	str sheaders = STR_NULL;
	jwt_alg_t valg = JWT_ALG_NONE;

	param_t *params_list = NULL;
	param_t *headers_list = NULL;
	param_hooks_t phooks;
	param_t *curr = NULL;

	jwt_builder_t *builder = NULL;
	jwk_set_t *jwks = NULL;
	jwt_value_t val;

	char *out = NULL;
	long lval = 0;

	(void)msg;

	if(!key || !key->s || !alg || !claims) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(_jwt_result.s != NULL) {
		jwt_free_str(_jwt_result.s);
		_jwt_result.s = NULL;
		_jwt_result.len = 0;
	}

	valg = jwt_str_alg(alg->s);
	if(valg == JWT_ALG_INVAL) {
		LM_ERR("not supported algorithm: %s\n", alg->s);
		return -1;
	}

	/* parse claims */
	if(pkg_str_dup(&dupclaims, claims) < 0)
		return -1;
	sparams = dupclaims;
	if(sparams.len > 0 && sparams.s[sparams.len - 1] == ';')
		sparams.len--;
	if(parse_params(&sparams, CLASS_ANY, &phooks, &params_list) < 0) {
		LM_ERR("failed to parse claims\n");
		goto error;
	}

	/* parse headers */
	if(headers && headers->len > 0) {
		if(pkg_str_dup(&dupheaders, headers) < 0)
			goto error;
		sheaders = dupheaders;
		if(sheaders.len > 0 && sheaders.s[sheaders.len - 1] == ';')
			sheaders.len--;
		if(parse_params(&sheaders, CLASS_ANY, &phooks, &headers_list) < 0) {
			LM_ERR("failed to parse headers\n");
			goto error;
		}
	}

	/* init builder */
	builder = jwt_builder_new();
	if(!builder) {
		LM_ERR("failed to create jwt builder\n");
		goto error;
	}

	/* load keys - backwards compatible */
	jwks = jwt_load_keys(key);

	if(!jwks || jwks_error(jwks)) {
		LM_ERR("failed to load key from: %s\n", key->s);
		goto error;
	}
	const jwk_item_t *item = jwks_item_get(jwks, 0);
	if(!item) {
		LM_ERR("jwks empty\n");
		goto error;
	}

	if(jwt_builder_setkey(builder, valg, item) != 0) {
		LM_ERR("Failed to set key: %s (%d)\n", jwt_builder_error_msg(builder),
				jwt_builder_error(builder));
	}

	/* set claims */
	for(curr = params_list; curr; curr = curr->next) {
		if(curr->name.len <= 0 || curr->body.len <= 0)
			continue;

		memset(&val, 0, sizeof(val));
		curr->name.s[curr->name.len] = '\0';
		curr->body.s[curr->body.len] = '\0';

		if(curr->body.s[curr->body.len - 1] == '\"'
				|| curr->body.s[curr->body.len - 1] == '\'') {
			/* quoted -> string */
			jwt_set_SET_STR(&val, curr->name.s, curr->body.s);
		} else if(str2slong(&curr->body, &lval) == 0) {
			/* number -> int */
			jwt_set_SET_INT(&val, curr->name.s, lval);
		} else {
			/* default -> string */
			jwt_set_SET_STR(&val, curr->name.s, curr->body.s);
		}

		if(jwt_builder_claim_set(builder, &val) != 0) {
			LM_ERR("failed to set claim: %s\n", curr->name.s);
			goto error;
		}
	}

	/* set headers */
	for(curr = headers_list; curr; curr = curr->next) {
		if(curr->name.len <= 0 || curr->body.len <= 0)
			continue;

		memset(&val, 0, sizeof(val));
		curr->name.s[curr->name.len] = '\0';
		curr->body.s[curr->body.len] = '\0';

		if(curr->body.s[curr->body.len - 1] == '\"'
				|| curr->body.s[curr->body.len - 1] == '\'') {
			/* quoted -> string */
			jwt_set_SET_STR(&val, curr->name.s, curr->body.s);
		} else if(str2slong(&curr->body, &lval) == 0) {
			/* number -> int */
			jwt_set_SET_INT(&val, curr->name.s, lval);
		} else {
			/* quoted -> string */
			jwt_set_SET_STR(&val, curr->name.s, curr->body.s);
		}

		if(jwt_builder_header_set(builder, &val) != 0) {
			LM_ERR("failed to set header: %s\n", curr->name.s);
			goto error;
		}
	}

	/* generate token */
	out = jwt_builder_generate(builder);
	if(!out) {
		LM_ERR("jwt generation failed\n");
		goto error;
	}

	_jwt_result.s = out;
	_jwt_result.len = strlen(out);

	/* clean up */
	jwks_free(jwks);
	jwt_builder_free(builder);
	free_params(params_list);
	free_params(headers_list);
	pkg_free(dupclaims.s);
	pkg_free(dupheaders.s);
	return 1;

error:
	if(jwks)
		jwks_free(jwks);
	if(builder)
		jwt_builder_free(builder);
	if(params_list)
		free_params(params_list);
	if(headers_list)
		free_params(headers_list);
	if(dupclaims.s)
		pkg_free(dupclaims.s);
	if(dupheaders.s)
		pkg_free(dupheaders.s);
	return -1;
}

/**
 *
 */
static int ki_jwt_generate(sip_msg_t *msg, str *key, str *alg, str *claims)
{
	return ki_jwt_generate_hdrs(msg, key, alg, claims, NULL);
}

/**
 *
 */
static int w_jwt_generate_3(
		sip_msg_t *msg, char *pkey, char *palg, char *pclaims)
{
	str skey = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pkey, &skey) != 0) {
		LM_ERR("cannot get path to the key file\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}

	return ki_jwt_generate(msg, &skey, &salg, &sclaims);
}

/**
 *
 */
static int w_jwt_generate_4(
		sip_msg_t *msg, char *pkey, char *palg, char *pclaims, char *pheaders)
{
	str skey = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;
	str sheaders = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pkey, &skey) != 0) {
		LM_ERR("cannot get path to the key file\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pheaders, &sheaders) != 0) {
		LM_ERR("cannot get headers value\n");
		return -1;
	}

	return ki_jwt_generate_hdrs(msg, &skey, &salg, &sclaims, &sheaders);
}

/**
 *
 */
static int ki_jwt_check_cb(jwt_t *jwt, void *cfg)
{
	jwt_config_t *c = (jwt_config_t *)cfg;
	param_t *params_list = (param_t *)c->ctx;
	param_t *pit;
	jwt_value_t val;

	char *endptr;
	int ret = 0;

	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len <= 0 || pit->body.len <= 0)
			continue;

		/* skip standard claims handled by the checker core */
		if(strcmp(pit->name.s, "iss") == 0 || strcmp(pit->name.s, "sub") == 0
				|| strcmp(pit->name.s, "aud") == 0
				|| strcmp(pit->name.s, "exp") == 0
				|| strcmp(pit->name.s, "nbf") == 0) {
			continue;
		}

		pit->name.s[pit->name.len] = '\0';
		pit->body.s[pit->body.len] = '\0';

		/* check custom claims */
		long long expected_int = strtoll(pit->body.s, &endptr, 10);
		memset(&val, 0, sizeof(val));

		if(*endptr == '\0') {
			/* int check */
			jwt_set_GET_INT(&val, pit->name.s);
			if(jwt_claim_get(jwt, &val) != JWT_VALUE_ERR_NONE) {
				LM_ERR("Missing claim (int): %s\n", pit->name.s);
				return JWT_VALUE_ERR_EXIST;
			}
			if(val.type != JWT_VALUE_INT || val.int_val != expected_int)
				ret = -1;
		} else {
			/* string check */
			jwt_set_GET_STR(&val, pit->name.s);
			if(jwt_claim_get(jwt, &val) != JWT_VALUE_ERR_NONE) {
				LM_ERR("Missing claim (str): %s\n", pit->name.s);
				return JWT_VALUE_ERR_EXIST;
			}
			const char *s =
					(val.type == JWT_VALUE_STR) ? val.str_val : val.json_val;
			if(s == NULL || strcmp(s, pit->body.s) != 0)
				ret = -1;
		}

		if(ret != 0)
			return JWT_VALUE_ERR_EXIST;
	}
	return JWT_VALUE_ERR_NONE;
}

static int ki_jwt_verify_key(
		sip_msg_t *msg, str *key, str *alg, str *claims, str *jwtval)
{
	str dupclaims = STR_NULL;
	jwt_alg_t valg = JWT_ALG_NONE;
	str kdata = STR_NULL;
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	int ret = JWT_VALUE_ERR_EXIST;
	jwt_checker_t *checker = NULL;
	jwk_set_t *jwks = NULL;
	str sparams = STR_NULL;
	(void)msg;

	_jwt_verify_status = 0;

	if(!key || !key->s || !alg || !claims || !jwtval)
		return -1;

	valg = jwt_str_alg(alg->s);
	if(pkg_str_dup(&dupclaims, claims) < 0)
		return -1;
	sparams = dupclaims;
	if(sparams.len > 0 && sparams.s[sparams.len - 1] == ';')
		sparams.len--;
	if(parse_params(&sparams, CLASS_ANY, &phooks, &params_list) < 0) {
		pkg_free(dupclaims.s);
		return -1;
	}

	checker = jwt_checker_new();
	if(!checker) {
		LM_ERR("failed to create jwt checker\n");
		goto error;
	}

	jwks = jwt_load_keys(key);

	if(jwks && !jwks_error(jwks)) {
		const jwk_item_t *item = jwks_item_get(jwks, 0);
		if(item) {
			jwt_checker_time_leeway(checker, JWT_CLAIM_EXP, _jwt_leeway_sec);
			jwt_checker_time_leeway(checker, JWT_CLAIM_NBF, _jwt_leeway_sec);
			jwt_checker_setkey(checker, valg, item);

			for(pit = params_list; pit; pit = pit->next) {
				jwt_claims_t cid = 0;
				if(strcmp(pit->name.s, "iss") == 0)
					cid = JWT_CLAIM_ISS;
				else if(strcmp(pit->name.s, "sub") == 0)
					cid = JWT_CLAIM_SUB;
				else if(strcmp(pit->name.s, "aud") == 0)
					cid = JWT_CLAIM_AUD;
				else if(strcmp(pit->name.s, "exp") == 0)
					cid = JWT_CLAIM_EXP;
				else if(strcmp(pit->name.s, "nbf") == 0)
					cid = JWT_CLAIM_NBF;
				if(cid > 0)
					jwt_checker_claim_set(checker, cid, pit->body.s);
			}

			kdata = *jwtval;
			trim(&kdata);

			jwt_checker_setcb(
					checker, (jwt_callback_t)ki_jwt_check_cb, params_list);
			ret = jwt_checker_verify(checker, kdata.s);
			_jwt_verify_status = ret;
			if(ret != 0) {
				LM_ERR("failed to validate jwt: %s (%d)\n",
						jwt_checker_error_msg(checker),
						jwt_checker_error(checker));
			}
		}
	}

	if(jwks)
		jwks_free(jwks);

	free_params(params_list);
	pkg_free(dupclaims.s);
	jwt_checker_free(checker);
	return (ret == 0) ? 1 : -1;

error:
	if(params_list != NULL) {
		free_params(params_list);
	}
	if(dupclaims.s != NULL) {
		pkg_free(dupclaims.s);
	}
	if(jwks != NULL) {
		jwks_free(jwks);
	}
	if(checker != NULL) {
		jwt_checker_free(checker);
	}
	return -1;
}

/**
 *
 */
static int ki_jwt_verify(
		sip_msg_t *msg, str *keypath, str *alg, str *claims, str *jwtval)
{
	if(keypath == NULL || keypath->s == NULL || alg == NULL || alg->s == NULL
			|| claims == NULL || claims->s == NULL || claims->len <= 0
			|| jwtval == NULL || jwtval->s == NULL || jwtval->len <= 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	return ki_jwt_verify_key(msg, keypath, alg, claims, jwtval);
}

/**
 *
 */
static int w_jwt_verify(sip_msg_t *msg, char *pkeypath, char *palg,
		char *pclaims, char *pjwtval)
{
	str skeypath = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;
	str sjwtval = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pkeypath, &skeypath) != 0) {
		LM_ERR("cannot get path to the key file\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pjwtval, &sjwtval) != 0) {
		LM_ERR("cannot get jwt value\n");
		return -1;
	}

	return ki_jwt_verify(msg, &skeypath, &salg, &sclaims, &sjwtval);
}

/**
 *
 */
static int w_jwt_verify_key(
		sip_msg_t *msg, char *pkey, char *palg, char *pclaims, char *pjwtval)
{
	str skey = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;
	str sjwtval = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pkey, &skey) != 0) {
		LM_ERR("cannot get the key value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pjwtval, &sjwtval) != 0) {
		LM_ERR("cannot get jwt value\n");
		return -1;
	}

	return ki_jwt_verify_key(msg, &skey, &salg, &sclaims, &sjwtval);
}

/**
 *
 */
static int jwt_pv_get(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n) {
		case 0:
			if(_jwt_result.s == NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_jwt_result);
		case 1:
			return pv_get_uintval(msg, param, res, _jwt_verify_status);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int jwt_pv_parse_name(pv_spec_t *sp, str *in)
{
	if(in->len == 3 && strncmp(in->s, "val", 3) == 0) {
		sp->pvp.pvn.u.isname.name.n = 0;
	} else if(in->len == 6 && strncmp(in->s, "status", 6) == 0) {
		sp->pvp.pvn.u.isname.name.n = 1;
	} else {
		LM_ERR("unknown inner name [%.*s]\n", in->len, in->s);
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_jwt_exports[] = {
	{ str_init("jwt3"), str_init("jwt3_generate"),
		SR_KEMIP_INT, ki_jwt_generate,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jwt3"), str_init("jwt3_generate_hdrs"),
		SR_KEMIP_INT, ki_jwt_generate_hdrs,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jwt3"), str_init("jwt3_verify"),
		SR_KEMIP_INT, ki_jwt_verify,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jwt3"), str_init("jwt3_verify_key"),
		SR_KEMIP_INT, ki_jwt_verify_key,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_jwt_exports);
	return 0;
}
