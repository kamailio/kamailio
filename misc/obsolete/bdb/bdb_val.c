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


int bdb_set_row(db_con_t* _h, bdb_urow_p u_r, bdb_val_p _v, bdb_row_p _r)
{
	bdb_val_p	v, nv;
	bdb_uval_p	uv;
	int		c_idx;
	int		found;

	/* filling data into row */
	for (v = _v, c_idx = 0; v != NULL; v = v->next, c_idx++) {
		nv = pkg_malloc(sizeof(*nv));
		memset(nv, 0, sizeof(*nv));
		bdb_push_field(_r, nv);

		found = 0;
		for (uv = u_r->fields; uv != NULL; uv = uv->next) {
			if (uv->c_idx == c_idx) {
				found = 1;
				break;
			}
		}

		if (found) {
#ifdef BDB_EXTRA_DEBUG
				LOG(L_NOTICE, "BDB:bdb_set_row: need to update column #%0d\n", c_idx);
#endif
			if (bdb_field_db2bdb(nv, &(uv->v)) < 0) {
				return -1;
			}
		} else {
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_set_row: no need to update column #%0d\n", c_idx);
#endif
			if (bdb_field_db2bdb(nv, &(v->v)) < 0) {
				return -1;
			}
		}

		bdb_push_data(_r, nv);
	};

	bdb_merge_tail(_r);

	return 0;
};


int bdb_row_db2bdb(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n, bdb_row_p *_r)
{
	bdb_row_p	r;
	bdb_table_p	t;
	bdb_column_p	c;

	int		found;
	int		use_key, found_key, key_idx;
	int		i;
	bdb_val_p	v;

	*_r = NULL;

	if ((t = bdb_find_table(CON_TABLE(_h))) == NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_row_db2bdb: table: no table in use\n");
#endif
		return -1;
	};

	key_idx = -1;
	use_key = -1;

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
			key_idx = i;
			use_key++;		/* set to 0 if used in clause only once */
                }
		if (!found) {
			LOG(L_ERR, "BDB:bdb_row_db2bdb: column '%s' does not exist\n", _k[i]);
			return -1;
		}
	}

	if (use_key < 0) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_row_db2bdb: primary key value must be supplied\n");
#endif
		return -1;
	}

	if (use_key > 0) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_row_db2bdb: primary key value must be supplied only once\n");
#endif
		return -1;
	}

	r = pkg_malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));

	/* filling data into row */
	for (c = t->cols; c != NULL; c = c->next) {
		v = pkg_malloc(sizeof(*v));
		memset(v, 0, sizeof(*v));
		VAL_NULL(&(v->v)) = 1;			/* default value is NULL */

		bdb_push_field(r, v);

		for (i = 0; i < _n; i++) {
			if (!strcmp(_k[i], c->name.s)) {
#ifdef BDB_EXTRA_DEBUG
				LOG(L_NOTICE, "BDB:bdb_row_db2bdb: filling column '%.*s'\n", c->name.len, c->name.s);
#endif
				if (bdb_field_db2bdb(v, &_v[i]) < 0) {
					bdb_free_row(r);
					return -1;
				};

				if (i == key_idx) {
					if (bdb_set_key(r, v) < 0) {
						bdb_free_row(r);
						return -1;
					};
				}

				break;
			}
		}

		bdb_push_data(r, v);
	};

	bdb_merge_tail(r);

	*_r = r;

	return 0;
};


void bdb_merge_tail(bdb_row_p _r)
{
	if (_r->tail.len > 0) {
		_r->data.data = pkg_realloc(_r->data.data, _r->data.size + _r->tail.len);
		memcpy(_r->data.data + _r->data.size, _r->tail.s, _r->tail.len);
		_r->data.size += _r->tail.len;
	}
};


