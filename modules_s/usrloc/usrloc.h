/*
 * $Id$
 *
 * Convenience usrloc header file
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


#ifndef USRLOC_H
#define USRLOC_H


#include "dlist.h"
#include "udomain.h"
#include "urecord.h"
#include "ucontact.h"


/* dlist.h interface */
typedef int  (*register_udomain_t) (const char* _n, udomain_t** _d);

/* udomain.h interface */
typedef int  (*insert_urecord_t)   (udomain_t* _d, str* _aor, struct urecord** _r);
typedef int  (*delete_urecord_t)   (udomain_t* _d, str* _a);
typedef int  (*get_urecord_t)      (udomain_t* _d, str* _a, struct urecord** _r);
typedef void (*lock_udomain_t)     (udomain_t* _d);
typedef void (*unlock_udomain_t)   (udomain_t* _d);


/* urecord.h interface */
typedef void (*release_urecord_t)  (urecord_t* _r);
typedef int  (*insert_ucontact_t)  (urecord_t* _r, str* _c, time_t _e, float _q, str* _cid, int _cs,
				    unsigned int _flags, struct ucontact** _con);
typedef int  (*delete_ucontact_t)  (urecord_t* _r, struct ucontact* _c);
typedef int  (*get_ucontact_t)     (urecord_t* _r, str* _c, struct ucontact** _co);

/* ucontact.h interface */
typedef int  (*update_ucontact_t)  (ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs, 
				    unsigned int _set, unsigned int _res);


struct usrloc_func {
	register_udomain_t register_udomain_f;

	insert_urecord_t   insert_urecord_f;
	delete_urecord_t   delete_urecord_f;
	get_urecord_t      get_urecord_f;
	lock_udomain_t     lock_udomain_f;
	unlock_udomain_t   unlock_udomain_f;

	release_urecord_t  release_urecord_f;
	insert_ucontact_t  insert_ucontact_f;
	delete_ucontact_t  delete_ucontact_f;
	get_ucontact_t     get_ucontact_f;

	update_ucontact_t  update_ucontact_f;
};


extern struct usrloc_func ul_func;

#define ul_register_udomain (ul_func.register_udomain_f)

#define ul_insert_urecord   (ul_func.insert_urecord_f)
#define ul_delete_urecord   (ul_func.delete_urecord_f)
#define ul_get_urecord      (ul_func.get_urecord_f)
#define ul_lock_udomain     (ul_func.lock_udomain_f)
#define ul_unlock_udomain   (ul_func.unlock_udomain_f)

#define ul_release_urecord  (ul_func.release_urecord_f)
#define ul_insert_ucontact  (ul_func.insert_ucontact_f)
#define ul_delete_ucontact  (ul_func.delete_ucontact_f)
#define ul_get_ucontact     (ul_func.get_ucontact_f)

#define ul_update_ucontact  (ul_func.update_ucontact_f)

int bind_usrloc(void);

#endif /* USRLOC_H */
