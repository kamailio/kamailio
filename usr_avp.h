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
 *
 * History:
 * ---------
 *  2004-02-06  created (bogdan)
 */

#ifndef _SER_URS_AVP_H_
#define _SER_URS_AVP_H_


#include "parser/msg_parser.h"


struct usr_avp {
	unsigned int id;
	str attr;
	union {
		str  str_val;
		unsigned int int_val;
	}val;
	struct usr_avp *next;
};

extern struct usr_avp   *users_avps;


#define AVP_USER_RURI    1
#define AVP_USER_FROM    2
#define AVP_USER_TO      3

#define AVP_ALL_ATTR     ((char*)0xffffffff)


/* init function */
int init_avp_child( int rank );
int get_user_type( char *id );

/* load/free/seach functions */
void destroy_avps( );
int load_avp( struct sip_msg *msg, int type, char *attr, int use_dom);

#endif

