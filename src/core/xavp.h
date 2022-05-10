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


#ifndef _SR_XAVP_H_
#define _SR_XAVP_H_

#include <time.h>
#include "str.h"
#include "str_list.h"

struct _sr_xavp;

/* types for xavp values */
typedef enum {
	SR_XTYPE_NULL=0,
	SR_XTYPE_INT,     /* integer value */
	SR_XTYPE_STR,     /* str value */
	SR_XTYPE_TIME,    /* timestamp value */
	SR_XTYPE_LONG,    /* long value */
	SR_XTYPE_LLONG,   /* long long value */
	SR_XTYPE_XAVP,    /* xavp value */
	SR_XTYPE_VPTR,    /* void pointer value (no free on destroy) */
	SR_XTYPE_SPTR,    /* void pointer value (shm free on destroy) */
	SR_XTYPE_DATA     /* custom data value */
} sr_xtype_t;

typedef void (*sr_xavp_sfree_f)(void *d);
typedef void (*sr_data_free_f)(void *d, sr_xavp_sfree_f sfree);

/* structure custom data value */
typedef struct _sr_data {
	void *p;
	sr_data_free_f pfree;
} sr_data_t;

/* avp value */
typedef struct _sr_xval {
	sr_xtype_t type;           /* type of the value */
	union {
		int i;
		str s;                 /* cloned in shared memory */
		time_t t;
		long l;
		long long ll;
		struct _sr_xavp *xavp; /* must be given in shm (not cloned) */
		void *vptr;            /* void pointer - see SR_XTYPE_VPTR, SR_XTYPE_SPTR */
		sr_data_t *data;       /* must be given in shm (not cloned) */
	} v;
} sr_xval_t;

/* structure for extended avp */
typedef struct _sr_xavp {
	unsigned int id;          /* internal hash id */
	str name;                 /* name of the xavp */
	sr_xval_t val;            /* value of the xavp */
	struct _sr_xavp *next;    /* pointer to next xavp in list */
} sr_xavp_t;

int xavp_init_head(void);
void xavp_free(sr_xavp_t *xa);

