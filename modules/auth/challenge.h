/*
 * Digest Authentication Module
 * 
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 */


#ifndef CHALLENGE_H
#define CHALLENGE_H

#include "../../str.h"

/**
 * Create {WWW,Proxy}-Authenticate header field
 * @param nonce nonce value
 * @param algorithm algorithm value
 * @return -1 on error, 0 on success
 * 
 * The result is stored in an attribute.
 * If nonce is not null that it is used, instead of call calc_nonce.
 * If algorithm is not null that it is used irrespective of _PRINT_MD5
 * 
 * Major usage of nonce and algorithm params is AKA authentication. 
 */
typedef int (*build_challenge_hf_t)(struct sip_msg* msg, int stale,
		str* realm, str* nonce, str* algorithm, int hftype);
int build_challenge_hf(struct sip_msg* msg, int stale, str* realm,
		str* nonce, str* algorithm, int hftype);

int get_challenge_hf(struct sip_msg* msg, int stale, str* realm,
		str* nonce, str* algorithm, struct qp* qop, int hftype, str *ahf);

void strip_realm(str* _realm);

#endif /* CHALLENGE_H */
