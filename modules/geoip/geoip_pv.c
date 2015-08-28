/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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

#include "../../dprint.h"
#include "../../hashes.h"
#include "../../pvar.h"

#include "geoip_pv.h"

typedef struct _sr_geoip_record {
	GeoIPRecord *record;
	char *time_zone;
	char *region_name;
	char **range;
	char latitude[16];
	char longitude[16];
	char tomatch[256];
	int flags;
} sr_geoip_record_t;

typedef struct _sr_geoip_item {
	str pvclass;
	unsigned int hashid;
	sr_geoip_record_t r;
	struct _sr_geoip_item *next;
} sr_geoip_item_t;

typedef struct _geoip_pv {
	sr_geoip_item_t *item;
	int type;
} geoip_pv_t;

static GeoIP *_handle_GeoIP = NULL;

static sr_geoip_item_t *_sr_geoip_list = NULL;

sr_geoip_record_t *sr_geoip_get_record(str *name)
{
	sr_geoip_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid =  get_hash1_raw(name->s, name->len);

	it = _sr_geoip_list;
	while(it!=NULL)
	{
		if(it->hashid==hashid && it->pvclass.len == name->len
				&& strncmp(it->pvclass.s, name->s, name->len)==0)
			return &it->r;
		it = it->next;
	}
	return NULL;
}

sr_geoip_item_t *sr_geoip_add_item(str *name)
{
	sr_geoip_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid =  get_hash1_raw(name->s, name->len);

	it = _sr_geoip_list;
	while(it!=NULL)
	{
		if(it->hashid==hashid && it->pvclass.len == name->len
				&& strncmp(it->pvclass.s, name->s, name->len)==0)
			return it;
		it = it->next;
	}
	/* add new */
	it = (sr_geoip_item_t*)pkg_malloc(sizeof(sr_geoip_item_t));
	if(it==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(it, 0, sizeof(sr_geoip_item_t));
	it->pvclass.s = (char*)pkg_malloc(name->len+1);
	if(it->pvclass.s==NULL)
	{
		LM_ERR("no more pkg.\n");
		pkg_free(it);
		return NULL;
	}
	memcpy(it->pvclass.s, name->s, name->len);
	it->pvclass.s[name->len] = '\0';
	it->pvclass.len = name->len;
	it->hashid = hashid;
	it->next = _sr_geoip_list;
	_sr_geoip_list = it;
	return it;
}


int pv_parse_geoip_name(pv_spec_p sp, str *in)
{
	geoip_pv_t *gpv=NULL;
	char *p;
	str pvc;
	str pvs;
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	gpv = (geoip_pv_t*)pkg_malloc(sizeof(geoip_pv_t));
	if(gpv==NULL)
		return -1;

	memset(gpv, 0, sizeof(geoip_pv_t));

	p = in->s;

	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		goto error;
	pvc.s = p;
	while(p < in->s + in->len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}
	if(p>in->s+in->len || *p=='\0')
		goto error;
	pvc.len = p - pvc.s;
	if(*p!='=')
	{
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!='=')
			goto error;
	}
	p++;
	if(*p!='>')
		goto error;
	p++;

	pvs.len = in->len - (int)(p - in->s);
	pvs.s = p;
	LM_DBG("geoip [%.*s] - key [%.*s]\n", pvc.len, pvc.s,
			pvs.len, pvs.s);

	gpv->item = sr_geoip_add_item(&pvc);
	if(gpv->item==NULL)
		goto error;

	switch(pvs.len)
	{
		case 2: 
			if(strncmp(pvs.s, "cc", 2)==0)
				gpv->type = 0;
			else if(strncmp(pvs.s, "tz", 2)==0)
				gpv->type = 1;
			else goto error;
		break;
		case 3: 
			if(strncmp(pvs.s, "zip", 3)==0)
				gpv->type = 2;
			else if(strncmp(pvs.s, "lat", 3)==0)
				gpv->type = 3;
			else if(strncmp(pvs.s, "lon", 3)==0)
				gpv->type = 4;
			else if(strncmp(pvs.s, "dma", 3)==0)
				gpv->type = 5;
			else if(strncmp(pvs.s, "ips", 3)==0)
				gpv->type = 6;
			else if(strncmp(pvs.s, "ipe", 3)==0)
				gpv->type = 7;
			else goto error;
		break;
		case 4: 
			if(strncmp(pvs.s, "city", 4)==0)
				gpv->type = 8;
			else if(strncmp(pvs.s, "area", 4)==0)
				gpv->type = 9;
			else if(strncmp(pvs.s, "regc", 4)==0)
				gpv->type = 10;
			else if(strncmp(pvs.s, "regn", 4)==0)
				gpv->type = 11;
			else goto error;
		break;
		case 5: 
			if(strncmp(pvs.s, "metro", 5)==0)
				gpv->type = 12;
			else if(strncmp(pvs.s, "contc", 5)==0)
				gpv->type = 13;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.u.dname = (void*)gpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;

	return 0;

error:
	if(gpv!=NULL)
		pkg_free(gpv);

	LM_ERR("error at PV geoip name: %.*s\n", in->len, in->s);
	return -1;
}

int pv_geoip_get_strzval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, char *sval)
{
	str s;
	if(sval==NULL)
		return pv_get_null(msg, param, res);

	s.s = sval;
	s.len = strlen(s.s);
	return pv_get_strval(msg, param, res, &s);
}

