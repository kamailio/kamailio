/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#ifndef _LH_FORMAT_H_
#define _LH_FORMAT_H_

#include "../../core/str.h"
#include "lh_async.h"

#define LH_OPT_STR 1
#define LH_OPT_INT 2

typedef struct lh_format_opt
{
	const char *name;
	int type; /* LH_OPT_STR (str *) or LH_OPT_INT (int *) */
	void *dst;
} lh_format_opt_t;

/**
 * Output format (log server dialect).
 */
typedef struct lh_format
{
	const char *name;
	const char *content_type;
	const lh_format_opt_t *opts;
	int (*init)(void);
	int (*body_begin)(lh_body_t *body);
	int (*body_entry)(lh_body_t *body, const str *entry);
	int (*body_end)(lh_body_t *body);
} lh_format_t;

/* format selected by the "format" modparam */
extern const lh_format_t *lh_format;

int lh_format_parse(char *val);

int lh_format_set_default(void);

void lh_format_destroy(void);

#endif
