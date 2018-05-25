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
#define SR_KEMIP_INTSTR	(1<<3)	/* type integer or str* */

#define SR_KEMI_FALSE	0
#define SR_KEMI_TRUE	1

#define SR_KEMI_PARAMS_MAX	6

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

/* sip_msg_t and six int|str params */
typedef int (*sr_kemi_fmssssss_f)(sip_msg_t*, str*, str*, str*, str*, str*, str*);

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

int sr_kemi_core_drop(sip_msg_t *msg);

#endif
