/*
 * Route & Record-Route module, avp cookie support
 *
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef AVP_COOKIE_H
#define AVP_COOKIE_H

#include "../../sr_module.h"
#include "../../usr_avp.h"
#include <sys/types.h> /* for regex */
#include <regex.h>

typedef struct avp_save_item_t {
	unsigned short type;
	union {
		str s;
		regex_t re;
		int n;
	} u;
} avp_save_item_t;

extern regex_t* cookie_filter_re;
extern unsigned short crc_secret;
extern avp_flags_t avp_flag_dialog;

int rr_add_avp_cookie(struct sip_msg *msg, char *param1, char *param2);
str *rr_get_avp_cookies(void);
void rr_set_avp_cookies(str *enc_cookies, int reverse_direction);


#endif  // AVP_COOKIE_H
