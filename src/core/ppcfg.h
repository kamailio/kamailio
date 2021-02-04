/*
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
 * \brief Kamailio core :: Config preprocessor directives
 * \ingroup core
 * Module: \ref core
 */

#ifndef _PPCFG_H_
#define _PPCFG_H_

#include "str.h"

typedef struct ksr_ppdefine {
	str name;
	str value;
	int dtype;
} ksr_ppdefine_t;

str* pp_get_define_name(int idx);
ksr_ppdefine_t* pp_get_define(int idx);

int pp_subst_add(char *data);
int pp_substdef_add(char *data, int mode);
int pp_subst_run(char **data);

int  pp_define(int len, const char *text);
int  pp_define_set(int len, char *text);
int  pp_define_set_type(int type);
str *pp_define_get(int len, const char * text);
int  pp_define_env(const char * text, int len);

void pp_ifdef_level_update(int val);
int pp_ifdef_level_check(void);
void pp_ifdef_level_error(void);

void pp_define_core(void);

void ksr_cfg_print_initial_state(void);

#endif /*_PPCFG_H_*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
