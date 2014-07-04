/*
 * $Id$
 *
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#ifndef OB_API_H
#define OB_API_H

#include "../../str.h"
#include "../../sr_module.h"

typedef int (*encode_flow_token_t)(str *, struct receive_info);
typedef int (*decode_flow_token_t)(struct sip_msg *, struct receive_info **, str);
typedef int (*use_outbound_t)(struct sip_msg *);

typedef struct ob_binds {
	encode_flow_token_t encode_flow_token;
	decode_flow_token_t decode_flow_token;
	use_outbound_t use_outbound;
} ob_api_t;

typedef int (*bind_ob_f)(ob_api_t*);

int bind_ob(struct ob_binds*);

inline static int ob_load_api(ob_api_t *pxb)
{
	bind_ob_f bind_ob_exports;
	if (!(bind_ob_exports = (bind_ob_f)find_export("bind_ob", 1, 0)))
	{
		LM_INFO("Failed to import bind_ob\n");
		return -1;
	}
	return bind_ob_exports(pxb);
}

#endif /* OB_API_H */
