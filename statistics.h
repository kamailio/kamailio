/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
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
/*
 * statistics compatibility wrapper for kamailio
 * for now it doesn't do anything
 * (obsolete, do not use anymore)
 *
 * History:
 * --------
 *  2008-11-17  initial version compatible with kamailio statistics.h (andrei)
 */

#ifndef _STATISTICS_H_
#define _STATISTICS_H_

#include "str.h"

#define STAT_NO_RESET  1
#define STAT_NO_SYNC   2
#define STAT_SHM_NAME  4
#define STAT_IS_FUNC   8



typedef unsigned int stat_val;
typedef unsigned long (*stat_function)(void);

typedef struct stat_var_{
	unsigned int mod_idx;
	str name;
	int flags;
	union{
		stat_val *val;
		stat_function f;
	}u;
	struct stat_var_ *hnext;
	struct stat_var_ *lnext;
} stat_var;


typedef struct stat_export_ {
	char* name;                /* null terminated statistic name */
	int flags;                 /* flags */
	stat_var** stat_pointer;   /* pointer to the variable's shm mem location */
} stat_export_t;

#define get_stat(name)  0
#define get_stat_val(v) 0
#define get_stat_var_from_num_code(num_code, in_code) 0
#define update_stat(v, n)
#define reset_stat(v)
#define if_update_stat(cond, v, n)

#ifdef STATISTICS
#warning "sorry sip-router does not support STATISTICS"
#endif

#endif /* _STATISTICS_H_ */
