/*
 * Export vontact attrs as PV
 *
 * Copyright (C) 2008 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/*!
 * \file
 * \brief SIP registrar module - export contacts as PV
 * \ingroup registrar
 */


#include <string.h>
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mod_fix.h"
#include "../../core/route.h"
#include "../../core/action.h"
#include "../../core/resolve.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/receive.h"
#include "../usrloc/usrloc.h"
#include "registrar.h"
#include "common.h"
#include "regpv.h"

extern str reg_event_callback;

typedef struct _regpv_profile
{
	str pname;
	str domain;
	str aor;
	int flags;
	unsigned int aorhash;
	int nrc;
	ucontact_t *contacts;
	struct _regpv_profile *next;
} regpv_profile_t;

typedef struct _regpv_name
{
	regpv_profile_t *rp;
	int attr;
} regpv_name_t;

static regpv_profile_t *_regpv_profile_list = NULL;

static inline regpv_profile_t *regpv_get_profile(str *name)
{
	regpv_profile_t *rp;

	if(name == NULL || name->len <= 0) {
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	rp = _regpv_profile_list;
	while(rp) {
		if(rp->pname.len == name->len
				&& strncmp(rp->pname.s, name->s, name->len) == 0)
			return rp;
		rp = rp->next;
	}

	rp = (regpv_profile_t *)pkg_malloc(sizeof(regpv_profile_t));
	if(rp == NULL) {
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(rp, 0, sizeof(regpv_profile_t));
	rp->pname.s = (char *)pkg_malloc((name->len + 1) * sizeof(char));
	if(rp->pname.s == NULL) {
		LM_ERR("no more pkg\n");
		pkg_free(rp);
		return NULL;
	}
	memcpy(rp->pname.s, name->s, name->len);
	rp->pname.s[name->len] = '\0';
	rp->pname.len = name->len;

	rp->next = _regpv_profile_list;
	_regpv_profile_list = rp;
	return rp;
}

static void regpv_free_profile(regpv_profile_t *rpp)
{
	ucontact_t *ptr;
	ucontact_t *ptr0;

	if(rpp == NULL)
		return;

	ptr = rpp->contacts;
	while(ptr) {
		ptr0 = ptr;
		ptr = ptr->next;
		pkg_free(ptr0);
	}
	if(rpp->domain.s != NULL) {
		rpp->domain.s = 0;
		rpp->domain.len = 0;
	}
	if(rpp->aor.s != NULL) {
		pkg_free(rpp->aor.s);
		rpp->aor.s = 0;
		rpp->aor.len = 0;
	}

	rpp->flags = 0;
	rpp->aorhash = 0;
	rpp->nrc = 0;
	rpp->contacts = 0;
}

void regpv_free_profiles(void)
{
	regpv_profile_t *rp;
	regpv_profile_t *rp0;

	rp = _regpv_profile_list;

	while(rp) {
		if(rp->pname.s != NULL)
			pkg_free(rp->pname.s);
		rp0 = rp;
		regpv_free_profile(rp0);
		rp = rp->next;
	}
	_regpv_profile_list = 0;
}

int pv_get_ulc(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	regpv_name_t *rp;
	regpv_profile_t *rpp;
	ucontact_t *c;
	int idx;
	int i;

	if(param == NULL) {
		LM_ERR("invalid params\n");
		return -1;
	}
	rp = (regpv_name_t *)param->pvn.u.dname;
	if(rp == NULL || rp->rp == NULL) {
		LM_DBG("no profile in params\n");
		return pv_get_null(msg, param, res);
	}
	rpp = rp->rp;

	if(rpp->flags == 0 || rpp->contacts == NULL) {
		LM_DBG("profile not set or no contacts there\n");
		return pv_get_null(msg, param, res);
	}
	/* get index */
	if(pv_get_spec_index(msg, param, &idx, &i) != 0) {
		LM_ERR("invalid index\n");
		return -1;
	}

	/* work only with positive indexes by now */
	if(idx < 0)
		idx = 0;

	/* get contact */
	i = 0;
	c = rpp->contacts;
	while(c) {
		if(i == idx)
			break;
		i++;
		c = c->next;
	}
	if(c == NULL)
		return pv_get_null(msg, param, res);

	switch(rp->attr) {
		case 0: /* aor */
			return pv_get_strval(msg, param, res, &rpp->aor);
			break;
		case 1: /* domain */
			return pv_get_strval(msg, param, res, &rpp->domain);
			break;
		case 2: /* aorhash */
			return pv_get_uintval(msg, param, res, rpp->aorhash);
			break;
		case 3: /* addr */
			return pv_get_strval(msg, param, res, &c->c);
			break;
		case 4: /* path */
			return pv_get_strval(msg, param, res, &c->path);
			break;
		case 5: /* received */
			return pv_get_strval(msg, param, res, &c->received);
			break;
		case 6: /* expires */
			return pv_get_uintval(msg, param, res, (unsigned int)c->expires);
			break;
		case 7: /* callid */
			return pv_get_strval(msg, param, res, &c->callid);
			break;
		case 8: /* q */
			return pv_get_sintval(msg, param, res, (int)c->q);
			break;
		case 9: /* cseq */
			return pv_get_sintval(msg, param, res, c->cseq);
			break;
		case 10: /* flags */
			return pv_get_uintval(msg, param, res, c->flags);
			break;
		case 11: /* cflags */
			return pv_get_uintval(msg, param, res, c->cflags);
			break;
		case 12: /* user agent */
			return pv_get_strval(msg, param, res, &c->user_agent);
			break;
		case 14: /* socket */
			if(c->sock == NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &c->sock->sock_str);
			break;
		case 15: /* modified */
			return pv_get_uintval(
					msg, param, res, (unsigned int)c->last_modified);
			break;
		case 16: /* methods */
			return pv_get_uintval(msg, param, res, c->methods);
			break;
		case 17: /* count */
			return pv_get_sintval(msg, param, res, rpp->nrc);
			break;
		case 18: /* ruid */
			return pv_get_strval(msg, param, res, &c->ruid);
			break;
		case 19: /* reg-id */
			return pv_get_uintval(msg, param, res, c->reg_id);
			break;
		case 20: /* instance */
			if(c->instance.len > 0)
				return pv_get_strval(msg, param, res, &c->instance);
			break;
		case 21: /* conid */
			if(c->tcpconn_id > 0)
				return pv_get_sintval(msg, param, res, c->tcpconn_id);
			if(c->sock
					&& (c->sock->proto == PROTO_TCP
							|| c->sock->proto == PROTO_TLS
							|| c->sock->proto == PROTO_WS
							|| c->sock->proto == PROTO_WSS))
				return pv_get_sintval(msg, param, res, c->tcpconn_id);
			break;
		case 22: /* server_id */
			return pv_get_uintval(msg, param, res, c->server_id);
			break;
	}

	return pv_get_null(msg, param, res);
}

int pv_set_ulc(struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val)
{
	return 0;
}

int pv_parse_ulc_name(pv_spec_p sp, str *in)
{
	str pn;
	str pa;
	regpv_name_t *rp = NULL;
	regpv_profile_t *rpp = NULL;

	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;

	pa.s = in->s;
	while(pa.s < in->s + in->len - 2) {
		if(*pa.s == '=')
			break;
		pa.s++;
	}

	if(pa.s >= in->s + in->len - 2) {
		LM_ERR("invalid contact pv name %.*s\n", in->len, in->s);
		return -1;
	}
	if(*(pa.s + 1) != '>') {
		LM_ERR("invalid contact pv name %.*s.\n", in->len, in->s);
		return -1;
	}

	pn.s = in->s;
	pn.len = pa.s - pn.s;

	LM_DBG("get profile [%.*s]\n", pn.len, pn.s);

	rpp = regpv_get_profile(&pn);
	if(rpp == NULL) {
		LM_ERR("cannot get profile [%.*s]\n", pn.len, pn.s);
		return -1;
	}
	pa.s += 2;
	pa.len = in->s + in->len - pa.s;
	LM_DBG("get attr [%.*s]\n", pa.len, pa.s);

	rp = (regpv_name_t *)pkg_malloc(sizeof(regpv_name_t));
	if(rp == 0) {
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(rp, 0, sizeof(regpv_name_t));
	rp->rp = rpp;

	switch(pa.len) {
		case 1:
			if(strncmp(pa.s, "q", 1) == 0)
				rp->attr = 8;
			else
				goto error;
			break;
		case 3:
			if(strncmp(pa.s, "aor", 3) == 0)
				rp->attr = 0;
			else
				goto error;
			break;
		case 4:
			if(strncmp(pa.s, "addr", 4) == 0)
				rp->attr = 3;
			else if(strncmp(pa.s, "path", 4) == 0)
				rp->attr = 4;
			else if(strncmp(pa.s, "cseq", 4) == 0)
				rp->attr = 9;
			else if(strncmp(pa.s, "ruid", 4) == 0)
				rp->attr = 18;
			else
				goto error;
			break;
		case 5:
			if(strncmp(pa.s, "flags", 5) == 0)
				rp->attr = 10;
			else if(strncmp(pa.s, "count", 5) == 0)
				rp->attr = 17;
			else if(strncmp(pa.s, "regid", 5) == 0)
				rp->attr = 19;
			else if(strncmp(pa.s, "conid", 5) == 0)
				rp->attr = 21;
			else
				goto error;
			break;
		case 6:
			if(strncmp(pa.s, "domain", 6) == 0)
				rp->attr = 1;
			else if(strncmp(pa.s, "callid", 6) == 0)
				rp->attr = 7;
			else if(strncmp(pa.s, "cflags", 6) == 0)
				rp->attr = 11;
			else if(strncmp(pa.s, "socket", 6) == 0)
				rp->attr = 14;
			else
				goto error;
			break;
		case 7:
			if(strncmp(pa.s, "aorhash", 7) == 0)
				rp->attr = 2;
			else if(strncmp(pa.s, "expires", 7) == 0)
				rp->attr = 6;
			else if(strncmp(pa.s, "methods", 7) == 0)
				rp->attr = 16;
			else
				goto error;
			break;
		case 8:
			if(strncmp(pa.s, "received", 8) == 0)
				rp->attr = 5;
			else if(strncmp(pa.s, "modified", 8) == 0)
				rp->attr = 15;
			else if(strncmp(pa.s, "instance", 8) == 0)
				rp->attr = 20;
			else
				goto error;
			break;
		case 9:
			if(strncmp(pa.s, "server_id", 9) == 0)
				rp->attr = 22;
			break;
		case 10:
			if(strncmp(pa.s, "user_agent", 10) == 0)
				rp->attr = 12;
			else
				goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.u.dname = (void *)rp;
	sp->pvp.pvn.type = PV_NAME_PVAR;

	return 0;

error:
	if(rp)
		pkg_free(rp);
	LM_ERR("unknown contact attr name in %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
static sr_kemi_xval_t _sr_kemi_reg_ulc_xval = {0};

sr_kemi_xval_t *ki_reg_ulc_rget(sip_msg_t *msg, str *rid, str *attr)
{
	regpv_profile_t *rpp = NULL;

	memset(&_sr_kemi_reg_ulc_xval, 0, sizeof(sr_kemi_xval_t));
	if(rid == NULL || rid->s == NULL || attr == NULL || attr->s == NULL) {
		LM_WARN("invalid parameters - return value 0\n");
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_reg_ulc_xval.v.n = 0;
		return &_sr_kemi_reg_ulc_xval;
	}

	rpp = regpv_get_profile(rid);
	if(rpp == 0) {
		LM_WARN("result [%.*s] is not defined - return value 0\n", rid->len,
				rid->s);
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_reg_ulc_xval.v.n = 0;
		return &_sr_kemi_reg_ulc_xval;
	}

	if(attr->len == 5 && strncmp(attr->s, "count", 5) == 0) {
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_reg_ulc_xval.v.n = rpp->nrc;
		return &_sr_kemi_reg_ulc_xval;
	} else if(attr->len == 3 && strncmp(attr->s, "aor", 3) == 0) {
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_reg_ulc_xval.v.s = rpp->aor;
		return &_sr_kemi_reg_ulc_xval;
	}

	LM_WARN("attribute [%.*s] is not defined - return value 0\n", attr->len,
			attr->s);
	_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
	_sr_kemi_reg_ulc_xval.v.n = 0;
	return &_sr_kemi_reg_ulc_xval;
}

sr_kemi_xval_t *ki_reg_ulc_cget(sip_msg_t *msg, str *rid, str *attr, int idx)
{
	regpv_profile_t *rpp = NULL;
	ucontact_t *c = NULL;
	int i = 0;


	memset(&_sr_kemi_reg_ulc_xval, 0, sizeof(sr_kemi_xval_t));
	if(rid == NULL || rid->s == NULL || attr == NULL || attr->s == NULL) {
		LM_WARN("invalid parameters - return value 0\n");
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_reg_ulc_xval.v.n = 0;
		return &_sr_kemi_reg_ulc_xval;
	}

	rpp = regpv_get_profile(rid);
	if(rpp == 0) {
		LM_WARN("result [%.*s] is not defined - return value 0\n", rid->len,
				rid->s);
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_reg_ulc_xval.v.n = 0;
		return &_sr_kemi_reg_ulc_xval;
	}

	/* work only with positive indexes by now */
	if(idx < 0) {
		idx = 0;
	}
	/* get contact by index */
	i = 0;
	for(c = rpp->contacts; c != NULL; c = c->next) {
		if(i == idx) {
			break;
		}
		i++;
	}
	if(c == NULL) {
		LM_WARN("contact at index [%d] is not found - return value 0\n", idx);
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
		_sr_kemi_reg_ulc_xval.v.n = 0;
		return &_sr_kemi_reg_ulc_xval;
	}

	if(attr->len == 4 && strncmp(attr->s, "addr", 4) == 0) {
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_reg_ulc_xval.v.s = c->c;
		return &_sr_kemi_reg_ulc_xval;
	} else if(attr->len == 6 && strncmp(attr->s, "socket", 6) == 0) {
		if(c->sock == NULL) {
			sr_kemi_xval_null(&_sr_kemi_reg_ulc_xval, SR_KEMI_XVAL_NULL_EMPTY);
			return &_sr_kemi_reg_ulc_xval;
		}
		_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_reg_ulc_xval.v.s = c->sock->sock_str;
		return &_sr_kemi_reg_ulc_xval;
	}

	LM_WARN("attribute [%.*s] is not defined - return value 0\n", attr->len,
			attr->s);
	_sr_kemi_reg_ulc_xval.vtype = SR_KEMIP_INT;
	_sr_kemi_reg_ulc_xval.v.n = 0;
	return &_sr_kemi_reg_ulc_xval;
}

int pv_fetch_contacts_helper(
		sip_msg_t *msg, udomain_t *dt, str *uri, str *profile)
{
	urecord_t *r;
	ucontact_t *ptr;
	ucontact_t *ptr0;
	ucontact_t *c0;
	regpv_profile_t *rpp;
	str aor = {0, 0};
	int res;
	int olen;
	int ilen;
	int n;
	char *p;

	rpp = regpv_get_profile((str *)profile);
	if(rpp == 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	/* check and free if profile already set */
	if(rpp->flags)
		regpv_free_profile(rpp);

	if(extract_aor(uri, &aor, NULL) < 0) {
		LM_ERR("failed to extract Address Of Record\n");
		return -1;
	}

	/* copy aor and ul domain */
	rpp->aor.s = (char *)pkg_malloc(aor.len * sizeof(char));
	if(rpp->aor.s == NULL) {
		LM_ERR("no more pkg\n");
		return -1;
	}
	memcpy(rpp->aor.s, aor.s, aor.len);
	rpp->aor.len = aor.len;
	rpp->domain = *((udomain_head_t *)dt)->name;
	rpp->flags = 1;

	/* copy contacts */
	ilen = sizeof(ucontact_t);
	_reg_ul.lock_udomain(dt, &aor);
	res = _reg_ul.get_urecord(dt, &aor, &r);
	if(res > 0) {
		LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
		_reg_ul.unlock_udomain(dt, &aor);
		return -1;
	}

	ptr = r->contacts;
	ptr0 = NULL;
	n = 0;
	while(ptr) {
		olen = (ptr->c.len + ptr->received.len + ptr->path.len + ptr->callid.len
					   + ptr->user_agent.len + ptr->ruid.len
					   + ptr->instance.len)
					   * sizeof(char)
			   + ilen;
		c0 = (ucontact_t *)pkg_malloc(olen);
		if(c0 == NULL) {
			LM_ERR("no more pkg\n");
			_reg_ul.release_urecord(r);
			_reg_ul.unlock_udomain(dt, &aor);
			goto error;
		}
		memcpy(c0, ptr, ilen);
		c0->domain = NULL;
		c0->aor = NULL;
		c0->next = NULL;
		c0->prev = NULL;

		c0->c.s = (char *)c0 + ilen;
		memcpy(c0->c.s, ptr->c.s, ptr->c.len);
		c0->c.len = ptr->c.len;
		p = c0->c.s + c0->c.len;

		if(ptr->received.s != NULL) {
			c0->received.s = p;
			memcpy(c0->received.s, ptr->received.s, ptr->received.len);
			c0->received.len = ptr->received.len;
			p += c0->received.len;
		}
		if(ptr->path.s != NULL) {
			c0->path.s = p;
			memcpy(c0->path.s, ptr->path.s, ptr->path.len);
			c0->path.len = ptr->path.len;
			p += c0->path.len;
		}
		c0->callid.s = p;
		memcpy(c0->callid.s, ptr->callid.s, ptr->callid.len);
		c0->callid.len = ptr->callid.len;
		p += c0->callid.len;
		if(ptr->user_agent.s != NULL) {
			c0->user_agent.s = p;
			memcpy(c0->user_agent.s, ptr->user_agent.s, ptr->user_agent.len);
			c0->user_agent.len = ptr->user_agent.len;
			p += c0->user_agent.len;
		}
		if(ptr->ruid.s != NULL) {
			c0->ruid.s = p;
			memcpy(c0->ruid.s, ptr->ruid.s, ptr->ruid.len);
			c0->ruid.len = ptr->ruid.len;
			p += c0->ruid.len;
		}
		if(ptr->instance.s != NULL) {
			c0->instance.s = p;
			memcpy(c0->instance.s, ptr->instance.s, ptr->instance.len);
			c0->instance.len = ptr->instance.len;
			p += c0->instance.len;
		}
		LM_DBG("memory block between %p - %p\n", c0, p);
		if((ptr->sock)
				&& (ptr->sock->proto == PROTO_TCP
						|| ptr->sock->proto == PROTO_TLS
						|| ptr->sock->proto == PROTO_WS
						|| ptr->sock->proto == PROTO_WSS)) {
			c0->tcpconn_id = ptr->tcpconn_id;
		}
		if(ptr->tcpconn_id > 0) {
			LM_DBG("preset tcpconn_id : %d\n", ptr->tcpconn_id);
			c0->tcpconn_id = ptr->tcpconn_id;
		}

		if(ptr0 == NULL) {
			rpp->contacts = c0;
		} else {
			ptr0->next = c0;
			c0->prev = ptr0;
		}
		n++;
		ptr0 = c0;
		ptr = ptr->next;
	}
	_reg_ul.release_urecord(r);
	_reg_ul.unlock_udomain(dt, &aor);
	rpp->nrc = n;
	LM_DBG("fetched <%d> contacts for <%.*s> in [%.*s]\n", n, aor.len, aor.s,
			rpp->pname.len, rpp->pname.s);
	return 1;

error:
	regpv_free_profile(rpp);
	return -1;
}

int pv_fetch_contacts(sip_msg_t *msg, char *table, char *uri, char *profile)
{
	str u = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)uri, &u) != 0 || u.len <= 0) {
		LM_ERR("invalid uri parameter\n");
		return -1;
	}
	return pv_fetch_contacts_helper(
			msg, (udomain_t *)table, &u, (str *)profile);
}

int ki_reg_fetch_contacts(sip_msg_t *msg, str *dtable, str *uri, str *profile)
{
	udomain_t *d;

	if(_reg_ul.get_udomain(dtable->s, &d) < 0) {
		LM_ERR("usrloc domain [%s] not found\n", dtable->s);
		return -1;
	}

	return pv_fetch_contacts_helper(msg, d, uri, profile);
}

int ki_reg_free_contacts(sip_msg_t *msg, str *profile)
{
	regpv_profile_t *rpp;

	rpp = regpv_get_profile(profile);
	if(rpp == 0)
		return -1;

	regpv_free_profile(rpp);

	return 1;
}

int pv_free_contacts(struct sip_msg *msg, char *profile, char *s2)
{
	return ki_reg_free_contacts(msg, (str *)profile);
}

void reg_ul_expired_contact(ucontact_t *ptr, int type, void *param)
{
	str profile = {"exp", 3};
	regpv_profile_t *rpp;
	ucontact_t *c0;
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	int olen;
	int ilen;
	char *p;
	sr_kemi_eng_t *keng = NULL;

	if(reg_expire_event_rt < 0 && reg_event_callback.s == NULL)
		return;

	if(faked_msg_init() < 0) {
		LM_ERR("faked_msg_init() failed\n");
		return;
	}

	rpp = regpv_get_profile(&profile);
	if(rpp == 0) {
		LM_ERR("error getting profile structure\n");
		return;
	}
	/* check and free if profile already set */
	if(rpp->flags)
		regpv_free_profile(rpp);

	/* copy aor and ul domain */
	rpp->aor.s = (char *)pkg_malloc(ptr->aor->len * sizeof(char));
	if(rpp->aor.s == NULL) {
		LM_ERR("no more pkg\n");
		return;
	}
	memcpy(rpp->aor.s, ptr->aor->s, ptr->aor->len);
	rpp->aor.len = ptr->aor->len;
	rpp->domain = *ptr->domain;
	rpp->flags = 1;

	/* copy contact */
	ilen = sizeof(ucontact_t);

	olen = (ptr->c.len + ptr->received.len + ptr->path.len + ptr->callid.len
				   + ptr->user_agent.len + ptr->ruid.len + ptr->instance.len)
				   * sizeof(char)
		   + ilen;
	c0 = (ucontact_t *)pkg_malloc(olen);
	if(c0 == NULL) {
		LM_ERR("no more pkg\n");
		goto error;
	}
	memcpy(c0, ptr, ilen);
	c0->domain = NULL;
	c0->aor = NULL;
	c0->next = NULL;
	c0->prev = NULL;

	c0->c.s = (char *)c0 + ilen;
	memcpy(c0->c.s, ptr->c.s, ptr->c.len);
	c0->c.len = ptr->c.len;
	p = c0->c.s + c0->c.len;

	if(ptr->received.s != NULL) {
		c0->received.s = p;
		memcpy(c0->received.s, ptr->received.s, ptr->received.len);
		c0->received.len = ptr->received.len;
		p += c0->received.len;
	}
	if(ptr->path.s != NULL) {
		c0->path.s = p;
		memcpy(c0->path.s, ptr->path.s, ptr->path.len);
		c0->path.len = ptr->path.len;
		p += c0->path.len;
	}
	c0->callid.s = p;
	memcpy(c0->callid.s, ptr->callid.s, ptr->callid.len);
	c0->callid.len = ptr->callid.len;
	p += c0->callid.len;
	if(ptr->user_agent.s != NULL) {
		c0->user_agent.s = p;
		memcpy(c0->user_agent.s, ptr->user_agent.s, ptr->user_agent.len);
		c0->user_agent.len = ptr->user_agent.len;
		p += c0->user_agent.len;
	}
	if(ptr->ruid.s != NULL) {
		c0->ruid.s = p;
		memcpy(c0->ruid.s, ptr->ruid.s, ptr->ruid.len);
		c0->ruid.len = ptr->ruid.len;
		p += c0->ruid.len;
	}
	if(ptr->instance.s != NULL) {
		c0->instance.s = p;
		memcpy(c0->instance.s, ptr->instance.s, ptr->instance.len);
		c0->instance.len = ptr->instance.len;
		p += c0->instance.len;
	}

	rpp->contacts = c0;
	rpp->nrc = 1;
	LM_DBG("saved contact for <%.*s> in [%.*s]\n", ptr->aor->len, ptr->aor->s,
			rpp->pname.len, rpp->pname.s);

	fmsg = faked_msg_next();
	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);

	if(reg_expire_event_rt >= 0) {
		run_top_route(event_rt.rlist[reg_expire_event_rt], fmsg, 0);
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL) {
			str evname = str_init("usrloc:contact-expired");
			if(sr_kemi_route(
					   keng, fmsg, EVENT_ROUTE, &reg_event_callback, &evname)
					< 0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	set_route_type(backup_rt);
	ksr_msg_env_reset();
	return;
error:
	regpv_free_profile(rpp);
	return;
}

int ki_lookup_xavp(
		sip_msg_t *msg, str *utname, str *uri, str *rxname, str *cxname)
{
	udomain_t *dt;
	urecord_t *r;
	ucontact_t *ptr;
	str aor = {0, 0};
	sr_xavp_t *rxavp = NULL;
	sr_xavp_t *cxavp = NULL;
	sr_xavp_t *pxavp = NULL;
	sr_xval_t nxval;
	str fxname = {0, 0};
	int res;
	int n;

	if(extract_aor(uri, &aor, NULL) < 0) {
		LM_ERR("failed to extract Address Of Record\n");
		return -1;
	}

	if(_reg_ul.get_udomain(utname->s, &dt) < 0) {
		LM_ERR("usrloc domain [%s] not found\n", utname->s);
		return -1;
	}

	/* add record aor field */
	memset(&nxval, 0, sizeof(sr_xval_t));
	nxval.type = SR_XTYPE_STR;
	nxval.v.s = aor;
	STR_STATIC_SET(fxname, "aor");
	if(xavp_add_value(&fxname, &nxval, &rxavp) == NULL) {
		LM_ERR("failed to add xavp %.*s field\n", fxname.len, fxname.s);
		return -1;
	}

	/* copy contacts */
	_reg_ul.lock_udomain(dt, &aor);
	res = _reg_ul.get_urecord(dt, &aor, &r);
	if(res > 0) {
		LM_DBG("'%.*s' not found in usrloc\n", aor.len, ZSW(aor.s));
		_reg_ul.unlock_udomain(dt, &aor);
		xavp_destroy_list(&rxavp);
		return -1;
	}

	ptr = r->contacts;
	n = 0;
	while(ptr) {
		/* add record uri field */
		memset(&nxval, 0, sizeof(sr_xval_t));
		nxval.type = SR_XTYPE_STR;
		nxval.v.s = ptr->c;
		STR_STATIC_SET(fxname, "uri");
		if(xavp_add_value(&fxname, &nxval, &cxavp) == NULL) {
			LM_ERR("failed to add xavp %.*s field\n", fxname.len, fxname.s);
			goto error;
		}
		if(ptr->sock) {
			memset(&nxval, 0, sizeof(sr_xval_t));
			nxval.type = SR_XTYPE_STR;
			nxval.v.s = ptr->sock->sock_str;
			STR_STATIC_SET(fxname, "socket");
			if(xavp_add_value(&fxname, &nxval, &cxavp) == NULL) {
				LM_ERR("failed to add xavp %.*s field\n", fxname.len, fxname.s);
				goto error;
			}
		}
		if(ptr->received.s != NULL) {
			memset(&nxval, 0, sizeof(sr_xval_t));
			nxval.type = SR_XTYPE_STR;
			nxval.v.s = ptr->received;
			STR_STATIC_SET(fxname, "dsturi");
			if(xavp_add_value(&fxname, &nxval, &cxavp) == NULL) {
				LM_ERR("failed to add xavp %.*s field\n", fxname.len, fxname.s);
				goto error;
			}
		}
		/* add cxavp in root list */
		memset(&nxval, 0, sizeof(sr_xval_t));
		nxval.type = SR_XTYPE_XAVP;
		nxval.v.xavp = cxavp;
		if((pxavp = xavp_add_value_after(cxname, &nxval, pxavp)) == NULL) {
			LM_ERR("cannot add dst xavp to root list\n");
			goto error;
		}
		cxavp = NULL;
		n++;
		ptr = ptr->next;
	}
	_reg_ul.release_urecord(r);
	_reg_ul.unlock_udomain(dt, &aor);

	/* add record count field */
	memset(&nxval, 0, sizeof(sr_xval_t));
	nxval.type = SR_XTYPE_LONG;
	nxval.v.l = n;
	STR_STATIC_SET(fxname, "count");
	if(xavp_add_value(&fxname, &nxval, &rxavp) == NULL) {
		LM_ERR("failed to add xavp %.*s field\n", fxname.len, fxname.s);
		xavp_destroy_list(&rxavp);
		return -1;
	}

	/* add rxavp in root list */
	memset(&nxval, 0, sizeof(sr_xval_t));
	nxval.type = SR_XTYPE_XAVP;
	nxval.v.xavp = rxavp;
	if((pxavp = xavp_add_value(rxname, &nxval, NULL)) == NULL) {
		LM_ERR("cannot add rxavp to root list\n");
		xavp_destroy_list(&rxavp);
		return -1;
	}

	LM_DBG("fetched <%d> contacts for <%.*s> in [%.*s]\n", n, aor.len, aor.s,
			cxname->len, cxname->s);
	return 1;

error:
	_reg_ul.release_urecord(r);
	_reg_ul.unlock_udomain(dt, &aor);
	if(cxavp != NULL) {
		xavp_destroy_list(&cxavp);
	}
	if(rxavp != NULL) {
		xavp_destroy_list(&rxavp);
	}
	return -1;
}

#define REG_FROM_USER_MPORT (1)
#define REG_FROM_USER_MPROTO (1 << 1)
/**
 *
 */
int ki_reg_from_user(sip_msg_t *msg, str *utname, str *uri, int vmode)
{
	udomain_t *dt;
	urecord_t *r;
	ucontact_t *ptr;
	str aor = {0, 0};
	sip_uri_t rcvuri;
	int res = -1;
	int ret = -1;
	ip_addr_t ipb;

	if(extract_aor(uri, &aor, NULL) < 0) {
		LM_ERR("failed to extract Address Of Record\n");
		return -1;
	}

	if(_reg_ul.get_udomain(utname->s, &dt) < 0) {
		LM_ERR("usrloc domain [%s] not found\n", utname->s);
		return -1;
	}

	/* copy contacts */
	_reg_ul.lock_udomain(dt, &aor);
	res = _reg_ul.get_urecord(dt, &aor, &r);
	if(res > 0) {
		LM_DBG("'%.*s' not found in usrloc\n", aor.len, ZSW(aor.s));
		_reg_ul.unlock_udomain(dt, &aor);
		return -1;
	}

	for(ptr = r->contacts; ptr; ptr = ptr->next) {
		if(ptr->received.s != NULL) {
			if(parse_uri(ptr->received.s, ptr->received.len, &rcvuri) < 0) {
				LM_ERR("failed to parse rcv uri [%.*s]\n", ptr->received.len,
						ptr->received.s);
				goto error;
			}
		} else {
			if(parse_uri(ptr->c.s, ptr->c.len, &rcvuri) < 0) {
				LM_ERR("failed to parse contact uri [%.*s]\n",
						ptr->received.len, ptr->received.s);
				goto error;
			}
		}
		if(str2ipxbuf(&rcvuri.host, &ipb) < 0) {
			LM_WARN("failed to convert ip [%.*s] - ignoring\n", rcvuri.host.len,
					rcvuri.host.s);
			continue;
		}
		if(!ip_addr_cmp(&msg->rcv.src_ip, &ipb)) {
			continue;
		}
		if(vmode & REG_FROM_USER_MPORT) {
			if(msg->rcv.src_port != rcvuri.port_no) {
				continue;
			}
		}
		if(vmode & REG_FROM_USER_MPROTO) {
			if(msg->rcv.proto != rcvuri.proto) {
				continue;
			}
		}
		LM_DBG("matched contact record with source address [%.*s]\n",
				rcvuri.host.len, rcvuri.host.s);
		ret = 1;
		break;
	}
	_reg_ul.release_urecord(r);
	_reg_ul.unlock_udomain(dt, &aor);

	return ret;

error:
	_reg_ul.release_urecord(r);
	_reg_ul.unlock_udomain(dt, &aor);
	return -1;
}
