/*
 * Copyright (C) 2009 iptelorg GmbH
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
 */

/*!
 * \file
 * \brief Kamailio core ::  pv_core.c - pvars needed in the core, e.g. $?, $retcode
 *
 * \note Note: in general please avoid adding pvars directly to the core, unless
 * absolutely necessary (use/create a new module instead).
 * \ingroup core
 * Module: \ref core
 */

#include <stdlib.h>

#include "pv_core.h"
#include "pvar.h"
#include "pvapi.h"
#include "ppcfg.h"
#include "str.h"
#include "mem/pkg.h"


/** needed to get the return code, because the PVs do not know (yet)
 * about the script context */
extern int _last_returned_code;

static int pv_get_retcode(struct sip_msg *msg, pv_param_t *p, pv_value_t *res)
{
	return pv_get_sintval(msg, p, res, _last_returned_code);
}


static int pv_parse_env_name(pv_spec_p sp, str *in)
{
	char *csname;

	if(in->s == NULL || in->len <= 0)
		return -1;

	csname = pkg_malloc(in->len + 1);

	if(csname == NULL) {
		LM_ERR("no more pkg memory");
		return -1;
	}

	memcpy(csname, in->s, in->len);
	csname[in->len] = '\0';

	sp->pvp.pvn.u.dname = (void *)csname;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;
}

static int pv_get_env(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	char *val;
	char *csname = (char *)param->pvn.u.dname;

	if(csname) {
		val = getenv(csname);

		if(val) {
			return pv_get_strzval(msg, param, res, val);
		}
	}
	return pv_get_null(msg, param, res);
}

static int pv_parse_envn_name(pv_spec_p sp, str *in)
{
	char *csname;

	if(in->s == NULL || in->len <= 0)
		return -1;

	csname = pkg_malloc(in->len + 1);

	if(csname == NULL) {
		LM_ERR("no more pkg memory");
		return -1;
	}

	memcpy(csname, in->s, in->len);
	csname[in->len] = '\0';

	sp->pvp.pvn.u.dname = (void *)csname;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;
}

static int pv_get_envn(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	str val;
	int r = 0;
	char *csname = (char *)param->pvn.u.dname;

	if(csname) {
		val.s = getenv(csname);
		if(val.s) {
			val.len = strlen(val.s);
			str2sint(&val, &r);
			return pv_get_intstrval(msg, param, res, r, &val);
		}
	}
	return pv_get_null(msg, param, res);
}

static int pv_parse_def_name(pv_spec_p sp, str *in)
{
	if(in == NULL || in->s == NULL || sp == NULL) {
		LM_ERR("INVALID DEF NAME\n");
		return -1;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;
	return 0;
}

static int pv_get_def(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	str *val = pp_define_get(
			param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);

	if(val) {
		return pv_get_strval(msg, param, res, val);
	}
	return pv_get_null(msg, param, res);
}

static int pv_parse_defn_name(pv_spec_p sp, str *in)
{
	if(in == NULL || in->s == NULL || sp == NULL) {
		LM_ERR("INVALID DEF NAME\n");
		return -1;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;
	return 0;
}

static int pv_get_defn(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	int n = 0;
	str *val = pp_define_get(
			param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);

	if(val) {
		str2sint(val, &n);
		return pv_get_intstrval(msg, param, res, n, val);
	} else {
		return pv_get_sintval(msg, param, res, n);
	}
}

static int pv_parse_defv_name(pv_spec_p sp, str *in)
{
	if(in == NULL || in->s == NULL || sp == NULL) {
		LM_ERR("INVALID DEF NAME\n");
		return -1;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;
	return 0;
}

static int pv_get_defv(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	str *val = NULL;
	str ret = STR_NULL;

	val = pp_define_get(
			param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);

	if(!val) {
		return pv_get_null(msg, param, res);
	}

	if(val->len < 2) {
		return pv_get_strval(msg, param, res, val);
	}
	if((val->s[0] == '"' && val->s[val->len - 1] == '"')
			|| (val->s[0] == '\'' && val->s[val->len - 1] == '\'')) {
		ret.s = pv_get_buffer();
		ret.len = pv_get_buffer_size();
		if(ret.len < val->len) {
			return pv_get_null(msg, param, res);
		}
		memcpy(ret.s, val->s + 1, val->len - 2);
		ret.len = val->len - 2;
		ret.s[ret.len] = '\0';
		return pv_get_strval(msg, param, res, &ret);
	} else {
		return pv_get_strval(msg, param, res, val);
	}
}

/* clang-format off */
/**
 *
 */
static pv_export_t core_pvs[] = {
	/* return code, various synonims */
	{STR_STATIC_INIT("?"), PVT_OTHER, pv_get_retcode, 0, 0, 0, 0, 0},
	{STR_STATIC_INIT("rc"), PVT_OTHER, pv_get_retcode, 0, 0, 0, 0, 0},
	{STR_STATIC_INIT("retcode"), PVT_OTHER, pv_get_retcode, 0, 0, 0, 0, 0},
	{STR_STATIC_INIT("env"), PVT_OTHER, pv_get_env, 0, pv_parse_env_name, 0,
			0, 0},
	{STR_STATIC_INIT("envn"), PVT_OTHER, pv_get_envn, 0, pv_parse_envn_name,
			0, 0, 0},
	{STR_STATIC_INIT("def"), PVT_OTHER, pv_get_def, 0, pv_parse_def_name, 0,
			0, 0},
	{STR_STATIC_INIT("defn"), PVT_OTHER, pv_get_defn, 0, pv_parse_defn_name,
			0, 0, 0},
	{STR_STATIC_INIT("defv"), PVT_OTHER, pv_get_defv, 0, pv_parse_defv_name,
			0, 0, 0},

	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};
/* clang-format on */

/**
 * register built-in core pvars.
 * should be called before parsing the config script.
 * @return 0 on success
 */
int pv_register_core_vars(void)
{
	return register_pvars_mod("core", core_pvs);
}

/**
 *
 */
int pv_eval_str(sip_msg_t *msg, str *dst, str *src)
{
	pv_elem_t *xmodel = NULL;
	str sval = STR_NULL;

	if(pv_parse_format(src, &xmodel) < 0) {
		LM_ERR("error in parsing src parameter\n");
		return -1;
	}

	if(pv_printf_s(msg, xmodel, &sval) != 0) {
		LM_ERR("cannot eval parsed parameter\n");
		pv_elem_free_all(xmodel);
		goto error;
	}

	dst->s = sval.s;
	dst->len = sval.len;
	pv_elem_free_all(xmodel);

	return 1;
error:
	return -1;
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