void bdb_push_data(bdb_row_p _r, bdb_val_p _v)
{
	if (_r->data.size == 0) {
		_r->data.data = pkg_malloc(sizeof(*_v));
	} else {
		_r->data.data = pkg_realloc(_r->data.data, _r->data.size + sizeof(*_v));
	}

	memcpy(_r->data.data + _r->data.size, _v, sizeof(*_v));

	_r->data.size += sizeof(*_v);

	if (!VAL_NULL(&(_v->v))) {
		switch (VAL_TYPE(&(_v->v))) {
		case DB_STRING:
		case DB_STR:
		case DB_BLOB:
			if (_r->tail.len == 0) {
				_r->tail.s = pkg_malloc(VAL_STR(&(_v->v)).len);
			} else {
				_r->tail.s = pkg_realloc(_r->tail.s, _r->tail.len + VAL_STR(&(_v->v)).len);
			}
			memcpy(_r->tail.s + _r->tail.len, VAL_STR(&(_v->v)).s, VAL_STR(&(_v->v)).len);
			_r->tail.len += VAL_STR(&(_v->v)).len;
			break;
		default:
			break;
		}
	}
};


void bdb_push_field(bdb_row_p _r, bdb_val_p _v)
{
	bdb_val_p	f;

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


void bdb_free_field(bdb_val_p _v)
{
	if (!VAL_NULL(&(_v->v))) {
		if (VAL_TYPE(&(_v->v)) == DB_STR || VAL_TYPE(&(_v->v)) == DB_STRING ||
		    VAL_TYPE(&(_v->v)) == DB_BLOB) {
			pkg_free(VAL_STR(&(_v->v)).s);
		}
	}
	pkg_free(_v);
};


void bdb_free_field_list(bdb_val_p _v)
{
	bdb_val_p     curr, next;

	for (curr = _v; curr != NULL;) {
		next = curr->next;
		bdb_free_field(curr);
		curr = next;
	}
};


void bdb_free_row(bdb_row_p _r)
{
	if (_r->fields != NULL) {
		bdb_free_field_list(_r->fields);
	}

	if (_r->data.size > 0) {
		pkg_free(_r->data.data);
	}

	if (_r->tail.len > 0) {
		pkg_free(_r->tail.s);
	}

	pkg_free(_r);
};


void bdb_free_row_list(bdb_row_p _r)
{
	bdb_row_p     curr, next;

	for (curr = _r; curr != NULL;) {
		next = curr->next;
		bdb_free_row(curr);
		curr = next;
	}
};


int bdb_field_db2bdb(bdb_val_p v, db_val_t* _v)
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
			LOG(L_ERR, "BDB:bdb_field_db2bdb: unknown column type: %0X\n", VAL_TYPE(_v));
			return -1;
			break;
		}
	}
	return 0;
};


int bdb_get_db_row(db_con_t* _h, DBT* _data, bdb_val_p* _v)
{
	bdb_val_p v, prev;
	void *p, *tail;
	int l;

	if (!_data || !_data->size) {
		LOG(L_ERR, "BDB:bdb_get_db_row: invalid data\n");
		*_v = NULL;
		return -1;
	}

	*_v = (bdb_val_p)_data->data;
	prev = NULL;
	p = _data->data;
	l = 0;
	tail = p + sizeof(*v) * BDB_CON_COL_NUM(_h);

	while (l < sizeof(*v) * BDB_CON_COL_NUM(_h)) {
		v = (bdb_val_p)p;
		p += sizeof(*v);
		l += sizeof(*v);
		v->next = NULL;
		if (prev) {
			prev->next = v;
			prev = v;
		} else {
			prev = v;
		}
		if (!VAL_NULL(&(v->v))) {
			switch (VAL_TYPE(&(v->v))) {
			case DB_BLOB:
			case DB_STRING:
			case DB_STR:
				VAL_STR(&(v->v)).s = tail;
				tail += VAL_STR(&(v->v)).len;
				break;
			default:
				break;
			}
		}
	}

	return 0;
};

int bdb_set_key(bdb_row_p _r, bdb_val_p _v)
{
	/* NULL is not allowed for primary key */
	if (VAL_NULL(&(_v->v))) {
		LOG(L_ERR, "BDB:bdb_set_key: NULL is not allowed for primary key\n");
		return -1;
	}

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
		return -1;
		break;
	}

	return 0;
};
