/*
 * $Id$
 *
 * Usrloc interface
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


#include "usrloc.h"
#include "../../sr_module.h"


struct usrloc_func ul_func;


int bind_usrloc(void)
{
	ul_register_udomain = (register_udomain_t)find_export("~ul_register_udomain", 1);
	if (ul_register_udomain == 0) return -1;

	ul_insert_urecord = (insert_urecord_t)find_export("~ul_insert_urecord", 1);
	if (ul_insert_urecord == 0) return -1;

	ul_delete_urecord = (delete_urecord_t)find_export("~ul_delete_urecord", 1);
	if (ul_delete_urecord == 0) return -1;

	ul_get_urecord = (get_urecord_t)find_export("~ul_get_urecord", 1);
	if (ul_get_urecord == 0) return -1;

	ul_lock_udomain = (lock_udomain_t)find_export("~ul_lock_udomain", 1);
	if (ul_lock_udomain == 0) return -1;
	
	ul_unlock_udomain = (unlock_udomain_t)find_export("~ul_unlock_udomain", 1);
	if (ul_unlock_udomain == 0) return -1;


	ul_release_urecord = (release_urecord_t)find_export("~ul_release_urecord", 1);
	if (ul_release_urecord == 0) return -1;

	ul_insert_ucontact = (insert_ucontact_t)find_export("~ul_insert_ucontact", 1);
	if (ul_insert_ucontact == 0) return -1;

	ul_delete_ucontact = (delete_ucontact_t)find_export("~ul_delete_ucontact", 1);
	if (ul_delete_ucontact == 0) return -1;

	ul_get_ucontact = (get_ucontact_t)find_export("~ul_get_ucontact", 1);
	if (ul_get_ucontact == 0) return -1;


	ul_update_ucontact = (update_ucontact_t)find_export("~ul_update_ucontact", 1);
	if (ul_update_ucontact == 0) return -1;

	return 0;
}
