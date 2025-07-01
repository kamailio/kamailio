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

#ifndef _CRYPTO_AES_H_
#define _CRYPTO_AES_H_

#include <openssl/evp.h>

#define AES_BLOCK_SIZE 256
#define CRYPTO_SALT_BSIZE 16

int crypto_set_salt(char *psalt);
char *crypto_get_salt(void);

int crypto_aes_init(unsigned char *key_data, int key_data_len,
		unsigned char *salt, unsigned char *custom_iv, EVP_CIPHER_CTX *e_ctx,
		EVP_CIPHER_CTX *d_ctx);

unsigned char *crypto_aes_encrypt(
		EVP_CIPHER_CTX *e, unsigned char *plaintext, int *len);

unsigned char *crypto_aes_decrypt(
		EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len);
#endif