/*
 *
 * Copyright (C) 2013 Voxbone SA
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * 
 */

#include "../../data_lump.h"
#include "../../parser/msg_parser.h"	/* struct sip_msg */

#ifndef _SIPT_SDP_MANGLE_
#define _SIPT_SDP_MANGLE_


struct sdp_mangler
{
	struct sip_msg *msg;
	int body_offset;
};


int replace_body_segment(struct sdp_mangler * mangler, int offset, int len, unsigned char * new_data, int new_len);
int add_body_segment(struct sdp_mangler * mangler, int offset, unsigned char * new_data, int new_len);

#endif
