/*
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 *  2004-07-21  created (bogdan)
 */

#ifndef _SER_URS_AVP_H_
#define _SER_URS_AVP_H_


#include "str.h"

typedef union {
	int  n;
	str *s;
} int_str;



struct usr_avp {
	unsigned short id;
	unsigned short flags;
	struct usr_avp *next;
	void *data;
};

#define AVP_NAME_STR     (1<<0)
#define AVP_VAL_STR      (1<<1)

/* add functions */
int add_avp( unsigned short flags, int_str name, int_str val);


/* search functions */
struct usr_avp *search_first_avp( unsigned short name_type, int_str name,
															int_str *val );
struct usr_avp *search_next_avp( struct usr_avp *avp, int_str *val  );


/* free functions */
void reset_avps( );
void destroy_avp( struct usr_avp *avp);
void destroy_avp_list( struct usr_avp **list );
void destroy_avp_list_unsafe( struct usr_avp **list );

/* get func */
void get_avp_val(struct usr_avp *avp, int_str *val );
str* get_avp_name(struct usr_avp *avp);
struct usr_avp** set_avp_list( struct usr_avp **list );
struct usr_avp** get_avp_list( );


#endif

