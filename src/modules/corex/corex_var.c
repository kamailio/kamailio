/**
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/action.h"
#include "../../core/socket_info.h"
#include "../../core/trim.h"
#include "../../core/pvapi.h"

#include "corex_var.h"

/**
 *
 */
int pv_parse_cfg_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 4:
			if(strncmp(in->s, "line", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "name", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "file", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "route", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV af key: %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_cfg(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	char *n;

	if(param==NULL)
		return -1;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			n = get_cfg_crt_file_name();
			if(n==0)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, n);
		case 2:
			n = get_cfg_crt_route_name();
			if(n==0)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, n);
		default:
			return pv_get_sintval(msg, param, res, get_cfg_crt_line());
	}
}

/**
 *
 */
typedef struct pv_lsock_expr {
	str match;
	str val;
	int valno;
	str field;
} pv_lsock_expr_t;

/**
 *
 */
int pv_parse_lsock_expr(str *in, pv_lsock_expr_t *lexpr)
{
	int i;
	char *p0 = NULL;
	char *p1 = NULL;

	/* match/val/field */
	for(i=0; i<in->len; i++) {
		if(in->s[i]=='/') {
			if(p0==NULL) {
				p0 = in->s + i;
			} else if(p1==NULL) {
				p1 = in->s + i;
			} else {
				LM_ERR("invalid expression format: %.*s\n", in->len, in->s);
				return -1;
			}
		}
	}
	if(p0==NULL || p1==NULL || p1 >= in->s+in->len-2) {
		LM_ERR("invalid expression format: %.*s\n", in->len, in->s);
		return -1;
	}
	memset(lexpr, 0, sizeof(pv_lsock_expr_t));
	lexpr->match.s = in->s;
	lexpr->match.len = p0 - lexpr->match.s;
	if(lexpr->match.len!=1 || (lexpr->match.s[0]!='n' && lexpr->match.s[0]!='l'
				&& lexpr->match.s[0]!='i' && lexpr->match.s[0]!='a')) {
		LM_ERR("invalid expression format: %.*s\n", in->len, in->s);
		return -1;
	}
	lexpr->val.s = p0 + 1;
	lexpr->val.len = p1 - lexpr->val.s;
	lexpr->field.s = p1 + 1;
	lexpr->field.len = in->s + in->len - lexpr->field.s;
	if(lexpr->field.len<1 || (lexpr->field.s[0]!='n' && lexpr->field.s[0]!='l'
				&& lexpr->field.s[0]!='i' && lexpr->field.s[0]!='a')) {
		LM_ERR("invalid expression format: %.*s\n", in->len, in->s);
		return -1;
	}
	LM_DBG("expression - match [%.*s] val [%.*s] field [%.*s]\n",
			lexpr->match.len, lexpr->match.s,
			lexpr->val.len, lexpr->val.s,
			lexpr->field.len, lexpr->field.s);

	return 0;
}

/**
 *
 */
void pv_free_lsock_name(void *p)
{
	pv_elem_free_all((pv_elem_t*)p);
}

/**
 *
 */
int pv_parse_lsock_name(pv_spec_t *sp, str *in)
{
	pv_elem_t *pve = NULL;

	if(in->s==NULL || in->len<=0)
		return -1;

	LM_DBG("lsock expression [%.*s]\n", in->len, in->s);
	if(pv_parse_format(in, &pve)<0 || pve==NULL) {
		LM_ERR("wrong format [%.*s]\n", in->len, in->s);
		goto error;
	}
	sp->pvp.pvn.u.dname = (void*)pve;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	sp->pvp.pvn.nfree = pv_free_lsock_name;

	return 0;

error:
	return -1;
}

/**
 *
 */
