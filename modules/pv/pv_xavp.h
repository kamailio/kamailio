/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com) 
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
 
#ifndef _PV_XAVP_H_
#define _PV_XAVP_H_

#ifdef WITH_XAVP

#include "../../pvar.h"

int pv_get_xavp(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_set_xavp(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_xavp_name(pv_spec_p sp, str *in);

int pv_xavp_print(struct sip_msg* msg, char* s1, char *s2);

int xavp_params_explode(str *params, str *xname);

#endif
#endif
