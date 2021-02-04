/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _SR_KEMI_H_
#define _SR_KEMI_H_

#include "str.h"
#include "parser/msg_parser.h"
#include "action.h"

#define SR_KEMIP_NONE	(0)		/* no type */
#define SR_KEMIP_INT	(1<<0)	/* type integer */
#define SR_KEMIP_STR	(1<<1)	/* type str* */
#define SR_KEMIP_BOOL	(1<<2)	/* type boolean (0/1) */
#define SR_KEMIP_XVAL	(1<<3)	/* type extended value (integer, str*, ...) */
#define SR_KEMIP_NULL	(1<<4)	/* type NULL */
#define SR_KEMIP_DICT	(1<<5)	/* type dictionary */
#define SR_KEMIP_ARRAY	(1<<6)	/* type array */

#define SR_KEMI_FALSE	0
#define SR_KEMI_TRUE	1

#define SR_KEMI_PARAMS_MAX	6

extern str kemi_onsend_route_callback;
extern str kemi_reply_route_callback;
extern str kemi_event_route_callback;

typedef struct sr_kemi {
	str mname; /* sub-module name */
	str fname; /* function name */
	int rtype; /* return type (supported SR_KEMIP_INT/BOOL) */
	void *func; /* pointer to the C function to be executed */
	int ptypes[SR_KEMI_PARAMS_MAX]; /* array with the type of parameters */
} sr_kemi_t;

typedef struct sr_kemi_item {
	sr_kemi_t *item;
	int nparams;
	struct sr_kemi_item *next;
} sr_kemi_item_t;

typedef struct sr_kemi_module {
	str mname;
	sr_kemi_t *kexp;
} sr_kemi_module_t;

typedef union {
	int n;
	str s;
} sr_kemi_val_t;

typedef struct sr_kemi_dict_item
{
	struct sr_kemi_dict_item *next;
	str name;
	int vtype;
	union {
		int n;
		str s;
		struct sr_kemi_dict_item *dict;
	} v;
} sr_kemi_dict_item_t;

typedef struct sr_kemi_xval {
	int vtype;
	union {
		int n;
		str s;
		sr_kemi_dict_item_t *dict;
	} v;
} sr_kemi_xval_t;

/* only sip_msg_t */
typedef int (*sr_kemi_fm_f)(sip_msg_t*);

/* sip_msg_t and one int|str param */
typedef int (*sr_kemi_fmn_f)(sip_msg_t*, int);
typedef int (*sr_kemi_fms_f)(sip_msg_t*, str*);

/* sip_msg_t and two int|str params */
typedef int (*sr_kemi_fmnn_f)(sip_msg_t*, int, int);
typedef int (*sr_kemi_fmns_f)(sip_msg_t*, int, str*);
typedef int (*sr_kemi_fmsn_f)(sip_msg_t*, str*, int);
typedef int (*sr_kemi_fmss_f)(sip_msg_t*, str*, str*);

/* sip_msg_t and three int|str params */
typedef int (*sr_kemi_fmnnn_f)(sip_msg_t*, int, int, int);
typedef int (*sr_kemi_fmnns_f)(sip_msg_t*, int, int, str*);
typedef int (*sr_kemi_fmnsn_f)(sip_msg_t*, int, str*, int);
typedef int (*sr_kemi_fmnss_f)(sip_msg_t*, int, str*, str*);
typedef int (*sr_kemi_fmsnn_f)(sip_msg_t*, str*, int, int);
typedef int (*sr_kemi_fmsns_f)(sip_msg_t*, str*, int, str*);
typedef int (*sr_kemi_fmssn_f)(sip_msg_t*, str*, str*, int);
typedef int (*sr_kemi_fmsss_f)(sip_msg_t*, str*, str*, str*);

