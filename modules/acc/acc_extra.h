/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 * Copyright (C) 2008 Juha Heinanen
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2004-10-28  first version (ramona)
 *  2005-05-30  acc_extra patch commited (ramona)
 *  2005-07-13  acc_extra specification moved to use pseudo-variables (bogdan)
 *  2006-09-08  flexible multi leg accounting support added,
 *              code cleanup for low level functions (bogdan)
 *  2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 *  2008-09-03  added support for integer type Radius attributes (jh)
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Extra attributes
 *
 * - \ref acc_extra.h
 * - Module: \ref acc
 */

#ifndef _ACC_EXTRA_H_
#define _ACC_EXTRA_H_

#include "acc_api.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../parser/msg_parser.h"
#include "../dialog/dlg_load.h"

void init_acc_extra(void);

struct acc_extra *parse_acc_extra(char *extra);

struct acc_extra *parse_acc_leg(char *extra);

void destroy_extras( struct acc_extra *extra);

int extra2strar( struct acc_extra *extra, struct sip_msg *rq,
		 str *val_arr, int *int_arr, char *type_arr);

int extra2strar_dlg_only(struct acc_extra *extra, struct dlg_cell* dlg,
		str *val_arr, int *int_arr, char *type_arr, const struct dlg_binds* p_dlgb);

int legs2strar( struct acc_extra *legs, struct sip_msg *rq, str *val_arr,
		int *int_arr, char *type_arr, int start);

int extra2int( struct acc_extra *extra, int *attrs );

#ifdef RAD_ACC
#include "../../lib/kcore/radius.h"
int extra2attrs( struct acc_extra *extra, struct attr *attrs, int offset);
#endif

static inline void free_strar_mem( char* type_arr, str* alloc_arr, int dim_arr, int dim_ext){
	int i = 0;
	for ( i = 0; i < dim_arr; i ++ ) {
		if (( TYPE_NULL !=  type_arr[i] ) && (  NULL != alloc_arr[i].s)) {
			LM_DBG("Freeing memory, type is %d, message_index %d, index i %d\n",
					type_arr[i], dim_ext - dim_arr, i);
			pkg_free( alloc_arr[i].s) ;
			alloc_arr[i].s = NULL;
		}
	}
}
#endif

