/* 
 * $Id$
 *
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


#include "del_list.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include <string.h>
#include "../../db/db.h"
#include "ul_mod.h"


static struct del_itm* del_root = 0;


int put_on_del_list(struct ucontact* _c)
{
	struct del_itm* p;

	p = (struct del_itm*)pkg_malloc(sizeof(struct del_itm) + _c->aor->len + _c->c.len);
	if (p == 0) {
		LOG(L_ERR, "put_on_del_list(): No memory left");
		return -1;
	}

	p->user_len = _c->aor->len;
	p->cont_len = _c->c.len;

	memcpy(p->tail, _c->aor->s, p->user_len);
	memcpy(p->tail + p->user_len, _c->c.s, p->cont_len);

	p->next = del_root;
	del_root = p;
	return 0;
}


int process_del_list(str* _d)
{
	struct del_itm* p;
	char b[256];
	db_key_t keys[2] = {user_col, contact_col};
	db_val_t vals[2];
	
	if (del_root) {
	     /* FIXME */
		memcpy(b, _d->s, _d->len);
		b[_d->len] = '\0';
		db_use_table(db, b);
	
		VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
		VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	}

	while(del_root) {
		p = del_root;
		del_root = del_root->next;

		VAL_STR(vals).len = p->user_len;
		VAL_STR(vals).s = p->tail;
		
		VAL_STR(vals + 1).len = p->cont_len;
		VAL_STR(vals + 1).s = p->tail + p->user_len;

		if (db_delete(db, keys, 0, vals, 2) < 0) {
			LOG(L_ERR, "process_del_list(): Error while deleting from database\n");
			return -1;
		}

		pkg_free(p);
	}

	return 0;
}
