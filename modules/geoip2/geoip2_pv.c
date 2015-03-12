/**
 * $Id$
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

#include "geoip2_pv.h"

typedef struct _sr_geoip2_record {
	MMDB_lookup_result_s record;
	str time_zone;
	str zip;
	str city;
	str region_code;
	str region_name;
	str country;
	str cont_code;
	char latitude[16];
	char longitude[16];
	char metro[16];
	char nmask[8];
	char tomatch[256];
	int flags;
} sr_geoip2_record_t;

typedef struct _sr_geoip2_item {
	str pvclass;
	unsigned int hashid;
	sr_geoip2_record_t r;
	struct _sr_geoip2_item *next;
} sr_geoip2_item_t;

typedef struct _geoip2_pv {
	sr_geoip2_item_t *item;
	int type;
} geoip2_pv_t;

static MMDB_s _handle_GeoIP;

static sr_geoip2_item_t *_sr_geoip2_list = NULL;

sr_geoip2_record_t *sr_geoip2_get_record(str *name)
{
	sr_geoip2_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid =  get_hash1_raw(name->s, name->len);

	it = _sr_geoip2_list;
	while(it!=NULL)
	{
		if(it->hashid==hashid && it->pvclass.len == name->len
				&& strncmp(it->pvclass.s, name->s, name->len)==0)
			return &it->r;
		it = it->next;
	}
	return NULL;
}

sr_geoip2_item_t *sr_geoip2_add_item(str *name)
{
	sr_geoip2_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid =  get_hash1_raw(name->s, name->len);

	it = _sr_geoip2_list;
	while(it!=NULL)
	{
		if(it->hashid==hashid && it->pvclass.len == name->len
				&& strncmp(it->pvclass.s, name->s, name->len)==0)
			return it;
		it = it->next;
	}
	/* add new */
	it = (sr_geoip2_item_t*)pkg_malloc(sizeof(sr_geoip2_item_t));
	if(it==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(it, 0, sizeof(sr_geoip2_item_t));
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
	it->next = _sr_geoip2_list;
	_sr_geoip2_list = it;
	return it;
}


int pv_parse_geoip2_name(pv_spec_p sp, str *in)
{
	geoip2_pv_t *gpv=NULL;
	char *p;
	str pvc;
	str pvs;
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	gpv = (geoip2_pv_t*)pkg_malloc(sizeof(geoip2_pv_t));
	if(gpv==NULL)
		return -1;

	memset(gpv, 0, sizeof(geoip2_pv_t));

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
	LM_DBG("geoip2 [%.*s] - key [%.*s]\n", pvc.len, pvc.s,
			pvs.len, pvs.s);

	gpv->item = sr_geoip2_add_item(&pvc);
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
			else goto error;
		break;
		case 4: 
			if(strncmp(pvs.s, "city", 4)==0)
				gpv->type = 8;
			else if(strncmp(pvs.s, "regc", 4)==0)
				gpv->type = 10;
			else if(strncmp(pvs.s, "regn", 4)==0)
				gpv->type = 11;
			else goto error;
		break;
		case 5: 
			if(strncmp(pvs.s, "metro", 5)==0)
				gpv->type = 12;
			else if(strncmp(pvs.s, "nmask", 5)==0)
				gpv->type = 13;
			else if(strncmp(pvs.s, "contc", 5)==0)
				gpv->type = 6;
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

	LM_ERR("error at PV geoip2 name: %.*s\n", in->len, in->s);
	return -1;
}

int pv_geoip2_get_strzval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, char *sval)
{
	str s;
	if(sval==NULL)
		return pv_get_null(msg, param, res);

	s.s = sval;
	s.len = strlen(s.s);
	return pv_get_strval(msg, param, res, &s);
}

