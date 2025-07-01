/**
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
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

#include <gcrypt.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/pvapi.h"
#include "../../core/lvalue.h"
#include "../../core/basex.h"
#include "../../core/kemi.h"

#include "gcrypt_uuid.h"
#include "gcrypt_aes128.h"
#include "api.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_gcrypt_aes_encrypt(
		sip_msg_t *msg, char *inb, char *keyb, char *outb);
static int w_gcrypt_aes_decrypt(
		sip_msg_t *msg, char *inb, char *keyb, char *outb);

/* init vector value */
static str _gcrypt_init_vector = str_init("SIP/2.0 is RFC3261");
static int _gcrypt_register_callid = 0;
static int _gcrypt_aes_mode_param = 0;
static int _gcrypt_aes_mode = GCRY_CIPHER_MODE_ECB;

int bind_gcrypt(gcrypt_api_t *api);

/* clang-format off */
static cmd_export_t cmds[] = {
	{"gcrypt_aes_encrypt", (cmd_function)w_gcrypt_aes_encrypt, 3,
			fixup_spve2_pvar, fixup_free_spve2_pvar, ANY_ROUTE},
	{"gcrypt_aes_decrypt", (cmd_function)w_gcrypt_aes_decrypt, 3,
			fixup_spve2_pvar, fixup_free_spve2_pvar, ANY_ROUTE},
	{"bind_gcrypt", (cmd_function)bind_gcrypt, 0, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"init_vector", PARAM_STR, &_gcrypt_init_vector},
	{"register_callid", PARAM_INT, &_gcrypt_register_callid},
	{"aes_mode", PARAM_INT, &_gcrypt_aes_mode_param},

	{0, 0, 0}
};

struct module_exports exports = {
	"gcrypt",		 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* cmd (cfg function) exports */
	params,			 /* param exports */
	0,				 /* RPC method exports */
	0,				 /* pseudo-variables exports */
	0,				 /* response handling function */
	mod_init,		 /* module init function */
	child_init,		 /* per-child init function */
	mod_destroy		 /* module destroy function */
};
/* clang-format on */

/**
 * @brief Initialize crypto module function
 */
