/* 
 * $Id$
 *
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
		LOG(L_ERR, "put_on_ins_list(): No memory left");
		return -1;
	}

	p->expires = _c->expires;
	p->q = _c->q;
	p->cseq = _c->cseq;

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
	db_key_t keys[] = {user_col, contact_col, expires_col, q_col, callid_col, cseq_col};
	db_val_t vals[6];
	
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

		VAL_NULL(vals) = 0;
		VAL_NULL(vals + 1) = 0;
		VAL_NULL(vals + 2) = 0;
		VAL_NULL(vals + 3) = 0;
		VAL_NULL(vals + 4) = 0;
		VAL_NULL(vals + 5) = 0;
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

		if (db_insert(db, keys, vals, 6) < 0) {
			LOG(L_ERR, "process_ins_list(): Error while deleting from database\n");
			return -1;
		}

		pkg_free(p);
	}

	return 0;	
}
