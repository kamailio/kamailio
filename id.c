/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
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

#include "id.h"


/*
 * Set From UID
 */
void set_from_uid(str* uid)
{
	struct search_state s;
	int_str name, val;
	avp_t* a;

	a = search_first_avp(AVP_USER | AVP_NAME_STR, name, 0, &s);
	while(a) {
		destroy_avp(a);
		a = search_next_avp(&s, 0);
	}

	val.s = uid;
	add_avp(AVP_USER | AVP_NAME_STR | AVP_VAL_STR, name, val);
}



/*
 * Set From UID
 */
int get_from_uid(str* uid)
{
	static str name_s = STR_STATIC_INIT(AVP_UID);
	int_str name, val;

	name.s = &name_s;
	if (search_first_avp(AVP_USER | AVP_NAME_STR, name, &val, 0)) {
		*uid = *val.s;
		return 1;
	} else {
		uid->s = 0;
		uid->len = 0;
		return 0;
	}
}


/*
 * Set To UID
 */
void set_to_uid(str* uid)
{
	struct search_state s;
	int_str name, val;
	avp_t* a;

	a = search_first_avp(AVP_USER | AVP_NAME_STR, name, 0, &s);
	while(a) {
		destroy_avp(a);
		a = search_next_avp(&s, 0);
	}

	val.s = uid;
	add_avp(AVP_USER | AVP_NAME_STR | AVP_VAL_STR, name, val);
}



/*
 * Set To UID
 */
int get_to_uid(str* uid)
{
	static str name_s = STR_STATIC_INIT(AVP_UID);
	int_str name, val;

	name.s = &name_s;
	if (search_first_avp(AVP_USER | AVP_NAME_STR, name, &val, 0)) {
		*uid = *val.s;
		return 1;
	} else {
		uid->s = 0;
		uid->len = 0;
		return 0;
	}
}


/*
 * Return the current domain id
 */
int get_did(str* did)
{
	static str name_s = STR_STATIC_INIT(AVP_DID);
	int_str name, val;
	
	name.s = &name_s;
	if (search_first_avp(AVP_DOMAIN | AVP_NAME_STR, name, &val, 0)) {
		*did = *val.s;
		return 1;
	} else {
		did->s = 0;
		did->len = 0;
		return 0;
	}	
}