int pv_get_geoip(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	geoip_pv_t *gpv;

	if(msg==NULL || param==NULL)
		return -1;

	gpv = (geoip_pv_t*)param->pvn.u.dname;
	if(gpv==NULL)
		return -1;
	if(gpv->item==NULL)
		return pv_get_null(msg, param, res);

	switch(gpv->type)
	{
		case 1: /* tz */
			if(gpv->item->r.time_zone==NULL)
			{
				if(gpv->item->r.flags&1)
					return pv_get_null(msg, param, res);
				if(gpv->item->r.record==NULL)
					return pv_get_null(msg, param, res);
				gpv->item->r.time_zone
					= (char*)GeoIP_time_zone_by_country_and_region(
						gpv->item->r.record->country_code,
						gpv->item->r.record->region);
				gpv->item->r.flags |= 1;
			}
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.time_zone);
		case 2: /* zip */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.record->postal_code);
		case 3: /* lat */
			if((gpv->item->r.flags&2)==0)
			{
				if(gpv->item->r.record==NULL)
					return pv_get_null(msg, param, res);
				snprintf(gpv->item->r.latitude, 15, "%f",
						gpv->item->r.record->latitude);
				gpv->item->r.flags |= 2;
			}
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.latitude);
		case 4: /* lon */
			if((gpv->item->r.flags&4)==0)
			{
				if(gpv->item->r.record==NULL)
					return pv_get_null(msg, param, res);
				snprintf(gpv->item->r.longitude, 15, "%f",
						gpv->item->r.record->longitude);
				gpv->item->r.flags |= 4;
			}
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.longitude);
		case 5: /* dma */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_sintval(msg, param, res,
					gpv->item->r.record->dma_code);
		case 6: /* ips */
		case 7: /* ipe */
			if((gpv->item->r.flags&8)==0)
			{
				gpv->item->r.range = GeoIP_range_by_ip(_handle_GeoIP,
					gpv->item->r.tomatch);
				gpv->item->r.flags |= 8;
			}
			if(gpv->item->r.range==NULL)
				return pv_get_null(msg, param, res);
			if(gpv->type==6)
				return pv_geoip_get_strzval(msg, param, res,
						gpv->item->r.range[0]);
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.range[1]);
		case 8: /* city */
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.record->city);
		case 9: /* area */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_sintval(msg, param, res,
					gpv->item->r.record->area_code);
		case 10: /* regc */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.record->region);
		case 11: /* regn */
			if((gpv->item->r.flags&16)==0)
			{
				if(gpv->item->r.record==NULL)
					return pv_get_null(msg, param, res);
				gpv->item->r.region_name
						= (char*)GeoIP_region_name_by_code(
							gpv->item->r.record->country_code,
							gpv->item->r.record->region);
				gpv->item->r.flags |= 16;
			}
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.region_name);
		case 12: /* metro */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_sintval(msg, param, res,
					gpv->item->r.record->metro_code);
		case 13: /* contc */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.record->continent_code);
		default: /* cc */
			if(gpv->item->r.record==NULL)
				return pv_get_null(msg, param, res);
			return pv_geoip_get_strzval(msg, param, res,
					gpv->item->r.record->country_code);
	}
}

int geoip_init_pv(char *path)
{
	_handle_GeoIP = GeoIP_open(path, GEOIP_MMAP_CACHE);
	
	if(_handle_GeoIP==NULL)
	{
		LM_ERR("cannot open GeoIP database file at: %s\n", path);
		return -1;
	}
	return 0;
}

void geoip_destroy_list(void)
{
}

void geoip_destroy_pv(void)
{
	if(_handle_GeoIP!=NULL)
	{
		GeoIP_delete(_handle_GeoIP);
		_handle_GeoIP=NULL;
	}
}

void geoip_pv_reset(str *name)
{
	sr_geoip_record_t *gr = NULL;
	
	gr = sr_geoip_get_record(name);

	if(gr==NULL)
		return;
	if(gr->range!=NULL)
		GeoIP_range_by_ip_delete(gr->range);
	if(gr->record!=NULL)
		GeoIPRecord_delete(gr->record);
	memset(gr, 0, sizeof(struct _sr_geoip_record));
}

int geoip_update_pv(str *tomatch, str *name)
{
	sr_geoip_record_t *gr = NULL;
	
	if(tomatch->len>255)
	{
		LM_DBG("target too long (max 255): %s\n", tomatch->s);
		return -3;
	}
	
	gr = sr_geoip_get_record(name);
	if(gr==NULL)
	{
		LM_DBG("container not found: %s\n", tomatch->s);
		return - 4;
	}

	strncpy(gr->tomatch, tomatch->s, tomatch->len);
	gr->tomatch[tomatch->len] = '\0';
	gr->record = GeoIP_record_by_name(_handle_GeoIP,
			(const char*)gr->tomatch);
	LM_DBG("attempt to match: %s\n", gr->tomatch);
	if (gr->record == NULL)
	{
		LM_DBG("no match for: %s\n", gr->tomatch);
		return -2;
	}
	LM_DBG("geoip PV updated for: %s\n", gr->tomatch);

	return 1;
}