int pv_get_geoip2(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	geoip2_pv_t *gpv;
	MMDB_entry_data_s entry_data;

	if(msg==NULL || param==NULL)
		return -1;

	gpv = (geoip2_pv_t*)param->pvn.u.dname;
	if(gpv==NULL)
		return -1;
	if(gpv->item==NULL)
		return pv_get_null(msg, param, res);

	switch(gpv->type)
	{
		case 1: /* tz */
			if(gpv->item->r.time_zone.s==NULL)
			{
				if(gpv->item->r.flags&1)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"location","time_zone", NULL
					) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.time_zone.s = (char *)entry_data.utf8_string;
					gpv->item->r.time_zone.len = entry_data.data_size;
				}
				gpv->item->r.flags |= 1;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.time_zone);
		case 2: /* zip */
			if(gpv->item->r.zip.s==NULL)
			{
				if(gpv->item->r.flags&32)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"postal","code", NULL) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.zip.s = (char *)entry_data.utf8_string;
					gpv->item->r.zip.len = entry_data.data_size;
				}
				gpv->item->r.flags |= 32;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.zip);
		case 3: /* lat */
			if((gpv->item->r.flags&2)==0)
			{
				gpv->item->r.latitude[0] = '\0';
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"location","latitude", NULL) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_DOUBLE)
					snprintf(gpv->item->r.latitude, 15, "%f", entry_data.double_value);
				gpv->item->r.flags |= 2;
			}
			return pv_geoip2_get_strzval(msg, param, res,
					gpv->item->r.latitude);
		case 4: /* lon */
			if((gpv->item->r.flags&4)==0)
			{
				gpv->item->r.latitude[0] = '\0';
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"location","longitude", NULL) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_DOUBLE)
					snprintf(gpv->item->r.longitude, 15, "%f", entry_data.double_value);
				gpv->item->r.flags |= 4;
			}
			return pv_geoip2_get_strzval(msg, param, res,
					gpv->item->r.longitude);
		case 6: /* contc */
			if(gpv->item->r.cont_code.s==NULL)
			{
				if(gpv->item->r.flags&16)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"continent","code", NULL
					) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.cont_code.s = (char *)entry_data.utf8_string;
					gpv->item->r.cont_code.len = entry_data.data_size;
				}
				gpv->item->r.flags |= 16;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.cont_code);
		case 8: /* city */
			if(gpv->item->r.city.s==NULL)
			{
				if(gpv->item->r.flags&64)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"city","names","en", NULL
					) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.city.s = (char *)entry_data.utf8_string;
					gpv->item->r.city.len = entry_data.data_size;
				}
				gpv->item->r.flags |= 64;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.city);
		case 10: /* regc */
			if(gpv->item->r.region_code.s==NULL)
			{
				if(gpv->item->r.flags&128)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"subdivisions","0","iso_code", NULL
					) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.region_code.s = (char *)entry_data.utf8_string;
					gpv->item->r.region_code.len = entry_data.data_size;
				}
				gpv->item->r.flags |= 128;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.region_code);
		case 11: /* regn */
			if(gpv->item->r.region_name.s==NULL)
			{
				if(gpv->item->r.flags&16)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"subdivisions","0","names","en", NULL
					) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.region_name.s = (char *)entry_data.utf8_string;
					gpv->item->r.region_name.len = entry_data.data_size;
				}
				gpv->item->r.flags |= 16;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.region_name);
		case 12: /* metro */
			if((gpv->item->r.flags&256)==0)
			{
				gpv->item->r.metro[0] = '\0';
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"location","metro_code", NULL) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UINT16)
					snprintf(gpv->item->r.metro, 15, "%hd", entry_data.uint16);
				gpv->item->r.flags |= 256;
			}
			return pv_geoip2_get_strzval(msg, param, res,
					gpv->item->r.metro);
		case 13: /* nmask */
			if((gpv->item->r.flags&1024)==0)
			{
				gpv->item->r.nmask[0] = '\0';
				snprintf(gpv->item->r.nmask, 8, "%hd", gpv->item->r.record.netmask);
				gpv->item->r.flags |= 1024;
			}
			return pv_geoip2_get_strzval(msg, param, res,
					gpv->item->r.nmask);
		default: /* cc */
			if(gpv->item->r.country.s==NULL)
			{
				if(gpv->item->r.flags&512)
					return pv_get_null(msg, param, res);
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"country","iso_code", NULL
					) != MMDB_SUCCESS)
					return pv_get_null(msg, param, res);
				if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
					gpv->item->r.country.s = (char *)entry_data.utf8_string;
					gpv->item->r.country.len = entry_data.data_size;
				}
				if(MMDB_get_value(&gpv->item->r.record.entry, &entry_data,
					"traits","is_anonymous_proxy", NULL) == MMDB_SUCCESS
					&& entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_BOOLEAN
					&& entry_data.boolean) {
					gpv->item->r.country.s = "A1";
					gpv->item->r.country.len = 2;
				}
				gpv->item->r.flags |= 512;
			}
			return pv_get_strval(msg, param, res, &gpv->item->r.country);
	}
}

int geoip2_init_pv(char *path)
{
	int status = MMDB_open(path, MMDB_MODE_MMAP, &_handle_GeoIP);
	
	if(MMDB_SUCCESS != status)
	{
		LM_ERR("cannot open GeoIP database file at: %s\n", path);
		return -1;
	}
	return 0;
}

void geoip2_destroy_list(void)
{
}

void geoip2_destroy_pv(void)
{
	MMDB_close(&_handle_GeoIP);
}

void geoip2_pv_reset(str *name)
{
	sr_geoip2_record_t *gr = NULL;
	
	gr = sr_geoip2_get_record(name);

	if(gr==NULL)
		return;
	memset(gr, 0, sizeof(struct _sr_geoip2_record));
}

int geoip2_update_pv(str *tomatch, str *name)
{
	sr_geoip2_record_t *gr = NULL;
	int gai_error, mmdb_error;
	
	if(tomatch->len>255)
	{
		LM_DBG("target too long (max 255): %s\n", tomatch->s);
		return -3;
	}
	
	gr = sr_geoip2_get_record(name);
	if(gr==NULL)
	{
		LM_DBG("container not found: %s\n", tomatch->s);
		return - 4;
	}

	strncpy(gr->tomatch, tomatch->s, tomatch->len);
	tomatch->s[tomatch->len] = '\0';
	gr->record = MMDB_lookup_string(&_handle_GeoIP,
			(const char*)gr->tomatch,
			&gai_error, &mmdb_error
			);
	LM_DBG("attempt to match: %s\n", gr->tomatch);
	if (gai_error || MMDB_SUCCESS != mmdb_error || !gr->record.found_entry)
	{
		LM_DBG("no match for: %s\n", gr->tomatch);
		return -2;
	}
	LM_DBG("geoip2 PV updated for: %s\n", gr->tomatch);

	return 1;
}

