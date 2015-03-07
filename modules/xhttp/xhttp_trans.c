/* 
 * Copyright (C) 2013 Crocodile RCS Ltd
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "../../pvar.h"
#include "../../str.h"
#include "../../trim.h"
#include "xhttp_trans.h"

enum _tr_xhttp_type { TR_XHTTP_NONE = 0, TR_XHTTPURL, TR_XHTTPURLQUERYSTRING };
enum _tr_xhttpurl_subtype { TR_XHTTPURL_NONE = 0, TR_XHTTPURL_PATH,
	TR_XHTTPURL_QUERYSTRING};
enum _tr_xhttpquerystring_subtype { TR_XHTTPUTLQUERYSTRING_NONE = 0,
	TR_XHTTPURLQUERYSTRING_VALUE};

static str _httpurl_str = {0, 0};
static int _httpurl_querystring_pos = 0;

int xhttp_tr_eval_xhttpurl(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	int pos = 0;

	if (val == NULL || val->flags & PV_VAL_NULL)
		return -1;

	if (!(val->flags & PV_VAL_STR))
	{
		val->rs.s = int2str(val->ri, &val->rs.len);
		val->flags = PV_VAL_STR;
	}

	if (_httpurl_str.len == 0 || _httpurl_str.len != val->rs.len
		|| strncmp(_httpurl_str.s, val->rs.s, val->rs.len) != 0)
	{
		if (val->rs.len > _httpurl_str.len)
		{
			if (_httpurl_str.s) pkg_free(_httpurl_str.s);
			_httpurl_str.s = (char *) pkg_malloc(
					(val->rs.len + 1) * sizeof(char));
			if (_httpurl_str.s == NULL)
			{
				LM_ERR("allocating package memory\n");
				memset(&_httpurl_str.s, 0, sizeof(str));
				return -1;
			}
		}
		_httpurl_str.len = val->rs.len;
		memcpy(_httpurl_str.s, val->rs.s, val->rs.len);

		while (pos < val->rs.len && val->rs.s[pos] != '?') pos++;
		_httpurl_querystring_pos = (pos >= val->rs.len) ? 0 : pos + 1;
	}

	switch (subtype)
	{
	case TR_XHTTPURL_PATH:
		val->rs.len = (_httpurl_querystring_pos == 0)
				? val->rs.len : _httpurl_querystring_pos - 1;
		break;

	case TR_XHTTPURL_QUERYSTRING:
		if (_httpurl_querystring_pos == 0)
		{
			val->rs.s[0] = '\0';
			val->rs.len = 0;
			break;
		}

		val->rs.s = &val->rs.s[_httpurl_querystring_pos];
		val->rs.len = val->rs.len - _httpurl_querystring_pos;
		break;

	default:
		LM_ERR("unknown subtype %d\n", subtype);
		return -1;
	}

	return 0;
}

char *xhttp_tr_parse_url(str *in, trans_t *t)
{
	char *p;
	str name;

	if (in == NULL || in->s == NULL || t == NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_XHTTPURL;
	t->trf = xhttp_tr_eval_xhttpurl;

        /* find next token */
        while (is_in_str(p, in) && *p != TR_PARAM_MARKER && *p != TR_RBRACKET)
	{
		p++;
	}

        if (*p == '\0')
        {
                LM_ERR("invalid transformation: %.*s\n", in->len, in->s);
                goto error;
        }
        name.len = p - name.s;
        trim(&name);

	if (name.len == 4 && strncasecmp(name.s, "path", 4) == 0)
	{
		t->subtype = TR_XHTTPURL_PATH;
		goto done;
	}
	else if (name.len == 11 && strncasecmp(name.s, "querystring", 11) == 0)
	{
		t->subtype = TR_XHTTPURL_QUERYSTRING;
		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	return NULL;

done:
	t->name = name;
	return p;
}
