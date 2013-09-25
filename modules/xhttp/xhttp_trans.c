/* 
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

	switch (subtype)
	{
	case TR_XHTTPURL_PATH:
		while (val->rs.s[pos] != '?' && pos < val->rs.len)
			pos++;

		val->rs.len = pos;
		break;

	case TR_XHTTPURL_QUERYSTRING:
		while (val->rs.s[pos] != '?' && pos < val->rs.len)
			pos++;

		if (pos >= val->rs.len)
		{
			val->rs.s[0] = '\0';
			val->rs.len = 0;
			break;
		}

		val->rs.s = &val->rs.s[pos + 1];
		val->rs.len = val->rs.len - pos - 1;
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
