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

#include "pv_core.h"
#include "pvar.h"
#include "str.h"

static int pv_get_retcode(struct sip_msg*, pv_param_t*, pv_value_t*);

static pv_export_t core_pvs[] = {
	/* return code, various synonims */
	{ STR_STATIC_INIT("?"), PVT_OTHER, pv_get_retcode, 0, 0, 0, 0, 0 },
	{ STR_STATIC_INIT("rc"), PVT_OTHER, pv_get_retcode, 0, 0, 0, 0, 0 },
	{ STR_STATIC_INIT("retcode"), PVT_OTHER, pv_get_retcode, 0, 0, 0, 0, 0 },
	
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/** ugly hack to get the return code, needed because the PVs do not know (yet)
   about the script context */
extern int _last_returned_code;

static int pv_get_retcode(struct sip_msg* msg, pv_param_t* p, pv_value_t* res)
{
	/* FIXME: as soon as PVs support script context, use it instead of the
	          return in global variable hack */
	return pv_get_sintval(msg, p, res, _last_returned_code);
}



/** register built-in core pvars.
 * should be called before parsing the config script.
 * @return 0 on success 
 */
int pv_register_core_vars(void)
{
	return register_pvars_mod("core", core_pvs);
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
