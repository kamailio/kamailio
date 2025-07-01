/**
 * Copyright (C) 2016-2020 Daniel-Constantin Mierla (asipto.com)
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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/mem/pkg.h"

#include "crypto_aes.h"

extern int _crypto_key_derivation;
static char _crypto_salt[CRYPTO_SALT_BSIZE];
static int _crypto_salt_set = 0;

/**
 *
 */
int crypto_set_salt(char *psalt)
{
	int i;
	char k;

	memset(_crypto_salt, 0, CRYPTO_SALT_BSIZE * sizeof(char));
	if(psalt != NULL) {
		if(strlen(psalt) < 8) {
			LM_ERR("salt parameter must be at least 8 characters\n");
			return -1;
		}
		k = 97;
		for(i = 0; i < strlen(psalt); i++) {
			if(i >= CRYPTO_SALT_BSIZE)
				break;
			_crypto_salt[i] = (psalt[i] * 7 + k + k * (i + 1)) % 0xff;
			k = _crypto_salt[i];
		}
		_crypto_salt_set = 1;
	}
	return 0;
}

/**
 *
 */
char *crypto_get_salt(void)
{
	if(_crypto_salt_set == 0) {
		return NULL;
	}
	return _crypto_salt;
}

/**
 * Create an 256 bit key and IV using the supplied key_data and salt.
 * Fills in the encryption and decryption ctx objects and returns 0 on success
 */
int crypto_aes_init(unsigned char *key_data, int key_data_len,
		unsigned char *salt, unsigned char *custom_iv, EVP_CIPHER_CTX *e_ctx,
		EVP_CIPHER_CTX *d_ctx)
{
	int i, nrounds = 5;
	int x;
	unsigned char key[32],
			iv[32]; /* IV is only 16 bytes, but makes it easier */
	const EVP_CIPHER *cipher;

	memset(key, 0, sizeof(key));
	memset(iv, 0, sizeof(iv));
	/*
	 * Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash
	 * the supplied key material.
	 * nrounds is the number of times the we hash the material. More rounds
	 * are more secure but slower.
	 */
	if(_crypto_key_derivation) {
		i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, key_data,
				key_data_len, nrounds, key, iv);
		if(i != 32) {
			LM_ERR("key size is %d bits - should be 256 bits\n", i);
			return -1;
		}

		for(x = 0; x < 32; ++x)
			LM_DBG("key: %x iv: %x \n", key[x], iv[x]);

		for(x = 0; x < 8; ++x)
			LM_DBG("salt: %x\n", salt[x]);
		cipher = EVP_aes_256_cbc();
	} else {
		cipher = (key_data_len == 16) ? EVP_aes_128_cbc() : EVP_aes_256_cbc();
		LM_DBG("got %d bytes key\n", key_data_len * 8);
		memcpy(key, key_data, key_data_len);
		if(custom_iv)
			memcpy(iv, custom_iv, 16);

		for(x = 0; x < key_data_len; ++x)
			LM_DBG("key: %x, iv: %x\n", key[x], iv[x]);
	}

	if(e_ctx) {
		EVP_CIPHER_CTX_init(e_ctx);
		EVP_EncryptInit_ex(e_ctx, cipher, NULL, key, iv);
	}
	if(d_ctx) {
		EVP_CIPHER_CTX_init(d_ctx);
		EVP_DecryptInit_ex(d_ctx, cipher, NULL, key, iv);
	}

	return 0;
}

/*
 * Encrypt *len bytes of data
 * All data going in & out is considered binary (unsigned char[])
 */
unsigned char *crypto_aes_encrypt(
		EVP_CIPHER_CTX *e, unsigned char *plaintext, int *len)
{
	/* max ciphertext len for a n bytes of plaintext is
	 * n + AES_BLOCK_SIZE -1 bytes */
	int c_len = *len + AES_BLOCK_SIZE - 1, f_len = 0;
	unsigned char *ciphertext = (unsigned char *)malloc(c_len);

	if(ciphertext == NULL) {
		SYS_MEM_ERROR;
		return NULL;
	}
	/* allows reusing of 'e' for multiple encryption cycles */
	if(!EVP_EncryptInit_ex(e, NULL, NULL, NULL, NULL)) {
		LM_ERR("failure in EVP_EncryptInit_ex \n");
		free(ciphertext);
		return NULL;
	}

	/* update ciphertext, c_len is filled with the length of ciphertext
	 * generated, *len is the size of plaintext in bytes */
	if(!EVP_EncryptUpdate(e, ciphertext, &c_len, plaintext, *len)) {
		LM_ERR("failure in EVP_EncryptUpdate \n");
		free(ciphertext);
		return NULL;
	}

	/* update ciphertext with the final remaining bytes */
	if(!EVP_EncryptFinal_ex(e, ciphertext + c_len, &f_len)) {
		LM_ERR("failure in EVP_EncryptFinal_ex \n");
		free(ciphertext);
		return NULL;
	}

	*len = c_len + f_len;
	return ciphertext;
}

/*
 * Decrypt *len bytes of ciphertext
 */
unsigned char *crypto_aes_decrypt(
		EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len)
{
	/* plaintext will always be equal to or lesser than length of ciphertext*/
	int p_len = *len, f_len = 0;
	unsigned char *plaintext = (unsigned char *)malloc(p_len);

	if(plaintext == NULL) {
		SYS_MEM_ERROR;
		return NULL;
	}
	if(!EVP_DecryptInit_ex(e, NULL, NULL, NULL, NULL)) {
		LM_ERR("failure in EVP_DecryptInit_ex \n");
		free(plaintext);
		return NULL;
	}

	if(!EVP_DecryptUpdate(e, plaintext, &p_len, ciphertext, *len)) {
		LM_ERR("failure in EVP_DecryptUpdate\n");
		free(plaintext);
		return NULL;
	}

	if(!EVP_DecryptFinal_ex(e, plaintext + p_len, &f_len)) {
		LM_ERR("failure in EVP_DecryptFinal_ex\n");
		free(plaintext);
		return NULL;
	}

	*len = p_len + f_len;
	return plaintext;
}