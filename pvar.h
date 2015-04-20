/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/*!
 * \file
 * \brief Kamailio core :: Definitions for Pseudo-variable support
 */


#ifndef _PVAR_H_
#define _PVAR_H_

#include "str.h"
#include "usr_avp.h"
#include "parser/msg_parser.h"

#define PV_MARKER_STR	"$"
#define PV_MARKER		'$'

#define PV_LNBRACKET_STR	"("
#define PV_LNBRACKET		'('
#define PV_RNBRACKET_STR	")"
#define PV_RNBRACKET		')'

#define PV_LIBRACKET_STR	"["
#define PV_LIBRACKET		'['
#define PV_RIBRACKET_STR	"]"
#define PV_RIBRACKET		']'

#define PV_VAL_NONE			0
#define PV_VAL_NULL			1
#define PV_VAL_EMPTY		2
#define PV_VAL_STR			4
#define PV_VAL_INT			8
#define PV_TYPE_INT			16
#define PV_VAL_PKG			32
#define PV_VAL_SHM			64

#define PV_NAME_INTSTR	0
#define PV_NAME_PVAR	1
#define PV_NAME_OTHER	2

#define PV_IDX_INT	0
#define PV_IDX_PVAR	1
#define PV_IDX_ALL	2
#define PV_IDX_ITR	3

/*! if PV name is dynamic, integer, or str */
#define pv_has_dname(pv) ((pv)->pvp.pvn.type==PV_NAME_PVAR)
#define pv_has_iname(pv) ((pv)->pvp.pvn.type==PV_NAME_INTSTR \
							&& !((pv)->pvp.pvn.u.isname.type&AVP_NAME_STR))
#define pv_has_sname(pv) ((pv)->pvp.pvn.type==PV_NAME_INTSTR \
							&& (pv)->pvp.pvn.u.isname.type&AVP_NAME_STR)
#define pv_is_w(pv)	((pv)->setf!=NULL)

enum _pv_type { 
	PVT_NONE=0,           PVT_EMPTY,             PVT_NULL, 
	PVT_MARKER,           PVT_AVP,               PVT_HDR,
	PVT_RURI,             PVT_RURI_USERNAME,     PVT_RURI_DOMAIN,
	PVT_DSTURI,           PVT_COLOR,             PVT_BRANCH,
	PVT_FROM,             PVT_TO,                PVT_OURI,
	PVT_SCRIPTVAR,        PVT_MSG_BODY,          PVT_CONTEXT,
	PVT_XAVP,             PVT_OTHER,             PVT_EXTRA /* keep it last */
};

typedef enum _pv_type pv_type_t;
typedef int pv_flags_t;

typedef void (*pv_name_free_f)(void*);

typedef struct _pv_value
{
	str rs;    /*!< string value */
	int ri;    /*!< integer value */
	int flags; /*!< flags about the type of value */
} pv_value_t, *pv_value_p;

typedef struct _pv_name
{
	int type;             /*!< type of name */
	pv_name_free_f nfree; /*!< function to free name structure */
	union {
		struct {
			int type;     /*!< type of int_str name - compatibility with AVPs */
			int_str name; /*!< the value of the name */
		} isname;
		void *dname;      /*!< PV value - dynamic name */
	} u;
} pv_name_t, *pv_name_p;

typedef struct _pv_index
{
	int type; /*!< type of PV index */
	union {
		int ival;   /*!< integer value */
		void *dval; /*!< PV value - dynamic index */
	} u;
} pv_index_t, *pv_index_p;

typedef struct _pv_param
{
	pv_name_t    pvn; /*!< PV name */
	pv_index_t   pvi; /*!< PV index */
} pv_param_t, *pv_param_p;

typedef int (*pv_getf_t) (struct sip_msg*,  pv_param_t*, pv_value_t*);
typedef int (*pv_setf_t) (struct sip_msg*,  pv_param_t*, int, pv_value_t*);

typedef struct _pv_spec {
	pv_type_t    type;   /*!< type of PV */
	pv_getf_t    getf;   /*!< get PV value function */
	pv_setf_t    setf;   /*!< set PV value function */
	pv_param_t   pvp;    /*!< parameter to be given to get/set functions */
	void         *trans; /*!< transformations */
} pv_spec_t, *pv_spec_p;

typedef int (*pv_parse_name_f)(pv_spec_p sp, str *in);
typedef int (*pv_parse_index_f)(pv_spec_p sp, str *in);
typedef int (*pv_init_param_f)(pv_spec_p sp, int param);

#define pv_alter_context(pv)	((pv)->type==PVT_CONTEXT \
									|| (pv)->type==PVT_BRANCH)

/*! \brief
 * PV spec format:
 * - $class_name
 * - $class_name(inner_name)
 * - $(class_name[index])
 * - $(class_name(inner_name)[index])
 * - $(class_name{transformation})
 * - $(class_name(inner_name){transformation})
 * - $(class_name[index]{transformation})
 * - $(class_name(inner_name)[index]{transformation})
 */
