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


int bdb_close_table(db_con_t* _h)
{
	/* close DB */
	if (BDB_CON_DB(_h) == NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_close_table: no need to close\n");
#endif
		return 0;
#ifdef BDB_EXTRA_DEBUG
	} else {
		LOG(L_NOTICE, "BDB:bdb_close_table: '%s'\n", CON_TABLE(_h));
#endif
	};
	BDB_CON_DB(_h)->close(BDB_CON_DB(_h), 0);

	CON_TABLE(_h) = NULL;

	BDB_CON_DB(_h) = NULL;

	return 0;
};


int bdb_open_table(db_con_t* _h, const char* _t)
{
	int ret;
	bdb_table_p t;

	if ((t = bdb_find_table(_t)) == NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_open_table: table: '%s' has not been described\n", _t);
#endif
		return -1;
	}
	BDB_CON_COL_NUM(_h) = t->col_num;

	CON_TABLE(_h) = t->name.s;
#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_open_table: '%s'\n", CON_TABLE(_h));
#endif

	ret = db_create(&BDB_CON_DB(_h), BDB_CON_DBENV(_h), 0);
	if (ret != 0) {
                LOG(L_ERR, "BDB:bdb_open_table: unable to db_create(): %s\n", db_strerror(ret));
                return -1;
	};

	ret = BDB_CON_DB(_h)->set_flags(BDB_CON_DB(_h), DB_DUP);
	if (ret != 0) {
                LOG(L_ERR, "BDB:bdb_open_table: unable to set_flags(): %s\n", db_strerror(ret));
                return -1;
	}


	ret = BDB_CON_DB(_h)->open(BDB_CON_DB(_h), NULL, CON_TABLE(_h), NULL, DB_BTREE, DB_CREATE, 0);
	if (ret != 0) {
                LOG(L_ERR, "BDB:bdb_open_table: unable to open database '%s': %s\n", CON_TABLE(_h), db_strerror(ret));
                return -1;
	};

	return 0;
};


int bdb_use_table(db_con_t* _h, const char* _t)
{
	int ret;

	if ((!_h) || (!_t)) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_use_table: Invalid parameter value\n");
#endif
		return -1;
	}

	if (CON_TABLE(_h) != NULL && !strcmp(CON_TABLE(_h), _t)) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_use_table: table '%s' has been already opened\n", _t);
#endif
		return 0;
	}

	/* close table if one was already opened */
	if (CON_TABLE(_h) != NULL) {
		bdb_close_table(_h);
	}

	ret = bdb_open_table(_h, _t);

	return ret;
}

int bdb_describe_table(modparam_t type, void* val)
{
	char *s, *p;
	bdb_table_p t;
	bdb_column_p c;

#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_describe_table: input string: '%s'\n", (char*)val);
#endif
	s = (char*) val;

	p = strchr(s, ':');
	*p = 0;
	if (bdb_find_table(s) != NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_describe_table: table: '%s' already has been described\n", s);
#endif
		return -1;
	}

	t = pkg_malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));

	t->name.s = pkg_malloc(strlen(s) + 1);
	memcpy(t->name.s, s, strlen(s) + 1);
	t->name.len = strlen(s) + 1;