int pv_get_lsock(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	str vexpr = STR_NULL;
	pv_lsock_expr_t lexpr;
	socket_info_t *si = NULL;

	if(pv_printf_s(msg, (pv_elem_t*)param->pvn.u.dname, &vexpr)!=0) {
		LM_ERR("cannot get lsock expression\n");
		return -1;
	}

	if(pv_parse_lsock_expr(&vexpr, &lexpr) < 0) {
		LM_ERR("failed to parse lsock expression [%.*s]\n",
				vexpr.len, vexpr.s);
		return -1;
	}

	switch(lexpr.match.s[0]) {
		case 'n':
			si = ksr_get_socket_by_name(&lexpr.val);
			break;
		case 'l':
			si = ksr_get_socket_by_listen(&lexpr.val);
			break;
	}
	if(si == NULL) {
		return pv_get_null(msg, param, res);
	}

	switch(lexpr.field.s[0]) {
		case 'n':
			if(si->sockname.len==0) {
				return pv_get_strempty(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &si->sockname);
		case 'l':
			if(si->sock_str.len==0) {
				return pv_get_strempty(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &si->sock_str);
		case 'a':
			if(si->useinfo.sock_str.len==0) {
				return pv_get_strempty(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &si->useinfo.sock_str);
		case 'i':
			return pv_get_sintval(msg, param, res, si->gindex);
	}

	return pv_get_null(msg, param, res);
}

enum _tr_sock_type
{
	TR_NONE = 0,
	TR_SOCK
};

enum _tr_sock_subtype
{
	TR_SOCK_NONE = 0,
	TR_SOCK_HOST,
	TR_SOCK_PORT,
	TR_SOCK_PROTO,
	TR_SOCK_TOURI
};

/*!
 * \brief Evaluate SOCK transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_sock_eval(sip_msg_t *msg, tr_param_t *tp, int subtype, pv_value_t *val)
{

	str sv;
	sr_phostp_t php;

	if(val == NULL || (val->flags & PV_VAL_NULL)) {
		return -1;
	}

	if(!(val->flags & PV_VAL_STR)) {
		return -1;
	}

	if(val->rs.len >= pv_get_buffer_size()-1) {
		LM_ERR("value is too long\n");
		return -1;
	}

	if(parse_protohostport(&val->rs, &php)<0) {
		return -1;
	}

	sv.s = pv_get_buffer();
	sv.s[0] = '\0';
	sv.len = 0;
	switch(subtype) {
		case TR_SOCK_HOST:
			if(php.host.len>0) {
				memcpy(sv.s, php.host.s, php.host.len);
				sv.len = php.host.len;
				sv.s[sv.len] = 0;
			}
		break;
		case TR_SOCK_PORT:
			if(php.sport.len>0) {
				memcpy(sv.s, php.sport.s, php.sport.len);
				sv.len = php.sport.len;
				sv.s[sv.len] = 0;
			}
		break;
		case TR_SOCK_PROTO:
			if(php.sproto.len>0) {
				memcpy(sv.s, php.sproto.s, php.sproto.len);
				sv.len = php.sproto.len;
				sv.s[sv.len] = 0;
			}
		break;
		case TR_SOCK_TOURI:
			if(php.host.len>0 && php.sport.len>0 && php.sproto.len) {
				sv.len = snprintf(sv.s, pv_get_buffer_size()-1,
						"sip:%.*s:%.*s;transport=%.*s",
						php.host.len, php.host.s,
						php.sport.len, php.sport.s,
						php.sproto.len, php.sproto.s);
			} else if(php.host.len>0 && php.sport.len>0) {
				sv.len = snprintf(sv.s, pv_get_buffer_size()-1,
						"sip:%.*s:%.*s",
						php.host.len, php.host.s,
						php.sport.len, php.sport.s);
			} else if(php.host.len>0 && php.sproto.len>0) {
				sv.len = snprintf(sv.s, pv_get_buffer_size()-1,
						"sip:%.*s;transport=%.*s",
						php.host.len, php.host.s,
						php.sproto.len, php.sproto.s);
			} else if(php.host.len>0) {
				sv.len = snprintf(sv.s, pv_get_buffer_size()-1,
						"sip:%.*s",
						php.host.len, php.host.s);
			}
			if(sv.len<0 || sv.len>=pv_get_buffer_size()-1) {
				LM_WARN("uri too long: %d\n", sv.len);
				sv.s[0] = '\0';
				sv.len = 0;
			}
		break;
		default:
			LM_ERR("unknown subtype %d (cfg line: %d)\n",
					subtype, get_cfg_crt_line());
			return -1;
	}
	val->flags = PV_VAL_STR;
	val->ri = 0;
	val->rs = sv;

	return 0;
}

char *tr_sock_parse(str *in, trans_t *tr)
{
	char *p;
	str name;

	if(in == NULL || tr == NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	tr->type = TR_SOCK;
	tr->trf = tr_sock_eval;

	/* find next token */
	while(is_in_str(p, in) && *p != TR_PARAM_MARKER && *p != TR_RBRACKET)
		p++;
	if(*p == '\0') {
		LM_ERR("invalid transformation: %.*s\n", in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len == 4 && strncasecmp(name.s, "host", 4) == 0) {
		tr->subtype = TR_SOCK_HOST;
		goto done;
	} else if(name.len == 4 && strncasecmp(name.s, "port", 4) == 0) {
		tr->subtype = TR_SOCK_PORT;
		goto done;
	} else if(name.len == 5 && strncasecmp(name.s, "proto", 5) == 0) {
		tr->subtype = TR_SOCK_PROTO;
		goto done;
	} else if(name.len == 5 && strncasecmp(name.s, "touri", 5) == 0) {
		tr->subtype = TR_SOCK_TOURI;
		goto done;
	}
	LM_ERR("unknown SOCK transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	return NULL;

done:
	tr->name = name;
	return p;
}