static int mod_init(void)
{
	if(_gcrypt_init_vector.len < 16) {
		LM_ERR("init vector value has to be longer\n");
		return -1;
	}
	if(_gcrypt_register_callid != 0) {
		if(gcrypt_register_callid_func() < 0) {
			LM_ERR("unable to register callid callback\n");
			return -1;
		}
		LM_DBG("registered crypto callid callback\n");
	}
	if(_gcrypt_aes_mode_param == 1) {
		_gcrypt_aes_mode = GCRY_CIPHER_MODE_CBC;
	}

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
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
static int ki_gcrypt_aes_encrypt_helper(
		sip_msg_t *msg, str *ins, str *keys, pv_spec_t *dst)
{
	pv_value_t val;
	gcry_error_t gcry_ret;
	gcry_cipher_hd_t cipher_hd;
	size_t key_length = 0;
	size_t blk_length = 0;
	char *encypted_txt = NULL;

	if(!gcry_control(GCRYCTL_ANY_INITIALIZATION_P)) {
		/* before calling any other functions */
		gcry_check_version(NULL);
	}

	gcry_ret = gcry_cipher_open(&cipher_hd, // gcry_cipher_hd_t *hd
			GCRY_CIPHER_AES256,				// int algo
			_gcrypt_aes_mode,				// int mode
			0);								// unsigned int flags
	if(gcry_ret) {
		LM_ERR("gcry cipher open failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		return -1;
	}
	key_length = gcry_cipher_get_algo_keylen(GCRY_CIPHER_AES256);
	if(keys->len < key_length) {
		LM_ERR("the encryption key is too short\n");
		goto error;
	}
	gcry_ret = gcry_cipher_setkey(cipher_hd, keys->s, key_length);
	if(gcry_ret) {
		LM_ERR("gcry_cipher_setkey failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		goto error;
	}

	blk_length = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES256);
	if(_gcrypt_init_vector.len < blk_length) {
		LM_ERR("the init vector is too short\n");
		goto error;
	}

	gcry_ret = gcry_cipher_setiv(cipher_hd, _gcrypt_init_vector.s, blk_length);
	if(gcry_ret) {
		LM_ERR("gcry_cipher_setiv failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		goto error;
	}

	encypted_txt = malloc(ins->len + 1);
	if(encypted_txt == 0) {
		SYS_MEM_ERROR;
		goto error;
	}
	encypted_txt[0] = '\0';
	gcry_ret = gcry_cipher_encrypt(cipher_hd, // gcry_cipher_hd_t h
			encypted_txt,					  // unsigned char *out
			ins->len,						  // size_t outsize
			ins->s,							  // const unsigned char *in
			ins->len);						  // size_t inlen
	if(gcry_ret) {
		LM_ERR("gcry_cipher_encrypt failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		goto error;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.rs.s = pv_get_buffer();
	val.rs.len = base64_enc((unsigned char *)encypted_txt, ins->len,
			(unsigned char *)val.rs.s, pv_get_buffer_size() - 1);
	if(val.rs.len < 0) {
		LM_ERR("base64 output of encrypted value is too large (need %d)\n",
				-val.rs.len);
		goto error;
	}
	LM_DBG("base64 encrypted result: [%.*s]\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	free(encypted_txt);
	gcry_cipher_close(cipher_hd);
	return 1;

error:
	if(encypted_txt) {
		free(encypted_txt);
	}
	gcry_cipher_close(cipher_hd);
	return -1;
}

/**
 *
 */
static int ki_gcrypt_aes_encrypt(sip_msg_t *msg, str *ins, str *keys, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);

	if(dst == NULL) {
		LM_ERR("failed getting pv: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_gcrypt_aes_encrypt_helper(msg, ins, keys, dst);
}

/**
 *
 */
static int w_gcrypt_aes_encrypt(
		sip_msg_t *msg, char *inb, char *keyb, char *outb)
{
	str ins;
	str keys;
	pv_spec_t *dst;

	if(fixup_get_svalue(msg, (gparam_t *)inb, &ins) != 0) {
		LM_ERR("cannot get input value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)keyb, &keys) != 0) {
		LM_ERR("cannot get key value\n");
		return -1;
	}
	dst = (pv_spec_t *)outb;

	return ki_gcrypt_aes_encrypt_helper(msg, &ins, &keys, dst);
}

/**
 *
 */
static int ki_gcrypt_aes_decrypt_helper(
		sip_msg_t *msg, str *ins, str *keys, pv_spec_t *dst)
{
	pv_value_t val;
	gcry_error_t gcry_ret;
	gcry_cipher_hd_t cipher_hd;
	size_t key_length = 0;
	size_t blk_length = 0;
	char *decrypted_txt = NULL;
	str etext = STR_NULL;

	memset(&val, 0, sizeof(pv_value_t));
	etext.s = pv_get_buffer();
	etext.len = base64_dec((unsigned char *)ins->s, ins->len,
			(unsigned char *)etext.s, pv_get_buffer_size() - 1);
	if(etext.len < 0) {
		LM_ERR("base64 input with encrypted value is too large (need %d)\n",
				-etext.len);
		return -1;
	}

	if(!gcry_control(GCRYCTL_ANY_INITIALIZATION_P)) {
		/* before calling any other functions */
		gcry_check_version(NULL);
	}

	gcry_ret = gcry_cipher_open(&cipher_hd, // gcry_cipher_hd_t *hd
			GCRY_CIPHER_AES256,				// int algo
			_gcrypt_aes_mode,				// int mode
			0);								// unsigned int flags
	if(gcry_ret) {
		LM_ERR("gcry cipher open failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		return -1;
	}
	key_length = gcry_cipher_get_algo_keylen(GCRY_CIPHER_AES256);
	if(keys->len < key_length) {
		LM_ERR("the encryption key is too short\n");
		goto error;
	}
	gcry_ret = gcry_cipher_setkey(cipher_hd, keys->s, key_length);
	if(gcry_ret) {
		LM_ERR("gcry_cipher_setkey failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		goto error;
	}

	blk_length = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES256);
	if(_gcrypt_init_vector.len < blk_length) {
		LM_ERR("the init vector is too short\n");
		goto error;
	}

	gcry_ret = gcry_cipher_setiv(cipher_hd, _gcrypt_init_vector.s, blk_length);
	if(gcry_ret) {
		LM_ERR("gcry_cipher_setiv failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		goto error;
	}

	decrypted_txt = malloc(etext.len + 1);
	if(decrypted_txt == 0) {
		SYS_MEM_ERROR;
		goto error;
	}
	decrypted_txt[0] = '\0';
	gcry_ret = gcry_cipher_decrypt(cipher_hd, // gcry_cipher_hd_t h
			decrypted_txt,					  // unsigned char *out
			etext.len,						  // size_t outsize
			etext.s,						  // const unsigned char *in
			etext.len);						  // size_t inlen
	if(gcry_ret) {
		LM_ERR("gcry_cipher_decrypt failed:  %s/%s\n", gcry_strsource(gcry_ret),
				gcry_strerror(gcry_ret));
		goto error;
	}

	val.rs.len = etext.len;
	val.rs.s = decrypted_txt;

	LM_DBG("plain result: [%.*s]\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	free(decrypted_txt);
	gcry_cipher_close(cipher_hd);
	return 1;

error:
	if(decrypted_txt) {
		free(decrypted_txt);
	}
	gcry_cipher_close(cipher_hd);
	return -1;
}

/**
 *
 */
static int ki_gcrypt_aes_decrypt(sip_msg_t *msg, str *ins, str *keys, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);

	if(dst == NULL) {
		LM_ERR("failed getting pv: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_gcrypt_aes_decrypt_helper(msg, ins, keys, dst);
}

/**
 *
 */
static int w_gcrypt_aes_decrypt(
		sip_msg_t *msg, char *inb, char *keyb, char *outb)
{
	str ins;
	str keys;
	pv_spec_t *dst;

	if(fixup_get_svalue(msg, (gparam_t *)inb, &ins) != 0) {
		LM_ERR("cannot get input value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)keyb, &keys) != 0) {
		LM_ERR("cannot get key value\n");
		return -1;
	}

	dst = (pv_spec_t *)outb;

	return ki_gcrypt_aes_decrypt_helper(msg, &ins, &keys, dst);
}

/**
 *
 */
int bind_gcrypt(gcrypt_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->aes128_context_init = aes128_context_init;
	api->aes128_context_destroy = aes128_context_destroy;
	api->aes128_encrypt = aes128_encrypt;
	api->aes128_decrypt = aes128_decrypt;

	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_gcrypt_exports[] = {
	{ str_init("gcrypt"), str_init("aes_encrypt"),
		SR_KEMIP_INT, ki_gcrypt_aes_encrypt,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("gcrypt"), str_init("aes_decrypt"),
		SR_KEMIP_INT, ki_gcrypt_aes_decrypt,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_gcrypt_exports);
	return 0;
}
