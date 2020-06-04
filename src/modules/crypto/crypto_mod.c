/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/pvapi.h"
#include "../../core/lvalue.h"
#include "../../core/basex.h"
#include "../../core/kemi.h"

#include "crypto_aes.h"
#include "crypto_uuid.h"
#include "crypto_evcb.h"
#include "api.h"

MODULE_VERSION

int crypto_aes_init(unsigned char *key_data, int key_data_len,
		unsigned char *salt, EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx);
unsigned char *crypto_aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext,
		int *len);
unsigned char *crypto_aes_decrypt(EVP_CIPHER_CTX *e, unsigned char *ciphertext,
		int *len);

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_crypto_aes_encrypt(sip_msg_t* msg, char* inb, char* keyb, char* outb);
static int fixup_crypto_aes_encrypt(void** param, int param_no);
static int w_crypto_aes_decrypt(sip_msg_t* msg, char* inb, char* keyb, char* outb);
static int fixup_crypto_aes_decrypt(void** param, int param_no);

static int w_crypto_nio_in(sip_msg_t* msg, char* p1, char* p2);
static int w_crypto_nio_out(sip_msg_t* msg, char* p1, char* p2);
static int w_crypto_nio_encrypt(sip_msg_t* msg, char* p1, char* p2);
static int w_crypto_nio_decrypt(sip_msg_t* msg, char* p1, char* p2);

static char *_crypto_salt_param = "k8hTm4aZ";

static int _crypto_register_callid = 0;
static int _crypto_register_evcb = 0;

str _crypto_kevcb_netio = STR_NULL;
str _crypto_netio_key = STR_NULL;