#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_describe_table: table: '%.*s'\n", t->name.len, t->name.s);
#endif
	bdb_push_table(t);

	s = p + 1;
	while ((p = strchr(s, '(')) != NULL) {
		*p = 0;
		if (bdb_find_column(t, s) != NULL) {
#ifdef BDB_EXTRA_DEBUG
			LOG(L_ERR, "BDB:bdb_describe_table: table: '%.*s': dublicated column: '%s', \n",
			    t->name.len, t->name.s, s);
#endif
			return -1;
		}

		c = pkg_malloc(sizeof(*c));
		memset(c, 0, sizeof(*c));

		c->name.s = pkg_malloc(strlen(s) + 1);
		memcpy(c->name.s, s, strlen(s) + 1);
		c->name.len = strlen(s) + 1;

#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_describe_table: column '%.*s'", c->name.len, c->name.s);
#endif
		bdb_push_column(t, c);
		t->col_num++;

		s = ++p;
		p = strchr(s, ')');
		*p = 0;
#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, ", type: '%s'\n", s);
#endif
		if (!strncmp("int", s, strlen(s))) {
			c->type = DB_INT;
		} else if (!strncmp("float", s, strlen(s))) {
			c->type = DB_FLOAT;
		} else if (!strncmp("double", s, strlen(s))) {
			c->type = DB_DOUBLE;
		} else if (!strncmp("string", s, strlen(s))) {
			c->type = DB_STRING;
		} else if (!strncmp("str", s, strlen(s))) {
			c->type = DB_STR;
		} else if (!strncmp("datetime", s, strlen(s))) {
			c->type = DB_DATETIME;
		} else if (!strncmp("blob", s, strlen(s))) {
			c->type = DB_BLOB;
		} else if (!strncmp("bitmap", s, strlen(s))) {
			c->type = DB_BITMAP;
		} else {
#ifdef BDB_EXTRA_DEBUG
			LOG(L_ERR, "BDB:bdb_describe_table: bad column type: '%s'\n", s);
#endif
			return -1;
		}

		s = ++p;
		if ((p = strchr(s, ' ')) != NULL)
			s++;
	}

	return 0;
};

void bdb_push_table(bdb_table_p _t)
{
	bdb_table_p     t;

	if (bdb_tables == NULL) {
		bdb_tables = _t;
		return;
	}
	t = bdb_tables;
	while (t->next != NULL) {
		t = t->next;
	}
	t->next = _t;
};

void bdb_free_table(bdb_table_p _t)
{
	if (_t->name.s) {
		pkg_free(_t->name.s);
	}
	if (_t->cols != NULL) {
		bdb_free_column_list(_t->cols);
	}
	pkg_free(_t);
};

void bdb_free_table_list(bdb_table_p _t)
{
	bdb_table_p     curr, next;

#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:bdb_free_table_list\n");
#endif
	for (curr = _t; curr != NULL;) {
		next = curr->next;
		bdb_free_table(curr);
		curr = next;
	}
};

bdb_table_p bdb_find_table(const char* _t)
{
	bdb_table_p	t;

	for (t = bdb_tables; t != NULL; t = t->next) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_NOTICE, "BDB:bdb_find_table: search for '%s', found '%s'\n", _t, t->name.s);
#endif
		if (!strcmp(_t, t->name.s))
			return t;
	}

	return NULL;
};

void bdb_free_column(bdb_column_p _c)
{
	if (_c->name.s) {
		pkg_free(_c->name.s);
	}
	pkg_free(_c);
};

void bdb_free_column_list(bdb_column_p _c)
{
	bdb_column_p     curr, next;

	for (curr = _c; curr != NULL;) {
		next = curr->next;
		bdb_free_column(curr);
		curr = next;
	}
};

bdb_column_p bdb_find_column(bdb_table_p _t, const char* _c)
{
	bdb_column_p	c;

	for (c = _t->cols; c != NULL; c = c->next) {
		if (!strcmp(_c, c->name.s))
			return c;
	}

	return NULL;
};

void bdb_push_column(bdb_table_p _t, bdb_column_p _c)
{
	bdb_column_p	c;

	if (_t->cols == NULL) {
		_t->cols = _c;
		return;
	}
	c = _t->cols;
	while (c->next != NULL) {
		c = c->next;
	}
	c->next = _c;
};


