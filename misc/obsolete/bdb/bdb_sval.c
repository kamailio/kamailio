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

int bdb_srow_db2bdb(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n, bdb_srow_p *_r)
{
	bdb_srow_p	r;
	bdb_table_p	t;
	bdb_column_p	c;

	int		found;
	int		use_key, found_key, key_idx;
	int		i;
	int		c_idx;
	bdb_sval_p	v;

	*_r = NULL;

	if ((t = bdb_find_table(CON_TABLE(_h))) == NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_srow_db2bdb: no table in use\n");
#endif
		return -1;
	};

	key_idx = -1;
	use_key = 0;

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
			/* use key only if it is used in clause and operator is '=' */
			if (!use_key && (_op == NULL || (_op != NULL && !strcmp(_op[i], OP_EQ)))) {
				key_idx = i;
				use_key = 1;
			}
		}
		if (!found) {
			LOG(L_ERR, "BDB:bdb_srow_db2bdb: column '%s' does not exist\n", _k[i]);
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
				LOG(L_NOTICE, "BDB:bdb_srow_db2bdb: filling column '%.*s', c_idx = %0d\n", c->name.len, c->name.s, c_idx);
#endif
				v = pkg_malloc(sizeof(*v));
				memset(v, 0, sizeof(*v));

				v->c_idx = c_idx;

				bdb_push_sfield(r, v);

				if (bdb_sfield_db2bdb(v, &_v[i], (!_op) ? NULL : _op[i]) < 0) {
					bdb_free_srow(r);
					return -1;
				};

				if (use_key && i == key_idx) {
					bdb_set_skey(r, v);
				}
			}
		}
	};

	*_r = r;

	return 0;
};


void bdb_free_srow(bdb_srow_p _r)
{
	if (_r->fields != NULL) {
		bdb_free_sfield_list(_r->fields);
	}

	pkg_free(_r);
};


void bdb_free_sfield(bdb_sval_p _v)
{
	if (!VAL_NULL(&(_v->v))) {
		if (VAL_TYPE(&(_v->v)) == DB_STR || VAL_TYPE(&(_v->v)) == DB_STRING ||
		    VAL_TYPE(&(_v->v)) == DB_BLOB) {
			pkg_free(VAL_STR(&(_v->v)).s);
                }
	}
	pkg_free(_v);
};


void bdb_free_sfield_list(bdb_sval_p _v)
{
	bdb_sval_p     curr, next;

	for (curr = _v; curr != NULL;) {
		next = curr->next;
		bdb_free_sfield(curr);
		curr = next;
	}
};


void bdb_push_sfield(bdb_srow_p _r, bdb_sval_p _v)
{
	bdb_sval_p	f;

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


void bdb_set_skey(bdb_srow_p _r, bdb_sval_p _v)
{
	/* NULL is not allowed for primary key */
	if (VAL_NULL(&(_v->v)))
		return;

	switch (VAL_TYPE(&(_v->v))) {
	case DB_INT:
		_r->key.data = &VAL_INT(&(_v->v));
		_r->key.size = sizeof(VAL_INT(&(_v->v)));
		break;
	case DB_FLOAT:
		_r->key.data = &VAL_FLOAT(&(_v->v));
		_r->key.size = sizeof(VAL_FLOAT(&(_v->v)));
		break;
	case DB_DATETIME:
		_r->key.data = &VAL_TIME(&(_v->v));
		_r->key.size = sizeof(VAL_TIME(&(_v->v)));
		break;
	case DB_BLOB:
		_r->key.data = VAL_BLOB(&(_v->v)).s;
		_r->key.size = VAL_BLOB(&(_v->v)).len;
		break;
	case DB_DOUBLE:
		_r->key.data = &VAL_DOUBLE(&(_v->v));
		_r->key.size = sizeof(VAL_DOUBLE(&(_v->v)));
		break;
	case DB_STRING:
		_r->key.data = (void *)VAL_STRING(&(_v->v));
		_r->key.size = strlen(VAL_STRING(&(_v->v))) + 1;
		break;
	case DB_STR:
		_r->key.data = VAL_STR(&(_v->v)).s;
		_r->key.size = VAL_STR(&(_v->v)).len;
		break;
	case DB_BITMAP:
		_r->key.data = &VAL_BITMAP(&(_v->v));
		_r->key.size = sizeof(VAL_BITMAP(&(_v->v)));
		break;
	default:
		LOG(L_ERR, "BDB:bdb_set_skey: unknown column type: %0X\n", VAL_TYPE(&(_v->v)));
		break;
	}

#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:bdb_set_skey: use key '%.*s' (%d bytes)\n", _r->key.size, (char *)_r->key.data, _r->key.size);
#endif
};


int bdb_sfield_db2bdb(bdb_sval_p v, db_val_t* _v, db_op_t _op)
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
			LOG(L_ERR, "BDB:bdb_sfield_db2bdb: unknown column type: %0X\n", VAL_TYPE(_v));
			return -1;
			break;
		}

		if (!_op || !strcmp(_op, OP_EQ)) {
			v->op = BDB_OP_EQ;
		} else if (!strcmp(_op, OP_LT)) {
			v->op = BDB_OP_LT;
		} else if (!strcmp(_op, OP_GT)) {
			v->op = BDB_OP_GT;
		} else if (!strcmp(_op, OP_LEQ)) {
			v->op = BDB_OP_LEQ;
		} else if (!strcmp(_op, OP_GEQ)) {
			v->op = BDB_OP_GEQ;
		} else {
			LOG(L_ERR, "BDB:sbdb_field_db2bdb: unknown operator: %s\n", _op);
			return -1;
		}
	}

	return 0;
};


