/*
 * Copyright (C) 2019 1&1 Internet AG
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
/*! crypto API export binding */
#ifndef CRYPTO_API_H_
#define CRYPTO_API_H_

#include "../../core/str.h"
#include "../../core/sr_module.h"

typedef int (*SHA1_hash_f)(str*, str*);

typedef struct crypto_binds {
	SHA1_hash_f SHA1;
} crypto_api_t;


typedef  int (*load_crypto_f)( struct crypto_binds* );

/*!
* \brief API bind function exported by the module - it will load the other functions
 * \param rrb record-route API export binding
 * \return 1
 */
int load_crypto( struct crypto_binds *cb );

/*!
 * \brief Function to be called directly from other modules to load the CRYPTO API
 * \param cb crypto API export binding
 * \return 0 on success, -1 if the API loader could not imported
 */
inline static int load_crypto_api( struct crypto_binds *cb )
{
	load_crypto_f load_crypto_v;

	/* import the crypto auto-loading function */
	if ( !(load_crypto_v=(load_crypto_f)find_export("load_crypto", 0, 0))) {
		LM_ERR("failed to import load_crypto\n");
		return -1;
	}
	/* let the auto-loading function load all crypto stuff */
	load_crypto_v( cb );

	return 0;
}

#endif