/* sip_msg_t and four int|str params */
typedef int (*sr_kemi_fmnnnn_f)(sip_msg_t*, int, int, int, int);
typedef int (*sr_kemi_fmnnns_f)(sip_msg_t*, int, int, int, str*);
typedef int (*sr_kemi_fmnnsn_f)(sip_msg_t*, int, int, str*, int);
typedef int (*sr_kemi_fmnnss_f)(sip_msg_t*, int, int, str*, str*);
typedef int (*sr_kemi_fmnsnn_f)(sip_msg_t*, int, str*, int, int);
typedef int (*sr_kemi_fmnsns_f)(sip_msg_t*, int, str*, int, str*);
typedef int (*sr_kemi_fmnssn_f)(sip_msg_t*, int, str*, str*, int);
typedef int (*sr_kemi_fmnsss_f)(sip_msg_t*, int, str*, str*, str*);
typedef int (*sr_kemi_fmsnnn_f)(sip_msg_t*, str*, int, int, int);
typedef int (*sr_kemi_fmsnns_f)(sip_msg_t*, str*, int, int, str*);
typedef int (*sr_kemi_fmsnsn_f)(sip_msg_t*, str*, int, str*, int);
typedef int (*sr_kemi_fmsnss_f)(sip_msg_t*, str*, int, str*, str*);
typedef int (*sr_kemi_fmssnn_f)(sip_msg_t*, str*, str*, int, int);
typedef int (*sr_kemi_fmssns_f)(sip_msg_t*, str*, str*, int, str*);
typedef int (*sr_kemi_fmsssn_f)(sip_msg_t*, str*, str*, str*, int);
typedef int (*sr_kemi_fmssss_f)(sip_msg_t*, str*, str*, str*, str*);

/* sip_msg_t and five int|str params */
typedef int (*sr_kemi_fmsssss_f)(sip_msg_t*, str*, str*, str*, str*, str*);
typedef int (*sr_kemi_fmssssn_f)(sip_msg_t*, str*, str*, str*, str*, int);
typedef int (*sr_kemi_fmsssns_f)(sip_msg_t*, str*, str*, str*, int, str*);
typedef int (*sr_kemi_fmsssnn_f)(sip_msg_t*, str*, str*, str*, int, int);
typedef int (*sr_kemi_fmssnss_f)(sip_msg_t*, str*, str*, int, str*, str*);
typedef int (*sr_kemi_fmssnsn_f)(sip_msg_t*, str*, str*, int, str*, int);
typedef int (*sr_kemi_fmssnns_f)(sip_msg_t*, str*, str*, int, int, str*);
typedef int (*sr_kemi_fmssnnn_f)(sip_msg_t*, str*, str*, int, int, int);
typedef int (*sr_kemi_fmsnsss_f)(sip_msg_t*, str*, int, str*, str*, str*);
typedef int (*sr_kemi_fmsnssn_f)(sip_msg_t*, str*, int, str*, str*, int);
typedef int (*sr_kemi_fmsnsns_f)(sip_msg_t*, str*, int, str*, int, str*);
typedef int (*sr_kemi_fmsnsnn_f)(sip_msg_t*, str*, int, str*, int, int);
typedef int (*sr_kemi_fmsnnss_f)(sip_msg_t*, str*, int, int, str*, str*);
typedef int (*sr_kemi_fmsnnsn_f)(sip_msg_t*, str*, int, int, str*, int);
typedef int (*sr_kemi_fmsnnns_f)(sip_msg_t*, str*, int, int, int, str*);
typedef int (*sr_kemi_fmsnnnn_f)(sip_msg_t*, str*, int, int, int, int);
typedef int (*sr_kemi_fmnssss_f)(sip_msg_t*, int, str*, str*, str*, str*);
typedef int (*sr_kemi_fmnsssn_f)(sip_msg_t*, int, str*, str*, str*, int);
typedef int (*sr_kemi_fmnssns_f)(sip_msg_t*, int, str*, str*, int, str*);
typedef int (*sr_kemi_fmnssnn_f)(sip_msg_t*, int, str*, str*, int, int);
typedef int (*sr_kemi_fmnsnss_f)(sip_msg_t*, int, str*, int, str*, str*);
typedef int (*sr_kemi_fmnsnsn_f)(sip_msg_t*, int, str*, int, str*, int);
typedef int (*sr_kemi_fmnsnns_f)(sip_msg_t*, int, str*, int, int, str*);
typedef int (*sr_kemi_fmnsnnn_f)(sip_msg_t*, int, str*, int, int, int);
typedef int (*sr_kemi_fmnnsss_f)(sip_msg_t*, int, int, str*, str*, str*);
typedef int (*sr_kemi_fmnnssn_f)(sip_msg_t*, int, int, str*, str*, int);
typedef int (*sr_kemi_fmnnsns_f)(sip_msg_t*, int, int, str*, int, str*);
typedef int (*sr_kemi_fmnnsnn_f)(sip_msg_t*, int, int, str*, int, int);
typedef int (*sr_kemi_fmnnnss_f)(sip_msg_t*, int, int, int, str*, str*);
typedef int (*sr_kemi_fmnnnsn_f)(sip_msg_t*, int, int, int, str*, int);
typedef int (*sr_kemi_fmnnnns_f)(sip_msg_t*, int, int, int, int, str*);
typedef int (*sr_kemi_fmnnnnn_f)(sip_msg_t*, int, int, int, int, int);

