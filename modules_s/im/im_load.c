/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#include "im_load.h"
#include "im_funcs.h"

#define LOAD_ERROR "ERROR: im_bind: IM module function "

int load_im( struct im_binds *imb)
{
	if (!( imb->im_extract_body=(im_extract_body_f)
	find_export("im_extract_body", 2)) ) {
		LOG(L_ERR, LOAD_ERROR "'im_extract_body' not found!\n");
		return -1;
	}
	if (!( imb->im_check_content_type=(im_check_content_type_f)
	find_export("im_check_content_type", 1)) ) {
		LOG(L_ERR, LOAD_ERROR "'im_check_content_type' not found\n");
		return -1;
	}
	if (!( imb->im_get_body_len=(im_get_body_len_f)
	find_export("im_get_body_len", 1)) ) {
		LOG( L_ERR, LOAD_ERROR "'im_get_body_len' not found\n");
		return -1;
	}
	return 1;

}
