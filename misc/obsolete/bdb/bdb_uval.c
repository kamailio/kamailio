/* $Id$
 *
 * Copyright (C) 2006-2007 Sippy Software, Inc. <sales@sippysoft.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "bdb.h"

int bdb_urow_db2bdb(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n, bdb_urow_p *_r)
{
	bdb_urow_p	r;
	bdb_table_p	t;
	bdb_column_p	c;

	int		found, found_key;
	int		i, j;
	int		c_idx;
	bdb_uval_p	v;

	*_r = NULL;

	if (_n <= 0) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_urow_db2bdb: no defined keys to be updated\n");
#endif
		return -1;
	}

	if ((t = bdb_find_table(CON_TABLE(_h))) == NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_urow_db2bdb: no table in use\n");
#endif
		return -1;
	};

	/* check for dublicates in update set */
	for (i = 1; i < _n; i++) {
		for (j = 0; j < i; j++) {
			if (!strcmp(_k[i], _k[j])) {
#ifdef BDB_EXTRA_DEBUG
				LOG(L_ERR, "BDB:bdb_urow_db2bdb: dublicates keys in update set: '%s' and '%s'\n", _k[i], _k[j]);
#endif
				return -1;
			}
		}
	}

	/* check if all columns exist */
	for (i = 0; i < _n; i++) {
		found = 0;
		/* key column is always first one */
		for (c = t->cols, found_key = 1; c != NULL; c = c->next, found_key = 0) {
			if (!strcmp(_k[i], c->name.s)) {
				found = 1;
				break;
			}
		}
		if (found_key == 1) {
			LOG(L_ERR, "BDB:bdb_urow_db2bdb: unable to update primary key value\n");
			return -1;
		}
		if (!found) {
			LOG(L_ERR, "BDB:bdb_urow_db2bdb: column '%s' does not exist\n", _k[i]);
			return -1;
		}
	}

	r = pkg_malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));

	/* filling data into row */
	for (c = t->cols, c_idx = 0; c != NULL; c = c->next, c_idx++) {
		for (i = 0; i < _n; i++) {
			if (!strcmp(_k[i], c->name.s)) {
#ifdef BDB_EXTRA_DEBUG
				LOG(L_NOTICE, "BDB:bdb_urow_db2bdb: filling column '%.*s', c_idx = %0d\n", c->name.len, c->name.s, c_idx);
#endif
				v = pkg_malloc(sizeof(*v));
				memset(v, 0, sizeof(*v));

				v->c_idx = c_idx;

				bdb_push_ufield(r, v);

				if (bdb_ufield_db2bdb(v, &_v[i]) < 0) {
					bdb_free_urow(r);
					return -1;
				};
			}
		}
	};

	*_r = r;

	return 0;
};


void bdb_free_urow(bdb_urow_p _r)
{
	if (_r->fields != NULL) {
		bdb_free_ufield_list(_r->fields);
	}

	pkg_free(_r);
};


void bdb_free_ufield(bdb_uval_p _v)
{
	if (!VAL_NULL(&(_v->v))) {
		if (VAL_TYPE(&(_v->v)) == DB_STR || VAL_TYPE(&(_v->v)) == DB_STRING ||
		    VAL_TYPE(&(_v->v)) == DB_BLOB) {
			pkg_free(VAL_STR(&(_v->v)).s);
                }
	}
	pkg_free(_v);
};


void bdb_free_ufield_list(bdb_uval_p _v)
{
	bdb_uval_p     curr, next;

	for (curr = _v; curr != NULL;) {
		next = curr->next;
		bdb_free_ufield(curr);
		curr = next;
	}
};


void bdb_push_ufield(bdb_urow_p _r, bdb_uval_p _v)
{
	bdb_uval_p	f;

	if (_r->fields == NULL) {
		_r->fields = _v;
		return;
	}
	f = _r->fields;
	while (f->next != NULL) {
		f = f->next;
	}
	f->next = _v;
};


int bdb_ufield_db2bdb(bdb_uval_p v, db_val_t* _v)
{
	char	*s;

	VAL_NULL(&(v->v)) = VAL_NULL(_v);
	VAL_TYPE(&(v->v)) = VAL_TYPE(_v);

	if (!VAL_NULL(&(v->v))) {
		switch (VAL_TYPE(_v)) {
		case DB_INT:
			VAL_INT(&(v->v)) = VAL_INT(_v);
			break;
		case DB_FLOAT:
			VAL_FLOAT(&(v->v)) = VAL_FLOAT(_v);
			break;
		case DB_DATETIME:
			VAL_TIME(&(v->v)) = VAL_TIME(_v);
			break;
		case DB_BLOB:
			s = pkg_malloc(VAL_BLOB(_v).len);
			memcpy(s, VAL_BLOB(_v).s, VAL_BLOB(_v).len);
			VAL_BLOB(&(v->v)).s = s;
			VAL_BLOB(&(v->v)).len = VAL_BLOB(_v).len;
			break;
		case DB_DOUBLE:
			VAL_DOUBLE(&(v->v)) = VAL_DOUBLE(_v);
			break;
		case DB_STRING:
			VAL_STR(&(v->v)).len = strlen(VAL_STRING(_v)) + 1;
			s = pkg_malloc(VAL_STR(&(v->v)).len);
			strcpy(s, VAL_STRING(_v));
			VAL_STRING(&(v->v)) = s;
			break;
		case DB_STR:
			s = pkg_malloc(VAL_STR(_v).len);
			memcpy(s, VAL_STR(_v).s, VAL_STR(_v).len);
			VAL_STR(&(v->v)).s = s;
			VAL_STR(&(v->v)).len = VAL_STR(_v).len;
			break;
		case DB_BITMAP:
			VAL_BITMAP(&(v->v)) = VAL_BITMAP(_v);
			break;
		default:
			LOG(L_ERR, "BDB:bdb_ufield_db2bdb: unknown column type: %0X\n", VAL_TYPE(_v));
			return -1;
			break;
		}
	}

	return 0;
};