/* sip_msg_t and six int|str params */
typedef int (*sr_kemi_fmssssss_f)(sip_msg_t*, str*, str*, str*, str*, str*, str*);

/* return xval, params only sip_msg_t */
typedef sr_kemi_xval_t* (*sr_kemi_xfm_f)(sip_msg_t*);

/* return xval, params sip_msg_t and one int|str param */
typedef sr_kemi_xval_t* (*sr_kemi_xfmn_f)(sip_msg_t*, int);
typedef sr_kemi_xval_t* (*sr_kemi_xfms_f)(sip_msg_t*, str*);

/* return xval, params sip_msg_t and two int|str params */
typedef sr_kemi_xval_t* (*sr_kemi_xfmnn_f)(sip_msg_t*, int, int);
typedef sr_kemi_xval_t* (*sr_kemi_xfmns_f)(sip_msg_t*, int, str*);
typedef sr_kemi_xval_t* (*sr_kemi_xfmsn_f)(sip_msg_t*, str*, int);
typedef sr_kemi_xval_t* (*sr_kemi_xfmss_f)(sip_msg_t*, str*, str*);

sr_kemi_t* sr_kemi_lookup(str *mname, int midx, str *fname);

int sr_kemi_modules_add(sr_kemi_t *klist);
int sr_kemi_modules_size_get(void);
sr_kemi_module_t* sr_kemi_modules_get(void);

typedef int (*sr_kemi_eng_route_f)(sip_msg_t*, int, str*, str*);

#define SR_KEMI_BNAME_SIZE	256
typedef struct sr_kemi_eng {
	char bname[SR_KEMI_BNAME_SIZE];
	str  ename;
	sr_kemi_eng_route_f froute;
} sr_kemi_eng_t;

int sr_kemi_eng_register(str *ename, sr_kemi_eng_route_f froute);
int sr_kemi_eng_set(str *ename, str *cpath);
int sr_kemi_eng_setz(char *ename, char *cpath);
sr_kemi_eng_t* sr_kemi_eng_get(void);

int sr_kemi_cbname_list_init(void);
int sr_kemi_cbname_lookup_name(str *name);
str* sr_kemi_cbname_lookup_idx(int idx);

void sr_kemi_act_ctx_set(run_act_ctx_t *ctx);
run_act_ctx_t* sr_kemi_act_ctx_get(void);

str *sr_kemi_param_map_get_name(int ptype);
str *sr_kemi_param_map_get_params(int *ptypes);

int sr_kemi_core_set_drop(sip_msg_t *msg);

int sr_kemi_route(sr_kemi_eng_t *keng, sip_msg_t *msg, int rtype,
		str *ename, str *edata);
int sr_kemi_ctx_route(sr_kemi_eng_t *keng, run_act_ctx_t *ctx, sip_msg_t *msg,
		int rtype, str *ename, str *edata);

sr_kemi_t* sr_kemi_exports_get_pv(void);

#define SR_KEMI_XVAL_NULL_NONE 0
#define SR_KEMI_XVAL_NULL_PRINT 1
#define SR_KEMI_XVAL_NULL_EMPTY 2
#define SR_KEMI_XVAL_NULL_ZERO 3
void sr_kemi_xval_null(sr_kemi_xval_t *xval, int rmode);
void sr_kemi_xval_free(sr_kemi_xval_t *xval);

/* functions exported to kemi that are used in other places */
int sr_kemi_hdr_remove(sip_msg_t *msg, str *hname);

#endif
