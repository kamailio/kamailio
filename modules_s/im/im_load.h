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


#ifndef _IM_BIND_H
#define _IM_BIND_H

#include "../../sr_module.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "im_funcs.h"

typedef int (*im_extract_body_f)( struct sip_msg* , str* );
typedef int (*im_check_content_type_f)( struct sip_msg* );
typedef int (*im_get_body_len_f)( struct sip_msg* );

struct im_binds {
	im_extract_body_f im_extract_body;
	im_check_content_type_f im_check_content_type;
	im_get_body_len_f im_get_body_len;
};


typedef int(*load_im_f)( struct im_binds *imb );
int load_im( struct im_binds *imb);


#endif
