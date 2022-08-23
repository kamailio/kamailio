/**
 * Copyright (C) 2010 Elena-Ramona Modroiu (asipto.com)
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

		       
#ifndef _MQUEUE_API_H_
#define _MQUEUE_API_H_

#include "../../core/pvar.h"
#include "../../core/parser/msg_parser.h"

/**
 *
 */
typedef struct _mq_item
{
	str key;
	str val;
	struct _mq_item *next;
} mq_item_t;

/**
 *
 */
typedef struct _mq_head
{
	str name;
	int msize;
	int csize;
	int dbmode;
	int addmode;
	gen_lock_t lock;
	mq_item_t *ifirst;
	mq_item_t *ilast;
	struct _mq_head *next;
} mq_head_t;

/**
 *
 */
typedef struct _mq_pv
{
	str *name;
	mq_item_t *item;
	struct _mq_pv *next;
} mq_pv_t;

mq_pv_t *mq_pv_get(str *name);
int pv_parse_mq_name(pv_spec_p sp, str *in);
int pv_get_mqk(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_mqv(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_mq_size(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
str* get_mqk(str *name);
str* get_mqv(str *name);
int mq_head_defined(void);
void mq_destroy(void);
int mq_head_add(str *name, int msize, int addmode);
int mq_head_fetch(str *name);
void mq_pv_free(str *name);
int mq_item_add(str *qname, str *key, str *val);
mq_head_t *mq_head_get(str *name);

int _mq_get_csize(str *);
int mq_set_dbmode(str *, int dbmode);

#endif

