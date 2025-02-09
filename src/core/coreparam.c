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
#include "rand/ksrxrand.h"
#include "coreparam.h"

/* clang-format off */
static ksr_cpexport_t _ksr_cpexports[] = {
	{ str_init("random_engine"), KSR_CPTYPE_STR,
		ksr_xrand_cp, NULL },

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
