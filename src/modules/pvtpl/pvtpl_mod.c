/**
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_param.h"

MODULE_VERSION

static int pvtpl_init(void);
static int pvtpl_tpl_param(modparam_t type, void *val);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_pvtpl_render(sip_msg_t *msg, char *ptplname, char *popv);

/* clang-format off */
typedef struct pvtpl_item {
	str name;
	str fpath;
	str fdata;
	unsigned int bsize;
	pv_elem_t *pvm;
	str tplval;
	char *tplbuf;
	struct pvtpl_item *next;
} pvtpl_item_t;
/* clang-format on */

static pvtpl_item_t *_pvtpl_list = NULL;

/* clang-format off */
static cmd_export_t cmds[]={
	{"pvtpl_render", (cmd_function)w_pvtpl_render, 2, fixup_spve_pvar,
		fixup_free_spve_pvar, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"tpl", PARAM_STRING | PARAM_USE_FUNC, (void *)pvtpl_tpl_param},

	{0, 0, 0}
};

struct module_exports exports = {
	"pvtpl",         /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported RPC methods */
	0,               /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* per child init function */
	mod_destroy      /* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	if(pvtpl_init() < 0) {
		return -1;
	}

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
/**
 *
 */
static int pvtpl_file_read(str *fname, str *fdata)
{

	FILE *f;
	long fsize;
	char c = '\0';

	LM_DBG("reading from file: %.*s\n", fname->len, fname->s);

	fdata->s = NULL;
	fdata->len = 0;

	STR_VTOZ(fname->s[fname->len], c);
	f = fopen(fname->s, "r");
	STR_ZTOV(fname->s[fname->len], c);

	if(f == NULL) {
		LM_ERR("cannot open file: %.*s\n", fname->len, fname->s);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	if(fsize < 0) {
		LM_ERR("ftell failed on file: %.*s\n", fname->len, fname->s);
		fclose(f);
		return -1;
	}
	fseek(f, 0, SEEK_SET);

	fdata->s = pkg_malloc(fsize + 1);
	if(fdata->s == NULL) {
		LM_ERR("no more pkg memory\n");
		fclose(f);
		return -1;
	}
	if(fread(fdata->s, fsize, 1, f) != fsize) {
		if(ferror(f)) {
			LM_ERR("error reading from file: %.*s\n", fname->len, fname->s);
			fclose(f);
			return -1;
		}
	}
	fclose(f);

	fdata->s[fsize] = 0;
	fdata->len = (int)fsize;

	return 0;
}

/**
 *
 */
static int pvtpl_init(void)
{
	pvtpl_item_t *it;

	if(_pvtpl_list == NULL) {
		LM_ERR("no templates provided\n");
		return -1;
	}
	for(it = _pvtpl_list; it; it = it->next) {
		if(pvtpl_file_read(&it->fpath, &it->fdata) < 0) {
			return -1;
		}
		if(pv_parse_format(&it->fdata, &it->pvm) < 0) {
			LM_ERR("wrong format[%.*s]\n", it->fdata.len, it->fdata.s);
			return -1;
		}
		it->tplbuf = (char *)pkg_malloc(it->bsize * sizeof(char));
		if(it->tplbuf == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		it->tplbuf[0] = '\0';
	}

	return 0;
}
/**
 *
 */
static int pvtpl_tpl_param(modparam_t type, void *val)
{
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	pvtpl_item_t tmp;
	pvtpl_item_t *nt;
	str s;

	if(val == NULL)
		return -1;
	s.s = (char *)val;
	s.len = strlen(s.s);
	if(s.s[s.len - 1] == ';')
		s.len--;
	if(parse_params(&s, CLASS_ANY, &phooks, &params_list) < 0)
		return -1;
	memset(&tmp, 0, sizeof(pvtpl_item_t));
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncasecmp(pit->name.s, "name", 4) == 0) {
			tmp.name = pit->body;
		} else if(pit->name.len == 5
				  && strncasecmp(pit->name.s, "fpath", 5) == 0) {
			tmp.fpath = pit->body;
		} else if(pit->name.len == 5
				  && strncasecmp(pit->name.s, "bsize", 5) == 0) {
			if(tmp.bsize == 0) {
				if(str2int(&pit->body, &tmp.bsize) < 0) {
					LM_ERR("invalid bsize: %.*s\n", pit->body.len, pit->body.s);
					return -1;
				}
			}
		}
	}
	if(tmp.name.s == NULL || tmp.fpath.s == NULL) {
		LM_ERR("invalid template name or file path\n");
		free_params(params_list);
		return -1;
	}
	if(tmp.bsize == 0) {
		tmp.bsize = 1024;
	}
	/* check for same template */
	nt = _pvtpl_list;
	while(nt) {
		if(nt->name.len == tmp.name.len
				&& strncasecmp(nt->name.s, tmp.name.s, tmp.name.len) == 0)
			break;
		nt = nt->next;
	}
	if(nt != NULL) {
		LM_ERR("duplicate template with same name: %.*s\n", tmp.name.len,
				tmp.name.s);
		free_params(params_list);
		return -1;
	}

	nt = (pvtpl_item_t *)pkg_malloc(sizeof(pvtpl_item_t));
	if(nt == 0) {
		PKG_MEM_ERROR;
		free_params(params_list);
		return -1;
	}
	memcpy(nt, &tmp, sizeof(pvtpl_item_t));
	nt->next = _pvtpl_list;
	_pvtpl_list = nt;
	free_params(params_list);
	LM_DBG("created template item name=%.*s fpath=%.*s\n", tmp.name.len,
			tmp.name.s, tmp.fpath.len, tmp.fpath.s);
	return 0;
}

/**
 *
 */
static int pvtpl_render(sip_msg_t *msg, str *tplname, pv_spec_t *dst)
{
	pvtpl_item_t *it;
	pv_value_t val;

	for(it = _pvtpl_list; it; it = it->next) {
		if(it->name.len == tplname->len
				&& strncasecmp(it->name.s, tplname->s, tplname->len) == 0) {
			break;
		}
	}
	if(it == NULL) {
		LM_ERR("template not found: %.*s\n", tplname->len, tplname->s);
		return -1;
	}

	it->tplval.s = it->tplbuf;
	it->tplval.len = it->bsize - 1;

	if(pv_printf_s(msg, it->pvm, &it->tplval) != 0) {
		LM_ERR("failed to eval template: %.*s\n", tplname->len, tplname->s);
		return -1;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.rs = it->tplval;
	LM_DBG("template eval result: [[%.*s]]\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	dst->setf(msg, &dst->pvp, (int)EQ_T, &val);

	return 1;
}

/**
 *
 */
static int ki_pvtpl_render(sip_msg_t *msg, str *tplname, str *opv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(opv);

	if(dst == NULL) {
		LM_ERR("failed getting pv: %.*s\n", opv->len, opv->s);
		return -1;
	}

	return pvtpl_render(msg, tplname, dst);
}

/**
 *
 */
static int w_pvtpl_render(sip_msg_t *msg, char *ptplname, char *popv)
{
	str tplname;
	pv_spec_t *dst;

	if(fixup_get_svalue(msg, (gparam_t *)ptplname, &tplname) != 0) {
		LM_ERR("cannot get template value\n");
		return -1;
	}
	dst = (pv_spec_t *)popv;

	return pvtpl_render(msg, &tplname, dst);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_pvtpl_exports[] = {
	{ str_init("pvtpl"), str_init("pvtpl_render"),
		SR_KEMIP_INT, ki_pvtpl_render,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_pvtpl_exports);
	return 0;
}
