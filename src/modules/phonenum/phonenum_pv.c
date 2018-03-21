/**
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/pvar.h"

#include "cphonenumber.h"
#include "phonenum_pv.h"

/* clang-format off */
typedef struct _sr_phonenum_record {
	telnum_t *record;
	char tomatch[256];
	long flags;
} sr_phonenum_record_t;

typedef struct _sr_phonenum_item {
	str pvclass;
	unsigned int hashid;
	sr_phonenum_record_t r;
	struct _sr_phonenum_item *next;
} sr_phonenum_item_t;

typedef struct _phonenum_pv {
	sr_phonenum_item_t *item;
	int type;
} phonenum_pv_t;
/* clang-format on */


static sr_phonenum_item_t *_sr_phonenum_list = NULL;

sr_phonenum_record_t *sr_phonenum_get_record(str *name)
{
	sr_phonenum_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid = get_hash1_raw(name->s, name->len);

	it = _sr_phonenum_list;
	while(it != NULL) {
		if(it->hashid == hashid && it->pvclass.len == name->len
				&& strncmp(it->pvclass.s, name->s, name->len) == 0)
			return &it->r;
		it = it->next;
	}
	return NULL;
}

sr_phonenum_item_t *sr_phonenum_add_item(str *name)
{
	sr_phonenum_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid = get_hash1_raw(name->s, name->len);

	it = _sr_phonenum_list;
	while(it != NULL) {
		if(it->hashid == hashid && it->pvclass.len == name->len
				&& strncmp(it->pvclass.s, name->s, name->len) == 0)
			return it;
		it = it->next;
	}
	/* add new */
	it = (sr_phonenum_item_t *)pkg_malloc(sizeof(sr_phonenum_item_t));
	if(it == NULL) {
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(it, 0, sizeof(sr_phonenum_item_t));
	it->pvclass.s = (char *)pkg_malloc(name->len + 1);
	if(it->pvclass.s == NULL) {
		LM_ERR("no more pkg.\n");
		pkg_free(it);
		return NULL;
	}
	memcpy(it->pvclass.s, name->s, name->len);
	it->pvclass.s[name->len] = '\0';
	it->pvclass.len = name->len;
	it->hashid = hashid;
	it->next = _sr_phonenum_list;
	_sr_phonenum_list = it;
	return it;
}


int pv_parse_phonenum_name(pv_spec_p sp, str *in)
{
	phonenum_pv_t *gpv = NULL;
	char *p;
	str pvc;
	str pvs;
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;

	gpv = (phonenum_pv_t *)pkg_malloc(sizeof(phonenum_pv_t));
	if(gpv == NULL)
		return -1;

	memset(gpv, 0, sizeof(phonenum_pv_t));

	p = in->s;

	while(p < in->s + in->len
			&& (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	if(p > in->s + in->len || *p == '\0')
		goto error;
	pvc.s = p;
	while(p < in->s + in->len) {
		if(*p == '=' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			break;
		p++;
	}
	if(p > in->s + in->len || *p == '\0')
		goto error;
	pvc.len = p - pvc.s;
	if(*p != '=') {
		while(p < in->s + in->len
				&& (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
			p++;
		if(p > in->s + in->len || *p == '\0' || *p != '=')
			goto error;
	}
	p++;
	if(*p != '>')
		goto error;
	p++;

	pvs.len = in->len - (int)(p - in->s);
	pvs.s = p;
	LM_DBG("phonenum [%.*s] - key [%.*s]\n", pvc.len, pvc.s, pvs.len, pvs.s);

	gpv->item = sr_phonenum_add_item(&pvc);
	if(gpv->item == NULL)
		goto error;

	switch(pvs.len) {
		case 5:
			if(strncmp(pvs.s, "ltype", 5) == 0)
				gpv->type = 2;
			else if(strncmp(pvs.s, "ndesc", 5) == 0)
				gpv->type = 3;
			else if(strncmp(pvs.s, "error", 5) == 0)
				gpv->type = 4;
			else if(strncmp(pvs.s, "cctel", 5) == 0)
				gpv->type = 5;
			else if(strncmp(pvs.s, "valid", 5) == 0)
				gpv->type = 6;
			else
				goto error;
			break;
		case 6:
			if(strncmp(pvs.s, "number", 6) == 0)
				gpv->type = 0;
			else
				goto error;
			break;
		case 10:
			if(strncmp(pvs.s, "normalized", 10) == 0)
				gpv->type = 1;
			else
				goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.u.dname = (void *)gpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;

	return 0;

error:
	if(gpv != NULL)
		pkg_free(gpv);

	LM_ERR("error at PV phonenum name: %.*s\n", in->len, in->s);
	return -1;
}

int pv_phonenum_get_strzval(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res, char *sval)
{
	str s;
	if(sval == NULL)
		return pv_get_null(msg, param, res);

	s.s = sval;
	s.len = strlen(s.s);
	return pv_get_strval(msg, param, res, &s);
}

int pv_get_phonenum(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	phonenum_pv_t *gpv;

	if(msg == NULL || param == NULL)
		return -1;

	gpv = (phonenum_pv_t *)param->pvn.u.dname;
	if(gpv == NULL)
		return -1;
	if(gpv->item == NULL)
		return pv_get_null(msg, param, res);
	if(gpv->item->r.record==NULL)
		return pv_get_null(msg, param, res);

	switch(gpv->type) {
		case 1: /* normalized */
			if(gpv->item->r.record->normalized==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res,
						gpv->item->r.record->normalized);
		case 2: /* ltype */
			if(gpv->item->r.record->ltype==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, gpv->item->r.record->ltype);
		case 3: /* ndesc */
			if(gpv->item->r.record->ndesc==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, gpv->item->r.record->ndesc);
		case 4: /* error */
			if(gpv->item->r.record->error==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, gpv->item->r.record->error);
		case 5: /* cctel */
			return pv_get_sintval(msg, param, res, gpv->item->r.record->cctel);
		case 6: /* valid */
			return pv_get_sintval(msg, param, res, gpv->item->r.record->valid);
		default: /* number */
			if(gpv->item->r.record->number==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, gpv->item->r.record->number);
	}
}

int phonenum_init_pv(int smode)
{
	return 0;
}

void phonenum_destroy_list(void)
{
}

void phonenum_destroy_pv(void)
{
}

void phonenum_pv_reset(str *name)
{
	sr_phonenum_record_t *gr = NULL;

	gr = sr_phonenum_get_record(name);

	if(gr == NULL)
		return;
	if(gr->record != NULL) {
		telnum_free(gr->record);
	}
	memset(gr, 0, sizeof(sr_phonenum_record_t));
}

int phonenum_update_pv(str *tomatch, str *name)
{
	sr_phonenum_record_t *gr = NULL;

	if(tomatch->len > 255) {
		LM_DBG("target too long (max 255): %s\n", tomatch->s);
		return -3;
	}

	gr = sr_phonenum_get_record(name);
	if(gr == NULL) {
		LM_DBG("container not found: %s\n", tomatch->s);
		return -4;
	}
	if(gr->record != NULL) {
		telnum_free(gr->record);
	}

	strncpy(gr->tomatch, tomatch->s, tomatch->len);
	gr->tomatch[tomatch->len] = '\0';
	LM_DBG("attempt to match: %s\n", gr->tomatch);
	gr->record = telnum_parse(gr->tomatch, "ZZ");
	if(gr->record == NULL) {
		LM_DBG("no match for: %s\n", gr->tomatch);
		return -2;
	}
	LM_DBG("phonenum PV updated for: %s (%d/%s/%s)\n", gr->tomatch,
			gr->record->valid,
			(gr->record->normalized)?gr->record->normalized:"none",
			(gr->record->error)?gr->record->error:"none");

	return 1;
}