int bdb_update_table(db_con_t* _h, bdb_srow_p s_r, bdb_urow_p u_r)
{
	DBC	*cursorp;
	DBT	key, *keyp, data;
	u_int32_t flags, nflags;
	bdb_val_p v;
	bdb_row_p r;
	int	ret;

	if (s_r->key.size > 0) {
		keyp = &(s_r->key);
		flags = DB_SET;
		nflags = DB_NEXT_DUP;
	} else {
		memset(&key, 0, sizeof(DBT));
		keyp = &key;
		flags = DB_NEXT;
		nflags = DB_NEXT;
	}
	memset(&data, 0, sizeof(DBT));
	r = pkg_malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));

	BDB_CON_DB(_h)->cursor(BDB_CON_DB(_h), NULL, &cursorp, DB_WRITECURSOR);

	ret = cursorp->c_get(cursorp, keyp, &data, flags);
	while (ret == 0) {
		if ((ret = bdb_get_db_row(_h, &data, &v)) < 0) {
			if (cursorp != NULL)
				cursorp->c_close(cursorp);
			bdb_free_row(r);
			return -1;
		};

		/* row content now in v */

		ret = bdb_row_match(_h, v, s_r);
		if (ret < 0) {
			if (cursorp != NULL)
				cursorp->c_close(cursorp);
			bdb_free_row(r);
			return -1;
		} else if (ret) {		/* match */
			if (bdb_set_row(_h, u_r, v, r) < 0) {
				if (cursorp != NULL)
					cursorp->c_close(cursorp);
				bdb_free_row(r);
				return -1;
			};

			ret = cursorp->c_put(cursorp, keyp, &(r->data), DB_CURRENT);
			if (ret != 0) {
                		LOG(L_ERR, "BDB:bdb_update_table: c_put(): %s\n", db_strerror(ret));
				if (cursorp != NULL)
					cursorp->c_close(cursorp);
				bdb_free_row(r);
				return -1;
			}

			if (r->data.data != NULL) {
				pkg_free(r->data.data);
			}
			if (r->tail.s != NULL) {
				pkg_free(r->tail.s);
			}
			if (r->fields != NULL) {
				bdb_free_field_list(r->fields);
			}
			memset(r, 0, sizeof(*r));
#ifdef BDB_EXTRA_DEBUG
		} else {
			LOG(L_NOTICE, "BDB:bdb_update_table: does not match\n");
#endif
		};

		ret = cursorp->c_get(cursorp, keyp, &data, nflags);
	}

	if (ret != DB_NOTFOUND) {
                LOG(L_ERR, "BDB:bdb_update_table: %s\n", db_strerror(ret));
		if (cursorp != NULL)
			cursorp->c_close(cursorp);
		bdb_free_row(r);
		return -1;
	}

	if (cursorp != NULL)
		cursorp->c_close(cursorp);
	bdb_free_row(r);

	return 0;
};


