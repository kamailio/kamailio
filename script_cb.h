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


#include "parser/msg_parser.h"

typedef int (cb_function)( struct sip_msg *msg, void *param );

typedef enum {
    PRE_SCRIPT_CB,
	POST_SCRIPT_CB
} callback_t;       /* Allowed types of callbacks */


struct script_cb{
	cb_function *cbf;
	struct script_cb *next;
	unsigned int id;
	void *param;
};

int register_script_cb( cb_function f, callback_t t, void *param );
int exec_pre_cb( struct sip_msg *msg);
void exec_post_cb( struct sip_msg *msg);
void destroy_script_cb();


