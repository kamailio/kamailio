/*
 * $Id$
 *
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
 *
 * History:
 * --------
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Core accounting
 *
 * - See \ref acc.c
 * - Module: \ref acc
 */

#ifndef _ACC_API_H_
#define _ACC_API_H_

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"

/* param trasnporter */
typedef struct acc_param {
	int code;
	str code_s;
	str reason;
	pv_elem_p  elem;
} acc_param_t;

/* various acc variables */
typedef struct acc_enviroment {
	unsigned int code;
	str code_s;
	str reason;
	struct hdr_field *to;
	str text;
	time_t ts;
	struct timeval tv;
} acc_enviroment_t;

/* acc extra parameter */
typedef struct acc_extra {
	str        name;       /*!< name (log comment/ column name) */
	pv_spec_t  spec;       /*!< value's spec */
	struct acc_extra *next;
} acc_extra_t;

typedef int (*core2strar_f)( struct sip_msg *req, str *c_vals,
			      int *i_vals, char *t_vals);
typedef int (*extra2strar_f)(struct acc_extra *extra, struct sip_msg *rq, str *val_arr,
		int *int_arr, char *type_arr);
typedef int (*legs2strar_f)( struct acc_extra *legs, struct sip_msg *rq, str *val_arr,
		int *int_arr, char *type_arr, int start);
typedef acc_extra_t* (*leg_info_f)(void);

/* acc event data structures */
typedef struct acc_info {
	acc_enviroment_t *env;
	str *varr;
	int *iarr;
	char *tarr;
	acc_extra_t *leg_info;
} acc_info_t;

/* acc engine initialization data structures */
typedef struct acc_init_info {
	acc_extra_t   *leg_info;
} acc_init_info_t;

typedef int (*acc_init_f)(acc_init_info_t *inf);
typedef int (*acc_req_f)(struct sip_msg *req, acc_info_t *data);

/* acc engine structure */
typedef struct acc_engine {
	char name[16];
	int flags;
	int acc_flag;
	int missed_flag;
	acc_init_f acc_init;
	acc_req_f  acc_req;
	struct acc_engine *next;
} acc_engine_t;

#define MAX_ACC_EXTRA 64
#define MAX_ACC_LEG   16
#define ACC_CORE_LEN  6


enum {TYPE_NULL = 0, TYPE_INT, TYPE_STR, TYPE_DOUBLE, TYPE_DATE};


typedef int (*register_engine_f)(acc_engine_t *eng);
typedef int (*acc_api_exec_f)(struct sip_msg *rq, acc_engine_t *eng,
		acc_param_t* comment);
typedef acc_extra_t* (*parse_extra_f)(char *extra_str);

/* the acc API */
typedef struct acc_api {
	leg_info_f    get_leg_info;
	core2strar_f  get_core_attrs;
	extra2strar_f get_extra_attrs;
	legs2strar_f  get_leg_attrs;
	parse_extra_f parse_extra;
	register_engine_f register_engine;
	acc_api_exec_f    exec;
} acc_api_t;

typedef int (*bind_acc_f)(acc_api_t* api);

int acc_run_engines(struct sip_msg *msg, int type, int *reset);
acc_engine_t *acc_api_get_engines(void);
void acc_api_set_arrays(acc_info_t *inf);


/**
 * @brief Load the SL API
 */
static inline int acc_load_api(acc_api_t *accb)
{
	bind_acc_f bindacc;

	bindacc = (bind_acc_f)find_export("bind_acc", 0, 0);
	if (bindacc == 0) {
		LM_ERR("cannot find bind_acc\n");
		return -1;
	}
	if (bindacc(accb)==-1)
	{
		LM_ERR("cannot bind acc api\n");
		return -1;
	}
	return 0;
}


#endif