int xavp_add(sr_xavp_t *xavp, sr_xavp_t **list);
int xavp_add_last(sr_xavp_t *xavp, sr_xavp_t **list);
int xavp_add_after(sr_xavp_t *nxavp, sr_xavp_t *pxavp);
sr_xavp_t *xavp_add_value(str *name, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavp_add_value_after(str *name, sr_xval_t *val, sr_xavp_t *pxavp);
sr_xavp_t *xavp_add_xavp_value(str *rname, str *name, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavp_set_value(str *name, int idx, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavp_get(str *name, sr_xavp_t *start);
sr_xavp_t *xavp_get_by_index(str *name, int idx, sr_xavp_t **start);
sr_xavp_t *xavp_get_next(sr_xavp_t *start);
sr_xavp_t *xavp_get_last(str *xname, sr_xavp_t **list);
int xavp_rm_by_name(str *name, int all, sr_xavp_t **head);
int xavp_rm_by_index(str *name, int idx, sr_xavp_t **head);
int xavp_rm(sr_xavp_t *xa, sr_xavp_t **head);
int xavp_rm_child_by_index(str *rname, str *cname, int idx);
int xavp_count(str *name, sr_xavp_t **start);
void xavp_destroy_list_unsafe(sr_xavp_t **head);
void xavp_destroy_list(sr_xavp_t **head);
void xavp_reset_list(void);
sr_xavp_t **xavp_set_list(sr_xavp_t **head);
sr_xavp_t **xavp_get_crt_list(void);
struct str_list *xavp_get_list_key_names(sr_xavp_t *xavp);

int xavp_insert(sr_xavp_t *xavp, int idx, sr_xavp_t **list);
sr_xavp_t *xavp_extract(str *name, sr_xavp_t **list);
int xavp_lshift(str *name, sr_xavp_t **head, int idx);

void xavp_print_list(sr_xavp_t **head);

sr_xavp_t *xavp_clone_level_nodata(sr_xavp_t *xold);
sr_xavp_t *xavp_clone_level_nodata_with_new_name(sr_xavp_t *xold, str *dst_name);

sr_xavp_t* xavp_get_child(str *rname, str *cname);
sr_xavp_t* xavp_get_child_with_ival(str *rname, str *cname);
sr_xavp_t* xavp_get_child_with_sval(str *rname, str *cname);
int xavp_serialize_fields(str *rname, char *obuf, int olen);

int xavp_set_child_ival(str *rname, str *cname, int ival);
int xavp_set_child_sval(str *rname, str *cname, str *sval);

/** xavu api */
void xavu_print_list_content(sr_xavp_t **head, int level);
void xavu_print_list(sr_xavp_t **head);
#define xavu_destroy_list_unsafe xavp_destroy_list_unsafe
#define xavu_destroy_list xavp_destroy_list
void xavu_reset_list(void);
sr_xavp_t **xavu_set_list(sr_xavp_t **head);
sr_xavp_t **xavu_get_crt_list(void);
sr_xavp_t *xavu_get(str *name, sr_xavp_t *start);
sr_xavp_t *xavu_lookup(str *name, sr_xavp_t **start);
int xavu_rm(sr_xavp_t *xa, sr_xavp_t **head);
int xavu_rm_by_name(str *name, sr_xavp_t **head);
int xavu_rm_child_by_name(str *rname, str *cname);
sr_xavp_t *xavu_set_xval(str *name, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavu_set_ival(str *rname, int ival);
sr_xavp_t *xavu_set_sval(str *rname, str *sval);
sr_xavp_t *xavu_set_child_xval(str *rname, str *cname, sr_xval_t *xval);
sr_xavp_t *xavu_set_child_ival(str *rname, str *cname, int ival);
sr_xavp_t *xavu_set_child_sval(str *rname, str *cname, str *sval);
sr_xavp_t* xavu_get_child(str *rname, str *cname);
sr_xavp_t* xavu_get_child_with_ival(str *rname, str *cname);
sr_xavp_t* xavu_get_child_with_sval(str *rname, str *cname);

int xavu_serialize_fields(str *rname, char *obuf, int olen);

/** xavi api */
int xavi_init_head(void);
#define xavi_free xavp_free

int xavi_add(sr_xavp_t *xavp, sr_xavp_t **list);
int xavi_add_last(sr_xavp_t *xavp, sr_xavp_t **list);
int xavi_add_after(sr_xavp_t *nxavp, sr_xavp_t *pxavp);
sr_xavp_t *xavi_add_value(str *name, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavi_add_value_after(str *name, sr_xval_t *val, sr_xavp_t *pxavp);
sr_xavp_t *xavi_add_xavi_value(str *rname, str *name, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavi_set_value(str *name, int idx, sr_xval_t *val, sr_xavp_t **list);
sr_xavp_t *xavi_get(str *name, sr_xavp_t *start);
sr_xavp_t *xavi_get_by_index(str *name, int idx, sr_xavp_t **start);
sr_xavp_t *xavi_get_next(sr_xavp_t *start);
sr_xavp_t *xavi_get_last(str *xname, sr_xavp_t **list);
int xavi_rm_by_name(str *name, int all, sr_xavp_t **head);
int xavi_rm_by_index(str *name, int idx, sr_xavp_t **head);
int xavi_rm(sr_xavp_t *xa, sr_xavp_t **head);
int xavi_rm_child_by_index(str *rname, str *cname, int idx);
int xavi_count(str *name, sr_xavp_t **start);
#define xavi_destroy_list_unsafe xavp_destroy_list_unsafe
#define xavi_destroy_list xavp_destroy_list
void xavi_reset_list(void);
sr_xavp_t **xavi_set_list(sr_xavp_t **head);
sr_xavp_t **xavi_get_crt_list(void);
struct str_list *xavi_get_list_key_names(sr_xavp_t *xavp);

int xavi_insert(sr_xavp_t *xavp, int idx, sr_xavp_t **list);
sr_xavp_t *xavi_extract(str *name, sr_xavp_t **list);

void xavi_print_list(sr_xavp_t **head);

sr_xavp_t *xavi_clone_level_nodata(sr_xavp_t *xold);
sr_xavp_t *xavi_clone_level_nodata_with_new_name(sr_xavp_t *xold, str *dst_name);

sr_xavp_t* xavi_get_child(str *rname, str *cname);
sr_xavp_t* xavi_get_child_with_ival(str *rname, str *cname);
sr_xavp_t* xavi_get_child_with_sval(str *rname, str *cname);
int xavi_serialize_fields(str *rname, char *obuf, int olen);

int xavi_set_child_ival(str *rname, str *cname, int ival);
int xavi_set_child_sval(str *rname, str *cname, str *sval);

#endif
