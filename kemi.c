/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "dprint.h"

#include "kemi.h"

#define SR_KEMI_MODULES_MAX_SIZE	1024
static int _sr_kemi_modules_size = 1;
static sr_kemi_module_t _sr_kemi_modules[SR_KEMI_MODULES_MAX_SIZE];

/**
 *
 */
int sr_kemi_modules_add(sr_kemi_t *klist)
{
	if(_sr_kemi_modules_size>=SR_KEMI_MODULES_MAX_SIZE) {
		return -1;
	}
	LM_DBG("adding module: %.*s\n", klist[0].mname.len, klist[0].mname.s);
	_sr_kemi_modules[_sr_kemi_modules_size].mname = klist[0].mname;
	_sr_kemi_modules[_sr_kemi_modules_size].kexp = klist;
	_sr_kemi_modules_size++;
	return 0;
}

/**
 *
 */
int sr_kemi_modules_size_get(void)
{
	return _sr_kemi_modules_size;
}

/**
 *
 */
sr_kemi_module_t* sr_kemi_modules_get(void)
{
	return _sr_kemi_modules;
}

/**
 *
 */
static int lua_sr_kemi_dbg(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_DBG("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int lua_sr_kemi_err(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_ERR("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int lua_sr_kemi_info(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_INFO("%.*s", txt->len, txt->s);
	return 0;
}


/**
 *
 */
static sr_kemi_t _sr_kemi_core[] = {
	{ str_init(""), str_init("dbg"),
		SR_KEMIP_NONE, lua_sr_kemi_dbg,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("err"),
		SR_KEMIP_NONE, lua_sr_kemi_err,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("info"),
		SR_KEMIP_NONE, lua_sr_kemi_info,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
sr_kemi_t* sr_kemi_lookup(str *mname, int midx, str *fname)
{
	int i;
	sr_kemi_t *ket;

	if(mname==NULL || mname->len<=0) {
		for(i=0; ; i++) {
			ket = &_sr_kemi_core[i];
			if(ket->fname.len==fname->len
					&& strncasecmp(ket->fname.s, fname->s, fname->len)==0) {
				return ket;
			}
		}
	} else {
		if(midx>0 && midx<SR_KEMI_MODULES_MAX_SIZE) {
			for(i=0; ; i++) {
				ket = &_sr_kemi_modules[midx].kexp[i];
				if(ket->fname.len==fname->len
						&& strncasecmp(ket->fname.s, fname->s, fname->len)==0) {
					return ket;
				}
			}
		}
	}
	return NULL;
}

/**
 *
 */

#define SR_KEMI_ENG_LIST_MAX_SIZE	8
static sr_kemi_eng_t _sr_kemi_eng_list[SR_KEMI_ENG_LIST_MAX_SIZE];
sr_kemi_eng_t *_sr_kemi_eng = NULL;
static int _sr_kemi_eng_list_size=0;

/**
 *
 */
int sr_kemi_eng_register(str *ename, sr_kemi_eng_route_f froute)
{
	int i;

	for(i=0; i<_sr_kemi_eng_list_size; i++) {
		if(_sr_kemi_eng_list[i].ename.len==ename->len
				&& strncasecmp(_sr_kemi_eng_list[i].ename.s, ename->s,
					ename->len)==0) {
			/* found */
			return 1;
		}
	}
	if(_sr_kemi_eng_list_size>=SR_KEMI_ENG_LIST_MAX_SIZE) {
		LM_ERR("too many config routing engines registered\n");
		return -1;
	}
	if(ename->len>=SR_KEMI_BNAME_SIZE) {
		LM_ERR("config routing engine name too long\n");
		return -1;
	}
	strncpy(_sr_kemi_eng_list[_sr_kemi_eng_list_size].bname,
			ename->s, ename->len);
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].ename.s
			= _sr_kemi_eng_list[_sr_kemi_eng_list_size].bname;
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].ename.len = ename->len;
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].ename.s[ename->len] = 0;
	_sr_kemi_eng_list[_sr_kemi_eng_list_size].froute = froute;
	_sr_kemi_eng_list_size++;

	LM_DBG("registered config routing enginge [%.*s]",
			ename->len, ename->s);

	return 0;
}

/**
 *
 */
int sr_kemi_eng_set(str *ename, str *cpath)
{
	int i;

	/* skip native and default */
	if(ename->len==6 && strncasecmp(ename->s, "native", 6)==0) {
		return 0;
	}
	if(ename->len==7 && strncasecmp(ename->s, "default", 7)==0) {
		return 0;
	}

	for(i=0; i<_sr_kemi_eng_list_size; i++) {
		if(_sr_kemi_eng_list[i].ename.len==ename->len
				&& strncasecmp(_sr_kemi_eng_list[i].ename.s, ename->s,
					ename->len)==0) {
			/* found */
			_sr_kemi_eng = &_sr_kemi_eng_list[i];
			return 0;
		}
	}
	return -1;
}

/**
 *
 */
int sr_kemi_eng_setz(char *ename, char *cpath)
{
	str sname;
	str spath;

	sname.s = ename;
	sname.len = strlen(ename);

	if(cpath!=0) {
		spath.s = cpath;
		spath.len = strlen(cpath);
		return sr_kemi_eng_set(&sname, &spath);
	} else {
		return sr_kemi_eng_set(&sname, NULL);
	}
}


sr_kemi_eng_t* sr_kemi_eng_get(void)
{
	return _sr_kemi_eng;
}
