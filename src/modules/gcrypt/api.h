/**
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _GCRYPT_API_H_
#define _GCRYPT_API_H_

#include <stdint.h>

typedef void *(*gcrypt_aes128_context_init_f)(uint8_t key[16]);
typedef void (*gcrypt_aes128_context_destroy_f)(void **context);
typedef int (*gcrypt_aes128_encrypt_f)(
		uint8_t *output, uint8_t *input, void *context);
typedef int (*gcrypt_aes128_decrypt_f)(
		uint8_t *output, uint8_t *input, void *context);

typedef struct gcrypt_api
{
	gcrypt_aes128_context_init_f aes128_context_init;
	gcrypt_aes128_context_destroy_f aes128_context_destroy;
	gcrypt_aes128_encrypt_f aes128_encrypt;
	gcrypt_aes128_decrypt_f aes128_decrypt;
} gcrypt_api_t;

typedef int (*bind_gcrypt_f)(gcrypt_api_t *api);
int bind_htable(gcrypt_api_t *api);

/**
 * @brief Load the GCrypt API
 */
static inline int gcrypt_load_api(gcrypt_api_t *api)
{
	bind_gcrypt_f bindgcrypt;

	bindgcrypt = (bind_gcrypt_f)find_export("bind_gcrypt", 0, 0);
	if(bindgcrypt == 0) {
		LM_ERR("cannot find bind_gcrypt\n");
		return -1;
	}
	if(bindgcrypt(api) < 0) {
		LM_ERR("cannot bind gcrypt api\n");
		return -1;
	}
	return 0;
}

#endif