int bdb_query_table(db_con_t* _h, bdb_srow_p s_r, bdb_rrow_p r_r, int _n, db_res_t** _r)
{
	bdb_table_p t;
	bdb_column_p c;
	int i, j;

	DBC	*cursorp;
	DBT	key, *keyp, data;
	u_int32_t flags, nflags;
	bdb_val_p v;
	db_res_t *res;
	int	ret;

	if (s_r->key.size > 0) {
		keyp = &(s_r->key);
		flags = DB_SET;
		nflags = DB_NEXT_DUP;
	} else {
		memset(&key, 0, sizeof(DBT));
		keyp = &key;
		flags = DB_NEXT;
		nflags = DB_NEXT;
	}
	memset(&data, 0, sizeof(DBT));

	/* prepare result */
	res = pkg_malloc(sizeof(*res));
	memset(res, 0, sizeof(*res));
	*_r = res;

	res->col.n = (_n == 0) ? BDB_CON_COL_NUM(_h) : _n;

	t = bdb_find_table(CON_TABLE(_h));
	if (_n == 0) {			/* return all columns */
		res->col.names = pkg_malloc(sizeof(db_key_t) * t->col_num);
		res->col.types = pkg_malloc(sizeof(db_type_t) * t->col_num);
		for (c = t->cols, i = 0; c != NULL; c = c->next, i++) {
			res->col.names[i] = pkg_malloc(c->name.len);
			memcpy((void *)res->col.names[i], (void *)c->name.s, c->name.len);
			res->col.types[i] = c->type;
		}
	} else {
		res->col.names = pkg_malloc(sizeof(db_key_t) * _n);
		res->col.types = pkg_malloc(sizeof(db_type_t) * _n);
		for (i = 0; i < _n; i++) {
			for (c = t->cols, j = 0; j < r_r[i]; c = c->next, j++);
			res->col.names[i] = pkg_malloc(c->name.len);
			memcpy((void *)res->col.names[i], (void *)c->name.s, c->name.len);
			res->col.types[i] = c->type;
		}
	}

	BDB_CON_DB(_h)->cursor(BDB_CON_DB(_h), NULL, &cursorp, 0);

	ret = cursorp->c_get(cursorp, keyp, &data, flags);
	while (ret == 0) {
		if ((ret = bdb_get_db_row(_h, &data, &v)) < 0) {
			if (cursorp != NULL)
				cursorp->c_close(cursorp);
			bdb_free_result(_h, *_r);
			return -1;
		};

		/* row content now in v */

		ret = bdb_row_match(_h, v, s_r);
		if (ret < 0) {
			if (cursorp != NULL)
				cursorp->c_close(cursorp);
			bdb_free_result(_h, *_r);
			return -1;
		} else if (ret) {		/* match */
			if (bdb_push_res_row(_h, _r, r_r, _n, v) < 0) {
				if (cursorp != NULL)
					cursorp->c_close(cursorp);
				bdb_free_result(_h, *_r);
				return -1;
			};
#ifdef BDB_EXTRA_DEBUG
		} else {
			LOG(L_NOTICE, "BDB:bdb_query_table: does not match\n");
#endif
		};

		ret = cursorp->c_get(cursorp, keyp, &data, nflags);
	}

	if (ret != DB_NOTFOUND) {
                LOG(L_ERR, "BDB:bdb_query_table: %s\n", db_strerror(ret));
		if (cursorp != NULL)
			cursorp->c_close(cursorp);
		bdb_free_result(_h, *_r);
		return -1;
	}

	if (cursorp != NULL)
		cursorp->c_close(cursorp);

	return 0;
}


