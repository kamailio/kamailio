/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef PV_XBUFF_H_
#define PV_XBUFF_H_

#include "../../pvar.h"
#include "../../xavp.h"

#include <regex.h>
#include <ei.h>

typedef enum {
	XBUFF_ATTR_TYPE   = (1<<2),
	XBUFF_ATTR_FORMAT = (1<<3),
	XBUFF_ATTR_LENGTH = (1<<4),
	XBUFF_NO_IDX      = (1<<5)
} xbuff_attr_t;

typedef enum {
	XBUFF_TYPE_ATOM,
	XBUFF_TYPE_INT,
	XBUFF_TYPE_STR,
	XBUFF_TYPE_TUPLE,
	XBUFF_TYPE_LIST,
	XBUFF_TYPE_PID,
	XBUFF_TYPE_REF,
	XBUFF_TYPE_COUNT
} xbuff_type_t;

#define XBUFF_IDX_MASK      3

int pv_xbuff_parse_name(pv_spec_t *sp, str *in);
int pv_xbuff_new_xavp(sr_xavp_t **new, pv_value_t *pval, int *counter, char prefix);
int pv_xbuff_get_type(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, sr_xavp_t *avp);

sr_xavp_t *pv_xbuff_get_xbuff(str *name);
int xavp_decode(ei_x_buff *xbuff, int *index, sr_xavp_t **xavp,int level);
sr_xavp_t *xbuff_new(str *name);

extern str xbuff_attributes[];

extern str xbuff_types[];

#define xbuff_attr_name(flag) (xbuff_attributes[(flag)>>XBUFF_IDX_MASK])
#define xbuff_set_attr_flag(type,flag) type |= flag
#define xbuff_get_attr_flags(type) ((type)&~XBUFF_IDX_MASK)
#define xbuff_is_attr_set(flags) ((attr)&~XBUFF_NO_IDX)
#define xbuff_is_no_index(attr) ((attr)&XBUFF_NO_IDX)
#define xbuff_fix_index(type) ((type)&XBUFF_IDX_MASK)

int compile_xbuff_re();
int xbuff_match_type_re(str *s, xbuff_type_t *type, sr_xavp_t **addr);

int is_pv_xbuff_valid_char(char c);

sr_xavp_t *xbuff_copy_xavp(sr_xavp_t *xavp);

int pv_xbuff_set(struct sip_msg*, pv_param_t*, int, pv_value_t*);
int pv_xbuff_get(struct sip_msg*, pv_param_t*, pv_value_t*);
void free_xbuff_fmt_buff();

/* destroy all xbuffs */
void xbuff_destroy_all();

/**
 * atom,tuple,xbuf,pid and list
 */
extern regex_t xbuff_type_re;

/**
 * XAVP extension
 */

sr_xavp_t *xavp_new_value(str *name, sr_xval_t *val);
sr_xavp_t *xavp_get_nth(sr_xavp_t **list, int idx, sr_xavp_t **prv);
int xavp_get_count(sr_xavp_t *list);
int xavp_encode(ei_x_buff *xbuff, sr_xavp_t *xavp,int level);

void xbuff_data_free(void *p, sr_xavp_sfree_f sfree);

#endif /* PV_XBUFF_H_ */
