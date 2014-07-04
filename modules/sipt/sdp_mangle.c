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

#include "sdp_mangle.h"

int replace_body_segment(struct sdp_mangler * mangler, int offset, int len, unsigned char * new_data, int new_len)
{
	
	struct lump * l;
	char *s;

	l = del_lump(mangler->msg, mangler->body_offset + offset, len, 0);

	if(l == NULL)
	{
		return -1;
	}

 	s = pkg_malloc(new_len);
	memcpy(s, new_data, new_len);

	if(insert_new_lump_after(l, s, new_len, 0) == 0)
	{
		pkg_free(s);
		return -2;
	}

	return 0;
}

int add_body_segment(struct sdp_mangler * mangler, int offset, unsigned char * new_data, int new_len)
{
	
	struct lump * l;
	char *s;
	int exists;
	l = anchor_lump2(mangler->msg, mangler->body_offset + offset, 0, 0, &exists);
	if(l == NULL)
	{
		return -1;
	}

 	s = pkg_malloc(new_len);
	memcpy(s, new_data, new_len);
	if(insert_new_lump_after(l, s, new_len, 0) == 0)
	{
		pkg_free(s);
		return -2;
	}

	return 0;
}

