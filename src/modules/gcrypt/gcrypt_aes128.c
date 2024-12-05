/*
 * Copyright (C) 2024 Dragos Vingarzan (neatpath.net)
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
 */

#include "gcrypt_aes128.h"

#include "../../core/dprint.h"

void *aes128_context_init(uint8_t key[16])
{
	gcry_cipher_hd_t context = 0;
	gcry_error_t err;

	err = gcry_cipher_open(
			&context, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CBC, 0);
	if(err) {
		LM_ERR("aes128_context_init: error opening cipher: %s/%s\n",
				gcry_strsource(err), gcry_strerror(err));
		return NULL;
	}

	err = gcry_cipher_setkey(context, key, 16);
	if(err) {
		LM_ERR("aes128_context_init: error setting key: %s/%s\n",
				gcry_strsource(err), gcry_strerror(err));
		gcry_cipher_close(context);
		return NULL;
	}

	return context;
}

void aes128_context_destroy(void **context)
{
	if(context != NULL) {
		gcry_cipher_close(*context);
		*context = NULL;
	}
}

int aes128_encrypt(uint8_t *output, uint8_t *input, void *context)
{
	if(context == NULL) {
		LM_ERR("aes128_encrypt: context is NULL\n");
		return -1;
	}

	gcry_error_t err;
	uint8_t iv[16] = {0};

	err = gcry_cipher_setiv(context, iv, 16);
	if(err) {
		LM_ERR("aes128_context_destroy: error setting IV: %s/%s\n",
				gcry_strsource(err), gcry_strerror(err));
		return -1;
	}

	err = gcry_cipher_encrypt((gcry_cipher_hd_t)context, output, 16, input, 16);
	if(err) {
		LM_ERR("aes128_context_destroy: error encrypting: %s/%s\n",
				gcry_strsource(err), gcry_strerror(err));
		return -1;
	}

	return 0;
}

int aes128_decrypt(uint8_t *output, uint8_t *input, void *context)
{
	if(context == NULL) {
		LM_ERR("aes128_decrypt: context is NULL\n");
		return -1;
	}

	gcry_error_t err;
	uint8_t iv[16] = {0};

	err = gcry_cipher_setiv(context, iv, 16);
	if(err) {
		LM_ERR("aes128_context_destroy: error setting IV: %s/%s\n",
				gcry_strsource(err), gcry_strerror(err));
		return -1;
	}

	err = gcry_cipher_decrypt((gcry_cipher_hd_t)context, output, 16, input, 16);
	if(err) {
		LM_ERR("aes128_context_destroy: error encrypting: %s/%s\n",
				gcry_strsource(err), gcry_strerror(err));
		return -1;
	}

	return 0;
}