typedef struct _pv_export {
	str name;                      /*!< class name of PV */
	pv_type_t type;                /*!< type of PV */
	pv_getf_t  getf;               /*!< function to get the value */
	pv_setf_t  setf;               /*!< function to set the value */
	pv_parse_name_f parse_name;    /*!< function to parse the inner name */
	pv_parse_index_f parse_index;  /*!< function to parse the index of PV */
	pv_init_param_f init_param;    /*!< function to init the PV spec */
	int iparam;                    /*!< parameter for the init function */
} pv_export_t;

typedef struct _pv_elem
{
	str text;
	pv_spec_t *spec;
	struct _pv_elem *next;
} pv_elem_t, *pv_elem_p;

char* pv_parse_spec2(str *in, pv_spec_p sp, int silent);
#define pv_parse_spec(in, sp) pv_parse_spec2((in), (sp), 0)
int pv_get_spec_value(struct sip_msg* msg, pv_spec_p sp, pv_value_t *value);
int pv_set_spec_value(struct sip_msg* msg, pv_spec_p sp, int op,
		pv_value_t *value);
int pv_printf(struct sip_msg* msg, pv_elem_p list, char *buf, int *len);
int pv_elem_free_all(pv_elem_p log);
void pv_value_destroy(pv_value_t *val);
void pv_spec_destroy(pv_spec_t *spec);
void pv_spec_free(pv_spec_t *spec);
int pv_spec_dbg(pv_spec_p sp);
int pv_get_spec_index(struct sip_msg* msg, pv_param_p ip, int *idx, int *flags);
int pv_get_avp_name(struct sip_msg* msg, pv_param_p ip, int_str *avp_name,
		unsigned short *name_type);
int pv_parse_avp_name(pv_spec_p sp, str *in);
int pv_get_spec_name(struct sip_msg* msg, pv_param_p ip, pv_value_t *name);
int pv_parse_format(str *in, pv_elem_p *el);
int pv_parse_index(pv_spec_p sp, str *in);
int pv_init_iname(pv_spec_p sp, int param);
int pv_printf_s(struct sip_msg* msg, pv_elem_p list, str *s);
pv_spec_t* pv_spec_lookup(str *name, int *len);

typedef struct _pvname_list {
	pv_spec_t sname;
	struct _pvname_list *next;
} pvname_list_t, *pvname_list_p;

typedef struct pv_spec_list {
	pv_spec_p spec;
	struct pv_spec_list *next;
} pv_spec_list_t, *pv_spec_list_p;

pvname_list_t* parse_pvname_list(str *in, unsigned int type);

int register_pvars_mod(char *mod_name, pv_export_t *items);
int pv_free_extra_list(void);

int pv_locate_name(str *in);
pv_spec_t* pv_cache_get(str *name);
str* pv_cache_get_name(pv_spec_t *spec);

/*! \brief PV helper functions */
int pv_get_null(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

int pv_get_uintval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, unsigned int uival);
int pv_get_sintval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, int sival);
int pv_get_strval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, str *sval);
int pv_get_strzval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, char *sval);
int pv_get_strlval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, char *sval, int slen);
int pv_get_strintval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, str *sval, int ival);
int pv_get_intstrval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, int ival, str *sval);

/**
 * Core PV Cache
 */
typedef struct _pv_cache
{
	str pvname;
	unsigned int pvid;
	pv_spec_t spec;
	struct _pv_cache *next;
} pv_cache_t;

#define PV_CACHE_SIZE	32  /*!< pseudo-variables cache table size */

pv_cache_t **pv_cache_get_table(void);


/**
 * Transformations
 */
#define TR_LBRACKET_STR		"{"
#define TR_LBRACKET		'{'
#define TR_RBRACKET_STR		"}"
#define TR_RBRACKET		'}'
#define TR_CLASS_MARKER		'.'
#define TR_PARAM_MARKER		','

enum _tr_param_type { TR_PARAM_NONE=0, TR_PARAM_STRING, TR_PARAM_NUMBER,
	TR_PARAM_SPEC, TR_PARAM_SUBST, TR_PARAM_OTHER };

typedef struct _tr_param {
	int type;
	union {
		int n;
		str s;
		void *data;
	} v;
	struct _tr_param *next;
} tr_param_t, *tr_param_p;

typedef int (*tr_func_t) (struct sip_msg *, tr_param_t*, int, pv_value_t*);

typedef struct _trans {
	str name;
	int type;
	int subtype;
	tr_func_t trf;
	tr_param_t *params;
	struct _trans *next;
} trans_t, *trans_p;

typedef char* (*tr_parsef_t)(str *, trans_t *);
typedef struct _tr_export {
	str tclass;
	tr_parsef_t tparse; 
} tr_export_t, *tr_export_p;

char* tr_lookup(str *in, trans_t **tr);
tr_export_t* tr_lookup_class(str *tclass);
int tr_exec(struct sip_msg *msg, trans_t *t, pv_value_t *v);
void tr_param_free(tr_param_t *tp);

int register_trans_mod(char *mod_name, tr_export_t *items);


/**
 * XAVP
 */
typedef struct _pv_xavp_name {
	str name;
	pv_spec_t index;
	struct _pv_xavp_name *next;
} pv_xavp_name_t;

#endif