static cmd_export_t cmds[]={
	{"crypto_aes_encrypt", (cmd_function)w_crypto_aes_encrypt, 3,
		fixup_crypto_aes_encrypt, 0, ANY_ROUTE},
	{"crypto_aes_decrypt", (cmd_function)w_crypto_aes_decrypt, 3,
		fixup_crypto_aes_decrypt, 0, ANY_ROUTE},
	{"crypto_netio_in", (cmd_function)w_crypto_nio_in, 0,
		0, 0, ANY_ROUTE},
	{"crypto_netio_out", (cmd_function)w_crypto_nio_out, 0,
		0, 0, ANY_ROUTE},
	{"crypto_netio_encrypt", (cmd_function)w_crypto_nio_encrypt, 0,
		0, 0, ANY_ROUTE},
	{"crypto_netio_decrypt", (cmd_function)w_crypto_nio_decrypt, 0,
		0, 0, ANY_ROUTE},
	{"load_crypto",        (cmd_function)load_crypto, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{ "salt",            PARAM_STRING, &_crypto_salt_param },
	{ "register_callid", PARAM_INT, &_crypto_register_callid },
	{ "register_evcb",   PARAM_INT, &_crypto_register_evcb },
	{ "kevcb_netio",     PARAM_STR, &_crypto_kevcb_netio },
	{ "netio_key",       PARAM_STR, &_crypto_netio_key },

	{ 0, 0, 0 }
};

struct module_exports exports = {
	"crypto",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};


/**
 * @brief Initialize crypto module function
 */
static int mod_init(void)
{

	if(_crypto_salt_param==NULL || _crypto_salt_param[0]==0) {
		_crypto_salt_param = NULL;
	}

	if(crypto_set_salt(_crypto_salt_param) < 0) {
		return -1;
	}

	if(_crypto_register_callid!=0) {
		if(crypto_init_callid()<0) {
			LM_ERR("failed to init callid callback\n");
			return -1;
		}
		if(crypto_register_callid_func()<0) {
			LM_ERR("unable to register callid callback\n");
			return -1;
		}
		LM_DBG("registered crypto callid callback\n");
	}

	if(_crypto_register_evcb!=0) {
		if(_crypto_netio_key.s==NULL || _crypto_netio_key.len<=0) {
			LM_ERR("crypto netio key parameter is not set\n");
			return -1;
		}
		crypto_evcb_enable();
	}

	return 0;
}

/**
 * @brief Initialize crypto module children
 */
static int child_init(int rank)
{
	if(_crypto_register_callid!=0 && crypto_child_init_callid(rank)<0) {
		LM_ERR("failed to register callid callback\n");
		return -1;
	}

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
static int ki_crypto_aes_encrypt_helper(sip_msg_t* msg, str *ins, str *keys,
		pv_spec_t *dst)
{
	pv_value_t val;
	EVP_CIPHER_CTX *en = NULL;
	str etext;

	en = EVP_CIPHER_CTX_new();
	if(en==NULL) {
		LM_ERR("cannot get new cipher context\n");
		return -1;
	}

	/* gen key and iv. init the cipher ctx object */
	if (crypto_aes_init((unsigned char *)keys->s, keys->len,
				(unsigned char*)crypto_get_salt(), en, NULL)) {
		EVP_CIPHER_CTX_free(en);
		LM_ERR("couldn't initialize AES cipher\n");
		return -1;
	}
	etext.len = ins->len;
	etext.s = (char *)crypto_aes_encrypt(en, (unsigned char *)ins->s, &etext.len);
	if(etext.s==NULL) {
		EVP_CIPHER_CTX_free(en);
		LM_ERR("AES encryption failed\n");
		return -1;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.rs.s = pv_get_buffer();
	val.rs.len = base64_enc((unsigned char *)etext.s, etext.len,
					(unsigned char *)val.rs.s, pv_get_buffer_size()-1);
	if (val.rs.len < 0) {
		EVP_CIPHER_CTX_free(en);
		LM_ERR("base64 output of encrypted value is too large (need %d)\n",
				-val.rs.len);
		goto error;
	}
	LM_DBG("base64 encrypted result: [%.*s]\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	free(etext.s);
	EVP_CIPHER_CTX_cleanup(en);
	EVP_CIPHER_CTX_free(en);
	return 1;

error:
	free(etext.s);
	EVP_CIPHER_CTX_cleanup(en);
	EVP_CIPHER_CTX_free(en);
	return -1;
}

/**
 *
 */
static int ki_crypto_aes_encrypt(sip_msg_t* msg, str *ins, str *keys, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);

	if(dst==NULL) {
		LM_ERR("failed getting pv: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_crypto_aes_encrypt_helper(msg, ins, keys, dst);
}

/**
 *
 */
static int w_crypto_aes_encrypt(sip_msg_t* msg, char* inb, char* keyb, char* outb)
{
	str ins;
	str keys;
	pv_spec_t *dst;

	if (fixup_get_svalue(msg, (gparam_t*)inb, &ins) != 0) {
		LM_ERR("cannot get input value\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)keyb, &keys) != 0) {
		LM_ERR("cannot get key value\n");
		return -1;
	}
	dst = (pv_spec_t*)outb;

	return ki_crypto_aes_encrypt_helper(msg, &ins, &keys, dst);
}

/**
 *
 */
static int fixup_crypto_aes_encrypt(void** param, int param_no)
{
	if(param_no==1 || param_no==2) {
		if(fixup_spve_null(param, 1)<0)
			return -1;
		return 0;
	} else if(param_no==3) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
	}
	return 0;
}

/**
 *
 */
static int ki_crypto_aes_decrypt_helper(sip_msg_t* msg, str *ins, str *keys,
		pv_spec_t *dst)
{

	pv_value_t val;
	EVP_CIPHER_CTX *de=NULL;
	str etext;

	de = EVP_CIPHER_CTX_new();
	if(de==NULL) {
		LM_ERR("cannot get new cipher context\n");
		return -1;
	}

	/* gen key and iv. init the cipher ctx object */
	if (crypto_aes_init((unsigned char *)keys->s, keys->len,
				(unsigned char*)crypto_get_salt(), NULL, de)) {
		EVP_CIPHER_CTX_free(de);
		LM_ERR("couldn't initialize AES cipher\n");
		return -1;
	}

	memset(&val, 0, sizeof(pv_value_t));
	etext.s = pv_get_buffer();
	etext.len = base64_dec((unsigned char *)ins->s, ins->len,
					(unsigned char *)etext.s, pv_get_buffer_size()-1);
	if (etext.len < 0) {
		EVP_CIPHER_CTX_free(de);
		LM_ERR("base64 inpuy with encrypted value is too large (need %d)\n",
				-etext.len);
		return -1;
	}
	val.rs.len = etext.len;
	val.rs.s = (char *)crypto_aes_decrypt(de, (unsigned char *)etext.s,
			&val.rs.len);
	if(val.rs.s==NULL) {
		EVP_CIPHER_CTX_free(de);
		LM_ERR("AES decryption failed\n");
		return -1;
	}
	LM_DBG("plain result: [%.*s]\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	free(val.rs.s);
	EVP_CIPHER_CTX_cleanup(de);
	EVP_CIPHER_CTX_free(de);
	return 1;
}

/**
 *
 */
static int ki_crypto_aes_decrypt(sip_msg_t* msg, str *ins, str *keys, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);

	if(dst==NULL) {
		LM_ERR("failed getting pv: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_crypto_aes_decrypt_helper(msg, ins, keys, dst);
}

/**
 *
 */
static int w_crypto_aes_decrypt(sip_msg_t* msg, char* inb, char* keyb, char* outb)
{
	str ins;
	str keys;
	pv_spec_t *dst;

	if (fixup_get_svalue(msg, (gparam_t*)inb, &ins) != 0) {
		LM_ERR("cannot get input value\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)keyb, &keys) != 0) {
		LM_ERR("cannot get key value\n");
		return -1;
	}

	dst = (pv_spec_t*)outb;

	return ki_crypto_aes_decrypt_helper(msg, &ins, &keys, dst);
}

/**
 *
 */
static int fixup_crypto_aes_decrypt(void** param, int param_no)
{
	if(param_no==1 || param_no==2) {
		if(fixup_spve_null(param, 1)<0)
			return -1;
		return 0;
	} else if(param_no==3) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
	}
	return 0;
}

/**
 * testing function
 */
int crypto_aes_test(void)
{
	/* "opaque" encryption, decryption ctx structures
	 * that libcrypto uses to record status of enc/dec operations */
	EVP_CIPHER_CTX *en = NULL;
	EVP_CIPHER_CTX *de = NULL;


	/* The salt paramter is used as a salt in the derivation:
	 * it should point to an 8 byte buffer or NULL if no salt is used. */
	unsigned char salt[] = {1,2,3,4,5,6,7,8};

	unsigned char *key_data;
	int key_data_len, i;
	char *input[] = {"Kamailio - The Open Source SIP Server",
		"Thank you for flying Kamailio!",
		"100 Trying\nYour call is important to us",
		NULL
	};

	en = EVP_CIPHER_CTX_new();
	if(en==NULL) {
		LM_ERR("cannot get new cipher context\n");
		return -1;
	}
	de = EVP_CIPHER_CTX_new();
	if(de==NULL) {
		EVP_CIPHER_CTX_free(en);
		LM_ERR("cannot get new cipher context\n");
		return -1;
	}
	/* the key_data for testing */
	key_data = (unsigned char *)"kamailio-sip-server";
	key_data_len = strlen((const char *)key_data);

	/* gen key and iv. init the cipher ctx object */
	if (crypto_aes_init(key_data, key_data_len, salt, en, de)) {
		LM_ERR("couldn't initialize AES cipher\n");
		return -1;
	}

	/* encrypt and decrypt each input string and compare with the original */
	for (i = 0; input[i]; i++) {
		char *plaintext;
		unsigned char *ciphertext;
		int olen, len;

		/* The enc/dec functions deal with binary data and not C strings.
		 * strlen() will return length of the string without counting the '\0'
		 * string marker. We always pass in the marker byte to the
		 * encrypt/decrypt functions so that after decryption we end up with
		 * a legal C string */
		olen = len = strlen(input[i])+1;

		ciphertext = crypto_aes_encrypt(en, (unsigned char *)input[i], &len);
		plaintext = (char *)crypto_aes_decrypt(de, ciphertext, &len);

		if (strncmp(plaintext, input[i], olen))
			LM_ERR("FAIL: enc/dec failed for \"%s\"\n", input[i]);
		else
			LM_NOTICE("OK: enc/dec ok for \"%s\"\n", plaintext);

		free(ciphertext);
		free(plaintext);
	}

	EVP_CIPHER_CTX_cleanup(de);
	EVP_CIPHER_CTX_free(de);
	EVP_CIPHER_CTX_cleanup(en);
	EVP_CIPHER_CTX_free(en);

	return 0;
}

/**
 *
 */
static int w_crypto_nio_in(sip_msg_t* msg, char* p1, char* p2)
{
	return crypto_nio_in(msg);
}

/**
 *
 */
static int w_crypto_nio_out(sip_msg_t* msg, char* p1, char* p2)
{
	return crypto_nio_out(msg);
}

/**
 *
 */
static int w_crypto_nio_encrypt(sip_msg_t* msg, char* p1, char* p2)
{
	return crypto_nio_encrypt(msg);
}

/**
 *
 */
static int w_crypto_nio_decrypt(sip_msg_t* msg, char* p1, char* p2)
{
	return crypto_nio_decrypt(msg);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_crypto_exports[] = {
	{ str_init("crypto"), str_init("aes_encrypt"),
		SR_KEMIP_INT, ki_crypto_aes_encrypt,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("crypto"), str_init("aes_decrypt"),
		SR_KEMIP_INT, ki_crypto_aes_decrypt,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_crypto_exports);
	return 0;
}