/*
 * Copyright (C) 2007 Elena-Ramona Modroiu
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

#ifndef _SHVAR_H_
#define _SHVAR_H_

#include "../../sr_module.h"
#include "../../locking.h"
#include "../../lib/kmi/mi.h"
#include "pv_svar.h"

typedef struct sh_var {
	int n;                  /* Index of the variable */
	str name;               /* Name of the variable */
	script_val_t v;         /* Value of the variable */
#ifdef GEN_LOCK_T_PREFERED
	gen_lock_t *lock;       /* Lock for hash entry - fastlock */
#else
	int lockidx;            /* Lock index for hash entry - the rest*/
#endif
	struct sh_var *next;
} sh_var_t, *sh_var_p;

sh_var_t* set_shvar_value(sh_var_t *shv, int_str *value, int flags);
sh_var_t* get_shvar_by_name(str *name);

void reset_shvars(void);
void destroy_shvars(void);

#ifndef GEN_LOCK_T_PREFERED
void shvar_lock_idx(int idx);
void shvar_release_idx(int idx);
#endif

void lock_shvar(sh_var_t *shv);
void unlock_shvar(sh_var_t *shv);

int pv_parse_shvar_name(pv_spec_p sp, str *in);
int pv_get_shvar(struct sip_msg *msg,  pv_param_t *param, pv_value_t *res);
int pv_set_shvar(struct sip_msg* msg, pv_param_t *param, int op,
		pv_value_t *val);

int shvar_init_locks(void);
void shvar_destroy_locks(void);

struct mi_root* mi_shvar_get(struct mi_root* cmd_tree, void* param);
struct mi_root* mi_shvar_set(struct mi_root* cmd_tree, void* param);

int param_set_var( modparam_t type, void* val);
int param_set_shvar( modparam_t type, void* val);

void rpc_shv_get(rpc_t* rpc, void* c);
void rpc_shv_set(rpc_t* rpc, void* c);

#endif