int bdb_row_match(db_con_t* _h, bdb_val_p _v, bdb_srow_p s_r)
{
	bdb_sval_p s_v;
	db_val_t *v, *v2;
	int op, l;

	s_v = s_r->fields;
	while (s_v != NULL) {
		v = &(s_v->v);			/* row field value*/
		v2 = &(_v[s_v->c_idx].v);	/* compared value */
		op = s_v->op;			/* expression operator */

		if (VAL_TYPE(v) != VAL_TYPE(v2)) {
                	LOG(L_ERR, "BDB:bdb_row_match: types mismatch: %d vs %d\n", VAL_TYPE(v), VAL_TYPE(v2));
			return -1;
		};

		if (VAL_NULL(v) && VAL_NULL(v) == VAL_NULL(v2)) {
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: NULL == NULL\n");
#endif
			return 1;
		};

		if (VAL_NULL(v) != VAL_NULL(v2)) {
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: NULL != NULL\n");
#endif
			return 0;
		};

		switch (VAL_TYPE(v)) {
		case DB_INT:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %d vs %d\n", VAL_INT(v), VAL_INT(v2));
#endif
			switch (op) {
			case BDB_OP_EQ:
				if (VAL_INT(v) != VAL_INT(v2)) return 0;
				break;
			case BDB_OP_LT:
				if (VAL_INT(v) >= VAL_INT(v2)) return 0;
				break;
			case BDB_OP_GT:
				if (VAL_INT(v) <= VAL_INT(v2)) return 0;
				break;
			case BDB_OP_LEQ:
				if (VAL_INT(v) > VAL_INT(v2)) return 0;
				break;
			case BDB_OP_GEQ:
				if (VAL_INT(v) < VAL_INT(v2)) return 0;
				break;
			}
			break;
		case DB_FLOAT:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %f vs %f\n", VAL_FLOAT(v), VAL_FLOAT(v2));
#endif
			switch (op) {
			case BDB_OP_EQ:
				if (VAL_FLOAT(v) != VAL_FLOAT(v2)) return 0;
				break;
			case BDB_OP_LT:
				if (VAL_FLOAT(v) >= VAL_FLOAT(v2)) return 0;
				break;
			case BDB_OP_GT:
				if (VAL_FLOAT(v) <= VAL_FLOAT(v2)) return 0;
				break;
			case BDB_OP_LEQ:
				if (VAL_FLOAT(v) > VAL_FLOAT(v2)) return 0;
				break;
			case BDB_OP_GEQ:
				if (VAL_FLOAT(v) < VAL_FLOAT(v2)) return 0;
				break;
			}
			break;
		case DB_DATETIME:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %d vs %d\n", VAL_TIME(v), VAL_TIME(v2));
#endif
			switch (op) {
			case BDB_OP_EQ:
				if (VAL_TIME(v) != VAL_TIME(v2)) return 0;
				break;
			case BDB_OP_LT:
				if (VAL_TIME(v) >= VAL_TIME(v2)) return 0;
				break;
			case BDB_OP_GT:
				if (VAL_TIME(v) <= VAL_TIME(v2)) return 0;
				break;
			case BDB_OP_LEQ:
				if (VAL_TIME(v) > VAL_TIME(v2)) return 0;
				break;
			case BDB_OP_GEQ:
				if (VAL_TIME(v) < VAL_TIME(v2)) return 0;
				break;
			}
			break;
		case DB_DOUBLE:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %f vs %f\n", VAL_DOUBLE(v), VAL_DOUBLE(v2));
#endif
			switch (op) {
			case BDB_OP_EQ:
				if (VAL_DOUBLE(v) != VAL_DOUBLE(v2)) return 0;
				break;
			case BDB_OP_LT:
				if (VAL_DOUBLE(v) >= VAL_DOUBLE(v2)) return 0;
				break;
			case BDB_OP_GT:
				if (VAL_DOUBLE(v) <= VAL_DOUBLE(v2)) return 0;
				break;
			case BDB_OP_LEQ:
				if (VAL_DOUBLE(v) > VAL_DOUBLE(v2)) return 0;
				break;
			case BDB_OP_GEQ:
				if (VAL_DOUBLE(v) < VAL_DOUBLE(v2)) return 0;
				break;
			}
			break;
		case DB_BITMAP:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %0X vs %0X\n", VAL_BITMAP(v), VAL_BITMAP(v2));
#endif
			switch (op) {
			case BDB_OP_EQ:
				if (VAL_BITMAP(v) != VAL_BITMAP(v2)) return 0;
				break;
			case BDB_OP_LT:
				if (VAL_BITMAP(v) >= VAL_BITMAP(v2)) return 0;
				break;
			case BDB_OP_GT:
				if (VAL_BITMAP(v) <= VAL_BITMAP(v2)) return 0;
				break;
			case BDB_OP_LEQ:
				if (VAL_BITMAP(v) > VAL_BITMAP(v2)) return 0;
				break;
			case BDB_OP_GEQ:
				if (VAL_BITMAP(v) < VAL_BITMAP(v2)) return 0;
				break;
			}
			break;
		case DB_STRING:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %s vs %s\n", VAL_STRING(v), VAL_STRING(v2));
#endif
			switch (op) {
			case BDB_OP_EQ:
				if (strcmp(VAL_STRING(v), VAL_STRING(v2))) return 0;
				break;
			case BDB_OP_LT:
				if (strcmp(VAL_STRING(v), VAL_STRING(v2)) >= 0) return 0;
				break;
			case BDB_OP_GT:
				if (strcmp(VAL_STRING(v), VAL_STRING(v2)) <= 0) return 0;
				break;
			case BDB_OP_LEQ:
				if (strcmp(VAL_STRING(v), VAL_STRING(v2)) > 0) return 0;
				break;
			case BDB_OP_GEQ:
				if (strcmp(VAL_STRING(v), VAL_STRING(v2)) < 0) return 0;
				break;
			}
			break;
		case DB_STR:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %.*s vs %.*s\n", VAL_STR(v).len, VAL_STR(v).s, VAL_STR(v2).len, VAL_STR(v2).s);
#endif
			l = VAL_STR(v).len > VAL_STR(v2).len ? VAL_STR(v).len : VAL_STR(v2).len;
			switch (op) {
			case BDB_OP_EQ:
				if (strncmp(VAL_STR(v).s, VAL_STR(v2).s, l)) return 0;
				break;
			case BDB_OP_LT:
				if (strncmp(VAL_STR(v).s, VAL_STR(v2).s, l) >= 0) return 0;
				break;
			case BDB_OP_GT:
				if (strncmp(VAL_STR(v).s, VAL_STR(v2).s, l) <= 0) return 0;
				break;
			case BDB_OP_LEQ:
				if (strncmp(VAL_STR(v).s, VAL_STR(v2).s, l) > 0) return 0;
				break;
			case BDB_OP_GEQ:
				if (strncmp(VAL_STR(v).s, VAL_STR(v2).s, l) < 0) return 0;
				break;
			}
			break;
		case DB_BLOB:
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_row_match: %.*s (len = %d) vs %.*s (len = %d)\n", VAL_BLOB(v).len, VAL_BLOB(v).s, VAL_BLOB(v).len, VAL_BLOB(v2).len, VAL_BLOB(v2).s, VAL_BLOB(v2).len);
#endif
			l = VAL_BLOB(v).len > VAL_BLOB(v2).len ? VAL_BLOB(v).len : VAL_BLOB(v2).len;
			switch (op) {
			case BDB_OP_EQ:
				if (memcmp(VAL_BLOB(v).s, VAL_BLOB(v2).s, l)) return 0;
				break;
			case BDB_OP_LT:
				if (memcmp(VAL_BLOB(v).s, VAL_BLOB(v2).s, l) >= 0) return 0;
				break;
			case BDB_OP_GT:
				if (memcmp(VAL_BLOB(v).s, VAL_BLOB(v2).s, l) <= 0) return 0;
				break;
			case BDB_OP_LEQ:
				if (memcmp(VAL_BLOB(v).s, VAL_BLOB(v2).s, l) > 0) return 0;
				break;
			case BDB_OP_GEQ:
				if (memcmp(VAL_BLOB(v).s, VAL_BLOB(v2).s, l) < 0) return 0;
				break;
			}
			break;
		default:
			return -1;		/* is it possible here? */
			break;
		}

		s_v = s_v->next;
	}

#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:bdb_row_match: match\n");
#endif
	return 1;	/* match */
};


