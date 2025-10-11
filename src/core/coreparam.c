/*
 * Copyright (C) 2025 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dprint.h"
#include "str.h"
#include "rand/ksrxrand.h"
#include "mem/pkg.h"
#include "coreparam.h"

int ksr_coreparam_store_nval(str *pname, ksr_cpval_t *pval, void *eparam);
int ksr_coreparam_store_sval_pkg(str *pname, ksr_cpval_t *pval, void *eparam);

int ksr_iuid_cp(str *pname, ksr_cpval_t *pval, void *eparam);

long ksr_timer_sanity_check = 0;
str _ksr_iuid = STR_NULL;

/* clang-format off */
static ksr_cpexport_t _ksr_cpexports[] = {
	{ str_init("iuid"), KSR_CPTYPE_STR,
		ksr_iuid_cp, NULL },
	{ str_init("random_engine"), KSR_CPTYPE_STR,
		ksr_xrand_cp, NULL },
	{ str_init("timer_sanity_check"), KSR_CPTYPE_NUM,
		ksr_coreparam_store_nval, &ksr_timer_sanity_check },
	{ {0, 0}, 0, NULL, NULL }
};
/* clang-format on */

/**
 *
 */
int ksr_coreparam_set_nval(char *name, long nval)
{
	ksr_cpval_t xval = {0};

	xval.vtype = KSR_CPTYPE_NUM;
	xval.v.nval = nval;

	return ksr_coreparam_set_xval(name, &xval);
}

/**
 *
 */
int ksr_coreparam_set_sval(char *name, char *sval)
{
	ksr_cpval_t xval = {0};

	xval.vtype = KSR_CPTYPE_STR;
	xval.v.sval = sval;

	return ksr_coreparam_set_xval(name, &xval);
}

/**
 *
 */
int ksr_coreparam_set_xval(char *name, ksr_cpval_t *xval)
{
	int i;
	str sname;

	sname.s = name;
	sname.len = strlen(sname.s);

	for(i = 0; _ksr_cpexports[i].name.s != NULL; i++) {
		if((_ksr_cpexports[i].name.len == sname.len)
				&& (strncmp(_ksr_cpexports[i].name.s, sname.s, sname.len)
						== 0)) {
			return _ksr_cpexports[i].setf(&sname, xval, _ksr_cpexports[i].ep);
		}
	}
	LM_ERR("core parameter [%.*s] not found\n", sname.len, sname.s);
	return -1;
}

/**
 *
 */
int ksr_coreparam_store_nval(str *pname, ksr_cpval_t *pval, void *eparam)
{
	*(long *)eparam = pval->v.nval;
	return 0;
}

/**
 * store the value in a str* variable, with v->s field allocated in pkg
 * - target v->s has to be initially null or pkg-alloced to cope properly
 *   with setting the parameters many times
 */
int ksr_coreparam_store_sval_pkg(str *pname, ksr_cpval_t *pval, void *eparam)
{
	str *v;

	v = (str *)eparam;
	if(v == NULL) {
		LM_ERR("store parameter not provided\n");
		return -1;
	}
	if(v->s != NULL) {
		pkg_free(v->s);
	}
	v->len = strlen(pval->v.sval);
	v->s = (char *)pkg_malloc(v->len + 1);
	if(v->s == NULL) {
		v->len = 0;
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(v->s, pval->v.sval, v->len);
	v->s[v->len] = '\0';

	return 0;
}

/**
 *
 */
int ksr_iuid_set(char *viuid, int mode)
{
	if(_ksr_iuid.s != NULL && mode == 0) {
		/* already set - do not overwrite */
		return 0;
	}
	if(_ksr_iuid.s != NULL) {
		pkg_free(_ksr_iuid.s);
	}
	_ksr_iuid.len = strlen(viuid);
	_ksr_iuid.s = (char *)pkg_malloc(_ksr_iuid.len + 1);
	if(_ksr_iuid.s == NULL) {
		_ksr_iuid.len = 0;
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(_ksr_iuid.s, viuid, _ksr_iuid.len);
	_ksr_iuid.s[_ksr_iuid.len] = '\0';

	return 0;
}

/**
 *
 */
int ksr_iuid_cp(str *pname, ksr_cpval_t *pval, void *eparam)
{
	return ksr_iuid_set(pval->v.sval, 0);
}
