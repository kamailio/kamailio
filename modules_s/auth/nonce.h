/*
 * $Id$
 *
 * Nonce related functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef NONCE_H
#define NONCE_H

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include <time.h>

/* auth_extra_checks flags */

#define AUTH_CHECK_FULL_URI (1 << 0)
#define AUTH_CHECK_CALLID   (1 << 1)
#define AUTH_CHECK_FROMTAG  (1 << 2)
#define AUTH_CHECK_SRC_IP   (1 << 3)


/*
 * Maximum length of nonce string in bytes
 * nonce = TIMESTAMP[8 chars] MD5SUM(TIMESTAMP, SECRET1)[32 chars] \
 *          MD5SUM(info(auth_extra_checks), SECRET2)[32 chars]
 */
#define MAX_NONCE_LEN (8 + 8 + 32 + 32)
/*
 * Minimum length of the nonce string
 * nonce = TIMESTAMP[8 chars] MD5SUM(TIMESTAMP, SECRET1)[32 chars]
 */
#define MIN_NONCE_LEN (8 + 8 + 32)

/* Extra authentication checks for REGISTER messages */
extern int auth_checks_reg;
/* Extra authentication checks for out-of-dialog requests */
extern int auth_checks_ood;
/* Extra authentication checks for in-dialog requests */
extern int auth_checks_ind;


int get_auth_checks(struct sip_msg* msg);


/*
 * get the configured nonce len
 */
#define get_nonce_len(cfg) ((cfg)?MAX_NONCE_LEN:MIN_NONCE_LEN)


/*
 * Calculate nonce value
 */
int calc_nonce(char* nonce, int* nonce_len, int cfg, int since, int expires, str* secret1,
			   str* secret2, struct sip_msg* msg);


/*
 * Check nonce value received from UA
 */
int check_nonce(str* nonce, str* secret1, str* secret2, struct sip_msg* msg);


/*
 * Get expiry time from nonce string
 */
time_t get_nonce_expires(str* nonce);

/*
 * Get valid_since time from nonce string
 */
time_t get_nonce_since(str* nonce);

/*
 * Check if the nonce is stale
 */
int is_nonce_stale(str* nonce);


#endif /* NONCE_H */
