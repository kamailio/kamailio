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

#ifndef __COREPARAM_H__
#define __COREPARAM_H__

#include "str.h"

#define KSR_CPTYPE_NUM 1
#define KSR_CPTYPE_STR 2
#define KSR_CPTYPE_ID 4
#define KSR_CPTYPE_STRUCT 8

/* clang-format off */
typedef struct ksr_cpval {
	unsigned int vtype;		/* value type */
	union {
		long nval;			/* numeric (long) value */
		char *sval;			/* string value */
	} v;
} ksr_cpval_t;

typedef int (*ksr_cpexport_set_f)(str *name, ksr_cpval_t *xval, void *ep);

typedef struct ksr_cpexport {
	str name;					/* parameter name */
	unsigned int ptype;			/* parameter value type */
	ksr_cpexport_set_f setf;	/* function to set the parameter */
	void *ep;					/* extra parameter for set function */
} ksr_cpexport_t;
/* clang-format on */

int ksr_coreparam_set_nval(char *name, long nval);
int ksr_coreparam_set_sval(char *name, char *sval);
int ksr_coreparam_set_xval(char *name, ksr_cpval_t *xval);

int ksr_iuid_set(char *viuid, int mode);

#endif
