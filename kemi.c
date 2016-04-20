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
#include "forward.h"
#include "locking.h"
#include "data_lump.h"
#include "data_lump_rpl.h"
#include "mem/shm.h"
#include "parser/parse_uri.h"

#include "kemi.h"


/**
 *
 */
static run_act_ctx_t *_sr_kemi_act_ctx = NULL;

/**
 *
 */
void sr_kemi_act_ctx_set(run_act_ctx_t *ctx)
{
	_sr_kemi_act_ctx = ctx;
}

/**
 *
 */
run_act_ctx_t* sr_kemi_act_ctx_get(void)
{
	return _sr_kemi_act_ctx;
}

/**
 *
 */
static int sr_kemi_core_dbg(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_DBG("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_err(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_ERR("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_info(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_INFO("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static int sr_kemi_core_drop(sip_msg_t *msg)
{
	if(_sr_kemi_act_ctx==NULL)
		return 0;
	LM_DBG("drop action executed inside embedded interpreter\n");
	_sr_kemi_act_ctx->run_flags |= EXIT_R_F|DROP_R_F;
	return 0;
}

/**
 *
 */
static int sr_kemi_core_is_myself(sip_msg_t *msg, str *uri)
{
	struct sip_uri puri;
	int ret;

	if(uri==NULL || uri->s==NULL) {
		return SR_KEMI_FALSE;
	}
	if(uri->len>4 && (strncmp(uri->s, "sip:", 4)==0
				|| strncmp(uri->s, "sips:", 5)==0)) {
		if(parse_uri(uri->s, uri->len, &puri)!=0) {
			LM_ERR("failed to parse uri [%.*s]\n", uri->len, uri->s);
			return SR_KEMI_FALSE;
		}
		ret = check_self(&puri.host, (puri.port.s)?puri.port_no:0,
				(puri.transport_val.s)?puri.proto:0);
	} else {
		ret = check_self(uri, 0, 0);
	}
	if(ret==1) {
		return SR_KEMI_TRUE;
	}
	return SR_KEMI_FALSE;
}

/**
 *
 */
static sr_kemi_t _sr_kemi_core[] = {
	{ str_init(""), str_init("dbg"),
		SR_KEMIP_NONE, sr_kemi_core_dbg,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("err"),
		SR_KEMIP_NONE, sr_kemi_core_err,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("info"),
		SR_KEMIP_NONE, sr_kemi_core_info,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("drop"),
		SR_KEMIP_NONE, sr_kemi_core_drop,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init(""), str_init("is_myself"),
		SR_KEMIP_BOOL, sr_kemi_core_is_myself,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
static int sr_kemi_hdr_append(sip_msg_t *msg, str *txt)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *hdr;

	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("append hf: %.*s\n", txt->len, txt->s);
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}

	hf = msg->last_header;
	hdr = (char*)pkg_malloc(txt->len);
	if(hdr==NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	anchor = anchor_lump(msg,
				hf->name.s + hf->len - msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, txt->len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_remove(sip_msg_t *msg, str *txt)
{
	struct lump* anchor;
	struct hdr_field *hf;

	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("remove hf: %.*s\n", txt->len, txt->s);
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}

	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len==txt->len
				&& strncasecmp(hf->name.s, txt->s, txt->len)==0) {
			anchor=del_lump(msg,
					hf->name.s - msg->buf, hf->len, 0);
			if (anchor==0) {
				LM_ERR("cannot remove hdr %.*s\n", txt->len, txt->s);
				return -1;
			}
		}
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_insert(sip_msg_t *msg, str *txt)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *hdr;

	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("insert hf: %.*s\n", txt->len, txt->s);
	hf = msg->headers;
	hdr = (char*)pkg_malloc(txt->len);
	if(hdr==NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(hdr, txt->s, txt->len);
	anchor = anchor_lump(msg, hf->name.s + hf->len - msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, txt->len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int sr_kemi_hdr_append_to_reply(sip_msg_t *msg, str *txt)
{
	if(txt==NULL || txt->s==NULL || msg==NULL)
		return -1;

	LM_DBG("append to reply: %.*s\n", txt->len, txt->s);

	if(add_lump_rpl(msg, txt->s, txt->len, LUMP_RPL_HDR)==0) {
		LM_ERR("unable to add reply lump\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static sr_kemi_t _sr_kemi_hdr[] = {
	{ str_init("hdr"), str_init("append"),
		SR_KEMIP_INT, sr_kemi_hdr_append,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("insert"),
		SR_KEMIP_INT, sr_kemi_hdr_insert,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("remove"),
		SR_KEMIP_INT, sr_kemi_hdr_remove,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("hdr"), str_init("append_to_reply"),
		SR_KEMIP_INT, sr_kemi_hdr_append_to_reply,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

#define SR_KEMI_MODULES_MAX_SIZE	1024
static int _sr_kemi_modules_size = 0;
static sr_kemi_module_t _sr_kemi_modules[SR_KEMI_MODULES_MAX_SIZE];

/**
 *
 */
int sr_kemi_modules_add(sr_kemi_t *klist)
{
	if(_sr_kemi_modules_size>=SR_KEMI_MODULES_MAX_SIZE) {
		return -1;
	}
	if(_sr_kemi_modules_size==0) {
		LM_DBG("adding core module\n");
		_sr_kemi_modules[_sr_kemi_modules_size].mname = _sr_kemi_core[0].mname;
		_sr_kemi_modules[_sr_kemi_modules_size].kexp = _sr_kemi_core;
		_sr_kemi_modules_size++;
		LM_DBG("adding hdr module\n");
		_sr_kemi_modules[_sr_kemi_modules_size].mname = _sr_kemi_hdr[0].mname;
		_sr_kemi_modules[_sr_kemi_modules_size].kexp = _sr_kemi_hdr;
		_sr_kemi_modules_size++;
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
sr_kemi_t* sr_kemi_lookup(str *mname, int midx, str *fname)
{
	int i;
	sr_kemi_t *ket;

	if(mname==NULL || mname->len<=0) {
		for(i=0; _sr_kemi_core[i].fname.s!=NULL; i++) {
			ket = &_sr_kemi_core[i];
			if(ket->fname.len==fname->len
					&& strncasecmp(ket->fname.s, fname->s, fname->len)==0) {
				return ket;
			}
		}
	} else {
		if(midx>0 && midx<SR_KEMI_MODULES_MAX_SIZE) {
			for(i=0; _sr_kemi_modules[midx].kexp[i].fname.s!=NULL; i++) {
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

	if(sr_kemi_cbname_list_init()<0) {
		return -1;
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

/**
 *
 */
sr_kemi_eng_t* sr_kemi_eng_get(void)
{
	return _sr_kemi_eng;
}

/**
 *
 */
#define KEMI_CBNAME_MAX_LEN	128
#define KEMI_CBNAME_LIST_SIZE	256

typedef struct sr_kemi_cbname {
	str name;
	char bname[KEMI_CBNAME_MAX_LEN];
} sr_kemi_cbname_t;

static gen_lock_t *_sr_kemi_cbname_lock = 0;
static sr_kemi_cbname_t *_sr_kemi_cbname_list = NULL;
static int *_sr_kemi_cbname_list_size = NULL;

/**
 *
 */
int sr_kemi_cbname_list_init(void)
{
	if(_sr_kemi_cbname_list!=NULL) {
		return 0;
	}
	if ( (_sr_kemi_cbname_lock=lock_alloc())==0) {
		LM_CRIT("failed to alloc lock\n");
		return -1;
	}
	if (lock_init(_sr_kemi_cbname_lock)==0 ) {
		LM_CRIT("failed to init lock\n");
		lock_dealloc(_sr_kemi_cbname_lock);
		_sr_kemi_cbname_lock = NULL;
		return -1;
	}
	_sr_kemi_cbname_list_size = shm_malloc(sizeof(int));
	if(_sr_kemi_cbname_list_size==NULL) {
		lock_destroy(_sr_kemi_cbname_lock);
		lock_dealloc(_sr_kemi_cbname_lock);
		LM_ERR("no more shared memory\n");
		return -1;
	}
	*_sr_kemi_cbname_list_size = 0;
	_sr_kemi_cbname_list
			= shm_malloc(KEMI_CBNAME_LIST_SIZE*sizeof(sr_kemi_cbname_t));
	if(_sr_kemi_cbname_list==NULL) {
		LM_ERR("no more shared memory\n");
		shm_free(_sr_kemi_cbname_list_size);
		_sr_kemi_cbname_list_size = NULL;
		lock_destroy(_sr_kemi_cbname_lock);
		lock_dealloc(_sr_kemi_cbname_lock);
		_sr_kemi_cbname_lock = NULL;
		return -1;
	}
	memset(_sr_kemi_cbname_list, 0,
			KEMI_CBNAME_LIST_SIZE*sizeof(sr_kemi_cbname_t));
	return 0;
}

/**
 *
 */
int sr_kemi_cbname_lookup_name(str *name)
{
	int n;
	int i;

	if(_sr_kemi_cbname_list==NULL) {
		return 0;
	}
	if(name->len >= KEMI_CBNAME_MAX_LEN) {
		LM_ERR("callback name is too long [%.*s] (max: %d)\n",
				name->len, name->s, KEMI_CBNAME_MAX_LEN);
		return 0;
	}
	n = *_sr_kemi_cbname_list_size;

	for(i=0; i<n; i++) {
		if(_sr_kemi_cbname_list[i].name.len==name->len
				&& strncmp(_sr_kemi_cbname_list[i].name.s,
						name->s, name->len)==0) {
			return i+1;
		}
	}

	/* not found -- add it */
	lock_get(_sr_kemi_cbname_lock);

	/* check if new callback were indexed meanwhile */
	for(; i<*_sr_kemi_cbname_list_size; i++) {
		if(_sr_kemi_cbname_list[i].name.len==name->len
				&& strncmp(_sr_kemi_cbname_list[i].name.s,
						name->s, name->len)==0) {
			return i+1;
		}
	}
	if(*_sr_kemi_cbname_list_size>=KEMI_CBNAME_LIST_SIZE) {
		lock_release(_sr_kemi_cbname_lock);
		LM_ERR("no more space to index callbacks\n");
		return 0;
	}
	strncpy(_sr_kemi_cbname_list[i].bname, name->s, name->len);
	_sr_kemi_cbname_list[i].bname[name->len] = '\0';
	_sr_kemi_cbname_list[i].name.s = _sr_kemi_cbname_list[i].bname;
	_sr_kemi_cbname_list[i].name.len = name->len;
	i++;
	*_sr_kemi_cbname_list_size = i;
	lock_release(_sr_kemi_cbname_lock);
	return i;
}

/**
 *
 */
str* sr_kemi_cbname_lookup_idx(int idx)
{
	int n;

	if(_sr_kemi_cbname_list==NULL) {
		return NULL;
	}
	n = *_sr_kemi_cbname_list_size;
	if(idx<1 || idx>n) {
		LM_ERR("index %d is out of range\n", idx);
		return NULL;
	}
	return &_sr_kemi_cbname_list[idx-1].name;
}