int bdb_push_res_row(db_con_t* _h, db_res_t** _r, bdb_rrow_p _r_r, int _n, bdb_val_p _v)
{
	db_res_t *r;
	db_row_t *row;
	db_val_t *v;
	int	i, n;
	char	*s;
	
	r = *_r;

	/*
	 * use system malloc() to allocate memory for RES_ROWS array
	 * due to pkg_malloc() memory pool fragmentation problem
	 */
	if (RES_ROW_N(r) == 0) {
		if ((row = malloc(sizeof(*(RES_ROWS(r))))) == NULL) {
                	LOG(L_ERR, "BDB:bdb_push_res_row: unable to allocate %d bytes\n",
			    sizeof(*(RES_ROWS(r))));
			return -1;
		};
	} else {
		if ((row = realloc(RES_ROWS(r), sizeof(*(RES_ROWS(r))) * (RES_ROW_N(r) + 1))) == NULL) {
                	LOG(L_ERR, "BDB:bdb_push_res_row: unable to reallocate %d bytes\n",
			    sizeof(*(RES_ROWS(r))) * (RES_ROW_N(r) + 1));
			return -1;
		};
	}
	RES_ROWS(r) = row;
	row = &RES_ROWS(r)[RES_ROW_N(r)];
	RES_ROW_N(r)++;

	n = (_n == 0) ? BDB_CON_COL_NUM(_h) : _n;
	ROW_VALUES(row) = malloc(sizeof(*(ROW_VALUES(row))) * n);
	if (ROW_VALUES(row) == NULL) {
		LOG(L_ERR, "BDB:bdb_push_res_row: unable to allocate %d bytes for ROW_VALUES\n",
		    sizeof(*(ROW_VALUES(row))) * n);
		return -1;
	}

	for (i = 0; i < n; i++) {
		v = &ROW_VALUES(row)[i];

		memcpy(v, &_v[_r_r[i]].v, sizeof(*v));
		if (VAL_TYPE(v) == DB_STRING || VAL_TYPE(v) == DB_STR || VAL_TYPE(v) == DB_BLOB) {
			s = malloc(VAL_STR(v).len + 1);
			if (s == NULL) {
				LOG(L_ERR, "BDB:bdb_push_res_row: unable to allocate %d bytes for VAL_STR\n",
				    VAL_STR(v).len);
				free(ROW_VALUES(row));
				return -1;
			}
			memcpy(s, VAL_STR(v).s, VAL_STR(v).len);
			/* some code expect STR value to be NULL terminated */
			s[VAL_STR(v).len] = 0; 
			VAL_STR(v).s = s;
		}
	}

	ROW_N(row) = n;

	return 0;
};


