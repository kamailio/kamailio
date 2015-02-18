/**
 * Copyright (C) 2011 SpeakUp B.V. (alex@speakup.nl)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../lib/kcore/strcommon.h"

#include "sql_trans.h"

#define TR_BUFFER_SIZE 2048


static int _tr_eval_sql_val(pv_value_t *val)
{
	int i;
	static char _tr_buffer[TR_BUFFER_SIZE];

	if(val->flags&PV_TYPE_INT || !(val->flags&PV_VAL_STR)) {
		val->rs.s = sint2str(val->ri, &val->rs.len);
		val->flags = PV_VAL_STR;
		return 0;
	}
	if(val->rs.len>TR_BUFFER_SIZE/2-1) {
		LM_ERR("escape buffer to short");
		return -1;
	}
	_tr_buffer[0] = '\'';
	i = escape_common(_tr_buffer+1, val->rs.s, val->rs.len);
	_tr_buffer[++i] = '\'';
	_tr_buffer[++i] = '\0';
	memset(val, 0, sizeof(pv_value_t));
	val->flags = PV_VAL_STR;
	val->rs.s = _tr_buffer;
	val->rs.len = i;
	return 0;
}


int tr_eval_sql(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	static str _sql_null = { "NULL", 4 };
	static str _sql_zero = { "0", 1 };
	static str _sql_empty = { "''", 2 };

	if(val==NULL)
		return -1;
	
	switch(subtype) {
		case TR_SQL_VAL:
			if (val->flags&PV_VAL_NULL) {
				val->flags = PV_VAL_STR;
				val->rs = _sql_null;
				return 0;
			} else {
				return _tr_eval_sql_val(val);
			}
			break;

		case TR_SQL_VAL_INT:
			if (val->flags&PV_VAL_NULL) {
				val->flags = PV_VAL_STR;
				val->rs = _sql_zero;
				return 0;
			} else {
				return _tr_eval_sql_val(val);
			}
			break;

		case TR_SQL_VAL_STR:
			if (val->flags&PV_VAL_NULL) {
				val->flags = PV_VAL_STR;
				val->rs = _sql_empty;
				return 0;
			} else {
				return _tr_eval_sql_val(val);
			}
			break;

		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
	return 0;
}


char* tr_parse_sql(str *in, trans_t *t)
{
	char *p;
	str name;


	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_SQL;
	t->trf = tr_eval_sql;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0') {
		LM_ERR("unable to find transformation start: %.*s\n", in->len, in->s);
		return NULL;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==3 && strncasecmp(name.s, "val", 3)==0) {
		t->subtype = TR_SQL_VAL;
		goto done;
	}
	if(name.len==7 && strncasecmp(name.s, "val.int", 7)==0) {
		t->subtype = TR_SQL_VAL_INT;
		goto done;
	}
	if(name.len==7 && strncasecmp(name.s, "val.str", 7)==0) {
		t->subtype = TR_SQL_VAL_STR;
		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
	return NULL;
done:
	t->name = name;
	return p;
}

