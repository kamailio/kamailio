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
 *
 * History:
 * ---------
 * 2003-03-12 added replicate mark and storing of state (nils)
 */



#include "ins_list.h"
#include "../../mem/mem.h"
#include <string.h>
#include "../../dprint.h"
#include "../../db/db.h"
#include "ul_mod.h"


static struct ins_itm* ins_root = 0;


int put_on_ins_list(struct ucontact* _c)
{
	struct ins_itm* p;

	p = (struct ins_itm*)pkg_malloc(sizeof(struct ins_itm) + _c->callid.len);
	if (p == 0) {
		LOG(L_ERR, "put_on_ins_list(): No memory left\n");
		return -1;
	}

	p->expires = _c->expires;
	p->q = _c->q;
	p->cseq = _c->cseq;
	p->replicate = _c->replicate;
	p->state = _c->state;

	p->user = _c->aor;
	p->cont = &_c->c;
	p->cid_len = _c->callid.len;

	memcpy(p->callid, _c->callid.s, p->cid_len);

	p->next = ins_root;
	ins_root = p;
	return 0;
}


int process_ins_list(str* _d)
{
	struct ins_itm* p;
	char b[256];
	db_key_t keys[] = {user_col, contact_col, expires_col, q_col, callid_col, cseq_col, replicate_col, state_col};
	db_val_t vals[8];

	keys[0] = user_col;
	keys[1] = contact_col;
	keys[2] = expires_col;
	keys[3] = q_col;
	keys[4] = callid_col;
	keys[5] = cseq_col;
	keys[6] = replicate_col;
	keys[7] = state_col;
	
	if (ins_root) {
	     /* FIXME */
		memcpy(b, _d->s, _d->len);
		b[_d->len] = '\0';
		db_use_table(db, b);
	
		VAL_TYPE(vals) = DB_STR;
		VAL_TYPE(vals + 1) = DB_STR;
		VAL_TYPE(vals + 2) = DB_DATETIME;
		VAL_TYPE(vals + 3) = DB_DOUBLE;
		VAL_TYPE(vals + 4) = DB_STR;
		VAL_TYPE(vals + 5) = DB_INT;
		VAL_TYPE(vals + 6) = DB_INT;
		VAL_TYPE(vals + 7) = DB_INT;

		VAL_NULL(vals) = 0;
		VAL_NULL(vals + 1) = 0;
		VAL_NULL(vals + 2) = 0;
		VAL_NULL(vals + 3) = 0;
		VAL_NULL(vals + 4) = 0;
		VAL_NULL(vals + 5) = 0;
		VAL_NULL(vals + 6) = 0;
		VAL_NULL(vals + 7) = 0;
	}

	while(ins_root) {
		p = ins_root;
		ins_root = ins_root->next;

		VAL_STR(vals).len = p->user->len;
		VAL_STR(vals).s = p->user->s;
		
		VAL_STR(vals + 1).len = p->cont->len;
		VAL_STR(vals + 1).s = p->cont->s;

		VAL_TIME(vals + 2) = p->expires;
		VAL_DOUBLE(vals + 3) = p->q;
		
		VAL_STR(vals + 4).len = p->cid_len;
		VAL_STR(vals + 4).s = p->callid;

		VAL_INT(vals + 5) = p->cseq;

		VAL_INT(vals + 6) = p->replicate;

		VAL_INT(vals + 7) = p->state;

		if (db_insert(db, keys, vals, 8) < 0) {
			LOG(L_ERR, "process_ins_list(): Error while inserting into database\n");
			return -1;
		}

		pkg_free(p);
	}

	return 0;	
}
