/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
 *
 */

#ifndef _AUTH_XKEYS_H_
#define _AUTH_XKEYS_H_

#include "../../str.h"

int authx_xkey_add_params(str *sparam);
int auth_xkeys_add(sip_msg_t* msg, str *hdr, str *key,
		str *alg, str *data);
int auth_xkeys_check(sip_msg_t* msg, str *hdr, str *key,
		str *alg, str *data);
int auth_xkeys_init_rpc(void);

#endif