int bdb_delete_table(db_con_t* _h, bdb_srow_p s_r)
{
	DBC	*cursorp;
	DBT	key, *keyp, data;
	u_int32_t flags, nflags;
	bdb_val_p v;
	int	ret;

	if (s_r->key.size > 0) {
		keyp = &(s_r->key);
		flags = DB_SET;
		nflags = DB_NEXT_DUP;
	} else {
		memset(&key, 0, sizeof(DBT));
		keyp = &key;
		flags = DB_NEXT;
		nflags = DB_NEXT;
	}
	memset(&data, 0, sizeof(DBT));

	BDB_CON_DB(_h)->cursor(BDB_CON_DB(_h), NULL, &cursorp, DB_WRITECURSOR);

	ret = cursorp->c_get(cursorp, keyp, &data, flags);
	while (ret == 0) {
		if ((ret = bdb_get_db_row(_h, &data, &v)) < 0) {
			return -1;
		};

		/* row content now in v */

		ret = bdb_row_match(_h, v, s_r);
		if (ret < 0) {
			if (cursorp != NULL)
				cursorp->c_close(cursorp);
			return -1;
		} else if (ret) {		/* match */
			ret = cursorp->c_del(cursorp, 0);
			if (ret != 0) {
                		LOG(L_ERR, "BDB:bdb_delete_table: c_del(): %s\n", db_strerror(ret));
				if (cursorp != NULL)
					cursorp->c_close(cursorp);
				return -1;
			}
#ifdef BDB_EXTRA_DEBUG
		} else {
			LOG(L_NOTICE, "BDB:bdb_delete_table: does not match\n");
#endif
		};

		ret = cursorp->c_get(cursorp, keyp, &data, nflags);
	}

	if (ret != DB_NOTFOUND) {
		if (cursorp != NULL)
			cursorp->c_close(cursorp);
                LOG(L_ERR, "BDB:bdb_delete_table: %s\n", db_strerror(ret));
		return -1;
	}

	if (cursorp != NULL)
		cursorp->c_close(cursorp);

	return 0;
}


