/*
 * $Id$
 *
 * Digest Authentication Module
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
 *
 * History:
 * --------
 * 2003-04-28 rpid contributed by Juha Heinanen added (janakj)
 */

#ifndef AUTH_MOD_H
#define AUTH_MOD_H

#include "../../str.h"
#include "../../parser/msg_parser.h"    /* struct sip_msg */


/*
 * Module parameters variables
 */
extern str secret;            /* secret phrase used to generate nonce */
extern int nonce_expire;      /* nonce expire interval */
extern str rpid_prefix;       /* Remote-Party-ID prefix */
extern str rpid_suffix;       /* Remote-Party-ID suffix */
extern str realm_prefix;      /* strip off auto-generated realm */
extern str rpid_avp;          /* The name of AVP containing the rpid value */


/* Stateless reply function pointer */
extern int (*sl_reply)(struct sip_msg* _m, char* _str1, char* _str2);

#endif /* AUTH_MOD_H */
