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
#ifndef AUTH_VECTOR_H
#define AUTH_VECTOR_H

#include <stdint.h>
#include <time.h>

#include "../../core/str.h"

/** Enumeration for the Authorization Vector status */
enum auth_vector_status
{
	AUTH_VECTOR_UNUSED = 0,
	AUTH_VECTOR_SENT = 1,
	AUTH_VECTOR_USELESS = 2, /**< invalidated, marked for deletion 		*/
	AUTH_VECTOR_USED = 3	 /**< the vector has been successfully used	*/
};

/** Authorization Vector storage structure */
typedef struct _auth_vector
{
	int item_number;	/**< index of the auth vector		*/
	unsigned char type; /**< type of authentication vector 	*/
	str authenticate;	/**< challenge (rand|autn in AKA)	*/
	str authorization;	/**< expected response				*/
	str ck;				/**< Cypher Key						*/
	str ik;				/**< Integrity Key					*/
	time_t expires;		/**< expires in (after it is sent)	*/
	uint32_t use_nb;	/**< number of use (nonce count) */

	int is_locally_generated; /**< flag to indicate if the vector was generated locally, not on the HSS */

	enum auth_vector_status status; /**< current status		*/
	struct _auth_vector *next;		/**< next av in the list		*/
	struct _auth_vector *prev;		/**< previous av in the list	*/
} auth_vector;

auth_vector *auth_vector_make_local(uint8_t k[16], uint8_t op[16], int opIsOPc,
		uint8_t amf[2], uint8_t sqn[6]);
int auth_vector_resync_local(uint8_t sqnMSout[6], auth_vector *av,
		uint8_t auts[14], uint8_t k[16], uint8_t op[16], int opIsOPc);
void sqn_increment(uint8_t sqn[6]);

#endif
