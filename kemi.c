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
