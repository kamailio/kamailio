/**
 * Copyright 2016 (C) Orange
 * <camille.oudot@orange.com>
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

#include "pv_api.h"
#include "pv_core.h"

int pv_register_api(pv_api_t* api)
{
	if (!api)
		return 0;

	api->get_body_size = pv_get_body_size;
	api->get_hdr = pv_get_hdr;
	api->get_msg_body = pv_get_msg_body;
	api->get_msg_buf = pv_get_msg_buf;
	api->get_msg_len = pv_get_msg_len;
	api->get_reason = pv_get_reason;
	api->get_status = pv_get_status;
	api->parse_hdr_name = pv_parse_hdr_name;
	return 1;
}
