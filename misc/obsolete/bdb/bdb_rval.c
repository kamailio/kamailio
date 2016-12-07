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

int bdb_rrow_db2bdb(db_con_t* _h, db_key_t* _k, int _n, bdb_rrow_p *_r)
{
	bdb_rrow_p	r;
	bdb_table_p	t;
	bdb_column_p	c;

	int		found;
	int		i;
	int		c_idx;

	*_r = NULL;

	if ((t = bdb_find_table(CON_TABLE(_h))) == NULL) {
#ifdef BDB_EXTRA_DEBUG
		LOG(L_ERR, "BDB:bdb_rrow_db2bdb: no table in use\n");
#endif
		return -1;
	};

	i = (_n == 0) ? BDB_CON_COL_NUM(_h) : _n;
	r = pkg_malloc(sizeof(*r) * i);
	memset(r, 0, sizeof(*r) * i);

	if (_n > 0) {
		for (i = 0; i < _n; i++) {
			found = 0;
			for (c = t->cols, c_idx = 0; c != NULL; c = c->next, c_idx++) {
				if (!strcmp(_k[i], c->name.s)) {
#ifdef BDB_EXTRA_DEBUG
					LOG(L_NOTICE, "BDB:bdb_rrow_db2bdb: filling column '%.*s', c_idx = %0d\n", c->name.len, c->name.s, c_idx);
#endif
					r[i] = c_idx;
					found = 1;
					break;
				}
			}
			if (!found) {
				LOG(L_ERR, "BDB:bdb_rrow_db2bdb: column '%s' does not exist\n", _k[i]);
				bdb_free_rrow(r);
				return -1;
			}
		}
	} else {		/* return all columns */
		for (c = t->cols, c_idx = 0; c != NULL; c = c->next, c_idx++) {
#ifdef BDB_EXTRA_DEBUG
			LOG(L_NOTICE, "BDB:bdb_rrow_db2bdb: filling column '%.*s', c_idx = %0d\n", c->name.len, c->name.s, c_idx);
#endif
			r[c_idx] = c_idx;
		}
	}

	*_r = r;

	return 0;
};


void bdb_free_rrow(bdb_rrow_p _r)
{
	pkg_free(_r);
};
