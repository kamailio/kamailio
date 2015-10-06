/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of kamailio, a free SIP server.
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
 */

/*!
 * \file
 * \brief Dialog variables
 * \ingroup dialog
 * Module: \ref dialog
 */
		       
#include "../../route.h"
#include "../../script_cb.h"
#include "../../pvapi.h"

#include "dlg_var.h"
#include "dlg_hash.h"
#include "dlg_profile.h"
#include "dlg_handlers.h"
#include "dlg_db_handler.h"

dlg_ctx_t _dlg_ctx;
extern int spiral_detected;

/*! global variable table, in case the dialog does not exist yet */
static struct dlg_var *_dlg_var_table = 0;
/*! ID of the current message */
int msg_id;

int dlg_cfg_cb(sip_msg_t *msg, unsigned int flags, void *cbp)
{
	dlg_cell_t *dlg;
	if(flags&POST_SCRIPT_CB) {
		dlg = dlg_get_ctx_dialog();
		if(dlg!=NULL) {
			if(_dlg_ctx.t==0 && (dlg->state==DLG_STATE_UNCONFIRMED
						|| _dlg_ctx.expect_t==1)) {
				if(_dlg_ctx.cpid!=0 && _dlg_ctx.cpid==my_pid()) {
					/* release to destroy dialog if created by this process
					 * and request was not forwarded */
					if(dlg->state==DLG_STATE_UNCONFIRMED) {
						LM_DBG("new dialog with no transaction after config"
									" execution\n");
					} else {
						LM_DBG("dialog with no expected transaction after"
								" config execution\n");
					}
					dlg_release(dlg);
				}
			}
			/* get ctx dlg increased ref count - release now */
			dlg_release(dlg);
		}
	}
	memset(&_dlg_ctx, 0, sizeof(dlg_ctx_t));

	return 1;
}

int cb_dlg_cfg_reset(sip_msg_t *msg, unsigned int flags, void *cbp)
{
	memset(&_dlg_ctx, 0, sizeof(dlg_ctx_t));

	return 1;
}

int cb_dlg_locals_reset(sip_msg_t *msg, unsigned int flags, void *cbp)
{
	LM_DBG("resetting the local dialog shortcuts on script callback: %u\n", flags);
	cb_dlg_cfg_reset(msg, flags, cbp);
	cb_profile_reset(msg, flags, cbp);

	return 1;
}

static inline struct dlg_var *new_dlg_var(str *key, str *val)
{
	struct dlg_var *var;

	var =(struct dlg_var*)shm_malloc(sizeof(struct dlg_var));
	if (var==NULL) {
		LM_ERR("no more shm mem\n");
		return NULL;
	}
	memset(var, 0, sizeof(struct dlg_var));
	var->vflags = DLG_FLAG_NEW;
	/* set key */
	var->key.len = key->len;
	var->key.s = (char*)shm_malloc(var->key.len+1);
	if (var->key.s==NULL) {
		shm_free(var);			
		LM_ERR("no more shm mem\n");
		return NULL;
	}
	memcpy(var->key.s, key->s, key->len);
	var->key.s[var->key.len] = '\0';
	/* set value */
	var->value.len = val->len;
	var->value.s = (char*)shm_malloc(var->value.len+1);
	if (var->value.s==NULL) {
		shm_free(var->key.s);			
		shm_free(var);			
		LM_ERR("no more shm mem\n");
		return NULL;
	}
	memcpy(var->value.s, val->s, val->len);
	var->value.s[var->value.len] = '\0';
	return var;
}

/*! Delete the current var-list */
void free_local_varlist() {
	struct dlg_var *var;
	while (_dlg_var_table) {
		var = _dlg_var_table;
		_dlg_var_table = _dlg_var_table->next;
		shm_free(var->key.s);
		shm_free(var->value.s);
		shm_free(var);
	}
	_dlg_var_table = NULL;
}

/*! Retrieve the local var-list pointer */
struct dlg_var * get_local_varlist_pointer(struct sip_msg *msg, int clear_pointer) {
	struct dlg_var *var;
	/* New list, delete the old one */
	if (msg->id != msg_id) {
		free_local_varlist();
		msg_id = msg->id;
	}
	var = _dlg_var_table;
	if (clear_pointer)
		_dlg_var_table = NULL;
	return var;
}

/* Adds, updates and deletes dialog variables */
int set_dlg_variable_unsafe(struct dlg_cell *dlg, str *key, str *val)
{
	struct dlg_var * var = NULL;
	struct dlg_var * it;
	struct dlg_var * it_prev;
	struct dlg_var ** var_list;
	
	if (dlg) 
		var_list = &dlg->vars;
	else 
		var_list = &_dlg_var_table;

	if ( val && (var=new_dlg_var(key, val))==NULL) {
		LM_ERR("failed to create new dialog variable\n");
		return -1;
	}

	/* iterate the list */
	for( it_prev=NULL, it=*var_list ; it ; it_prev=it,it=it->next) {
		if (key->len==it->key.len && memcmp(key->s,it->key.s,key->len)==0
			&& (it->vflags & DLG_FLAG_DEL) == 0) {
			/* found -> replace or delete it */
			if (val==NULL) {
				/* delete it */
				if (it_prev) it_prev->next = it->next;
				else *var_list = it->next;
				/* Set the delete-flag for the current var: */
				it->vflags &= DLG_FLAG_DEL;
			} else {
				/* replace the current it with var and free the it */
				var->next = it->next;
				/* Take the previous vflags: */
				var->vflags = it->vflags & DLG_FLAG_CHANGED;
				if (it_prev) it_prev->next = var;
				else *var_list = var;				  
			}

			/* Free this var: */
			shm_free(it->key.s);
			shm_free(it->value.s);
			shm_free(it);
			return 0;
		}
	}

	/* not found: */
	if (!var) {
		LM_DBG("dialog variable <%.*s> does not exist in variable list\n", key->len, key->s);
		return 1;
	}

	/* insert a new one at the beginning of the list */
	var->next = *var_list;
	*var_list = var;

	return 0;
}

str * get_dlg_variable_unsafe(struct dlg_cell *dlg, str *key)
{
	struct dlg_var *var, *var_list;

	if (dlg) 
		var_list = dlg->vars;
	else
		var_list = _dlg_var_table;

	/* iterate the list */
	for(var=var_list ; var ; var=var->next) {
		if (key->len==var->key.len && memcmp(key->s,var->key.s,key->len)==0
		&& (var->vflags & DLG_FLAG_DEL) == 0) {
			return &var->value;
		}
	}

	return NULL;
}

int pv_parse_dialog_var_name(pv_spec_p sp, str *in)
{
	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;

	return 0;
}

/*! Internal debugging function: Prints the list of dialogs */
void print_lists(struct dlg_cell *dlg) {
	struct dlg_var *varlist;
	varlist = _dlg_var_table;
	LM_DBG("Internal var-list (%p):\n", varlist);
	while (varlist) {
		LM_DBG("%.*s=%.*s (flags %i)\n",
			varlist->key.len, varlist->key.s,
			varlist->value.len, varlist->value.s,
			varlist->vflags);
		varlist = varlist->next;
	}
	if (dlg) {
		varlist = dlg->vars;
		LM_DBG("Dialog var-list (%p):\n", varlist);
		while (varlist) {
			LM_DBG("%.*s=%.*s (flags %i)\n",
				varlist->key.len, varlist->key.s,
				varlist->value.len, varlist->value.s,
				varlist->vflags);
			varlist = varlist->next;
		}
	}
}

str * get_dlg_variable(struct dlg_cell *dlg, str *key)
{
    str* var = NULL;

    if( !dlg || !key || key->len > strlen(key->s))
    {
        LM_ERR("BUG - bad parameters\n");

        return NULL;
    }

    dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));
    var = get_dlg_variable_unsafe( dlg, key);
    dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));

    return var;
}

int set_dlg_variable(struct dlg_cell *dlg, str *key, str *val)
{
    int ret = -1;
    if( !dlg || !key || key->len > strlen(key->s) || (val && val->len > strlen(val->s)))
    {
        LM_ERR("BUG - bad parameters\n");
        return -1;
    }

    dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));

    ret = set_dlg_variable_unsafe(dlg, key, val);
    if(ret!= 0)
        goto done;

    dlg->dflags |= DLG_FLAG_CHANGED_VARS;
    dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
    if ( dlg_db_mode==DB_MODE_REALTIME )
        update_dialog_dbinfo(dlg);

    print_lists(dlg);

    return 0;

done:
    dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
    return ret;
}

int pv_get_dlg_variable(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	dlg_cell_t *dlg;
	str * value;
	str spv;

	if (param==NULL || param->pvn.type!=PV_NAME_INTSTR
			|| param->pvn.u.isname.type!=AVP_NAME_STR
			|| param->pvn.u.isname.name.s.s==NULL) {
		LM_CRIT("BUG - bad parameters\n");
		return -1;
	}

	/* Retrieve the dialog for current message */
	dlg=dlg_get_msg_dialog( msg);

	if (dlg) {
		/* Lock the dialog */
		dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));
	} else {
		/* Verify the local list */
		get_local_varlist_pointer(msg, 0);
	}

	/* dcm: todo - the value should be cloned for safe usage */
	value = get_dlg_variable_unsafe(dlg, &param->pvn.u.isname.name.s);

	spv.s = NULL;
	if(value) {
		spv.len = pv_get_buffer_size();
		if(spv.len<value->len+1) {
			LM_ERR("pv buffer too small (%d) - needed %d\n", spv.len, value->len);
		} else {
			spv.s = pv_get_buffer();
			strncpy(spv.s, value->s, value->len);
			spv.len = value->len;
			spv.s[spv.len] = '\0';
		}
	}

	print_lists(dlg);

	/* unlock dialog */
	if (dlg) {
		dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
		dlg_release(dlg);
	}

	if (spv.s)
		return pv_get_strval(msg, param, res, &spv);


	return pv_get_null(msg, param, res);
}

int pv_set_dlg_variable(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val)
{
	dlg_cell_t *dlg = NULL;
	int ret = -1;

	if (param==NULL || param->pvn.type!=PV_NAME_INTSTR
			|| param->pvn.u.isname.type!=AVP_NAME_STR
			|| param->pvn.u.isname.name.s.s==NULL ) {
		LM_CRIT("BUG - bad parameters\n");
		goto error;
	}

	/* Retrieve the dialog for current message */
	dlg=dlg_get_msg_dialog( msg);
	
	if (dlg) {
		/* Lock the dialog */
		dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));
	} else {
		/* Verify the local list */
		get_local_varlist_pointer(msg, 0);
	}

	if (val==NULL || val->flags&(PV_VAL_NONE|PV_VAL_NULL|PV_VAL_EMPTY)) {
		/* if NULL, remove the value */
		ret = set_dlg_variable_unsafe(dlg, &param->pvn.u.isname.name.s, NULL);
		if(ret!= 0) {
			/* unlock dialog */
			if (dlg) {
				dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
				dlg_release(dlg);
			}
			return ret;
		}
	} else {
		/* if value, must be string */
		if ( !(val->flags&PV_VAL_STR)) {
			LM_ERR("non-string values are not supported\n");
			/* unlock dialog */
			if (dlg) dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
			goto error;
		}

		ret = set_dlg_variable_unsafe(dlg, &param->pvn.u.isname.name.s, &val->rs);
		if(ret!= 0) {
			/* unlock dialog */
			if (dlg) dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
			goto error;
		}
	}
	/* unlock dialog */
	if (dlg) {
		dlg->dflags |= DLG_FLAG_CHANGED_VARS;
		dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
		if ( dlg_db_mode==DB_MODE_REALTIME )
			update_dialog_dbinfo(dlg);

	}
	print_lists(dlg);

	dlg_release(dlg);
	return 0;
error:
	dlg_release(dlg);
	return -1;
}

int pv_get_dlg_ctx(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	if(param==NULL)
		return -1;
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.flags);
		case 2:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.timeout);
		case 3:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.to_bye);
		case 4:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.to_route);
		case 5:
			_dlg_ctx.set = (_dlg_ctx.iuid.h_id==0)?0:1;
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.set);
		case 6:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dir);
		default:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.on);
	}
	return 0;
}

int pv_set_dlg_ctx(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int n;
	char *rtp;

	if(param==NULL)
		return -1;

	n = 0;
	if(val!=NULL && val->flags&PV_VAL_INT)
		n = val->ri;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			_dlg_ctx.flags = n;
		break;
		case 2:
			_dlg_ctx.timeout = n;
		break;
		case 3:
			_dlg_ctx.to_bye = n;
		break;
		case 4:
			if(val && val->flags&PV_VAL_STR) {
				if(val->rs.s[val->rs.len]=='\0'
						&& val->rs.len<DLG_TOROUTE_SIZE) {
					_dlg_ctx.to_route = route_lookup(&main_rt, val->rs.s);
					strcpy(_dlg_ctx.to_route_name, val->rs.s);
				} else _dlg_ctx.to_route = 0;
			} else {
				if(n!=0) {
					rtp = int2str(n, NULL);
					_dlg_ctx.to_route = route_lookup(&main_rt, rtp);
					strcpy(_dlg_ctx.to_route_name, rtp);
				} else _dlg_ctx.to_route = 0;
			}
			if(_dlg_ctx.to_route <0) _dlg_ctx.to_route = 0;
		break;
		default:
			_dlg_ctx.on = n;
		break;
	}
	return 0;
}

int pv_parse_dlg_ctx_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 2: 
			if(strncmp(in->s, "on", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 3: 
			if(strncmp(in->s, "set", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "dir", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "flags", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "timeout", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		case 11: 
			if(strncmp(in->s, "timeout_bye", 11)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 13:
			if(strncmp(in->s, "timeout_route", 13)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV name %.*s\n", in->len, in->s);
	return -1;
}

int pv_get_dlg(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	dlg_cell_t *dlg = NULL;
	int res_type = 0;
	str sv = { 0 };
	unsigned int ui = 0;

	if(param==NULL)
		return -1;

	if(_dlg_ctx.iuid.h_id==0)
	{
		/* Retrieve the dialog for current message */
		dlg=dlg_get_msg_dialog(msg);
	} else {
		/* Retrieve the dialog for current context */
		dlg=dlg_get_by_iuid(&_dlg_ctx.iuid);
	}
	if(dlg == NULL)
		return pv_get_null(msg, param, res);

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			res_type = 1;
			ui = (unsigned int)dlg->h_id;
			break;
		case 2:
			res_type = 1;
			ui = (unsigned int)dlg->state;
			break;
		case 3:
			if(dlg->route_set[DLG_CALLEE_LEG].s==NULL
					|| dlg->route_set[DLG_CALLEE_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->route_set[DLG_CALLEE_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->route_set[DLG_CALLEE_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 4:
			res_type = 1;
			ui = (unsigned int)dlg->dflags;
			break;
		case 5:
			res_type = 1;
			ui = (unsigned int)dlg->sflags;
			break;
		case 6:
			if(dlg->callid.s==NULL
					|| dlg->callid.len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->callid.len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->callid.s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 7:
			if(dlg->to_uri.s==NULL
					|| dlg->to_uri.len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->to_uri.len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->to_uri.s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 8:
			if(dlg->tag[DLG_CALLEE_LEG].s==NULL
					|| dlg->tag[DLG_CALLEE_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->tag[DLG_CALLEE_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->tag[DLG_CALLEE_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 9:
			res_type = 1;
			ui = (unsigned int)dlg->toroute;
			break;
		case 10:
			if(dlg->cseq[DLG_CALLEE_LEG].s==NULL
					|| dlg->cseq[DLG_CALLEE_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->cseq[DLG_CALLEE_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->cseq[DLG_CALLEE_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 11:
			if(dlg->route_set[DLG_CALLER_LEG].s==NULL
					|| dlg->route_set[DLG_CALLER_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->route_set[DLG_CALLER_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->route_set[DLG_CALLER_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 12:
			if(dlg->from_uri.s==NULL
					|| dlg->from_uri.len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->from_uri.len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->from_uri.s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 13:
			if(dlg->tag[DLG_CALLER_LEG].s==NULL
					|| dlg->tag[DLG_CALLER_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->tag[DLG_CALLER_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->tag[DLG_CALLER_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 14:
			res_type = 1;
			ui = (unsigned int)dlg->lifetime;
			break;
		case 15:
			res_type = 1;
			ui = (unsigned int)dlg->start_ts;
			break;
		case 16:
			if(dlg->cseq[DLG_CALLER_LEG].s==NULL
					|| dlg->cseq[DLG_CALLER_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->cseq[DLG_CALLER_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->cseq[DLG_CALLER_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 17:
			if(dlg->contact[DLG_CALLEE_LEG].s==NULL
					|| dlg->contact[DLG_CALLEE_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->contact[DLG_CALLEE_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->contact[DLG_CALLEE_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 18:
			if(dlg->bind_addr[DLG_CALLEE_LEG]==NULL)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->bind_addr[DLG_CALLEE_LEG]->sock_str.len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->bind_addr[DLG_CALLEE_LEG]->sock_str.s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 19:
			if(dlg->contact[DLG_CALLER_LEG].s==NULL
					|| dlg->contact[DLG_CALLER_LEG].len<=0)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->contact[DLG_CALLER_LEG].len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->contact[DLG_CALLER_LEG].s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 20:
			if(dlg->bind_addr[DLG_CALLER_LEG]==NULL)
				goto done;
			sv.s = pv_get_buffer();
			sv.len = dlg->bind_addr[DLG_CALLER_LEG]->sock_str.len;
			if(pv_get_buffer_size()<sv.len)
				goto done;
			res_type = 2;
			strncpy(sv.s, dlg->bind_addr[DLG_CALLER_LEG]->sock_str.s, sv.len);
			sv.s[sv.len] = '\0';
			break;
		case 21:
			res_type = 1;
			ui = (unsigned int)dlg->h_entry;
			break;
		default:
			res_type = 1;
			ui = (unsigned int)dlg->ref;
	}

done:
	dlg_release(dlg);

	switch(res_type) {
		case 1:
			return pv_get_uintval(msg, param, res, ui);
		case 2:
			return pv_get_strval(msg, param, res, &sv);
		default:
			return pv_get_null(msg, param, res);
	}
}

int pv_parse_dlg_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "ref", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "h_id", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "state", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "to_rs", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 6: 
			if(strncmp(in->s, "dflags", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else if(strncmp(in->s, "sflags", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "callid", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else if(strncmp(in->s, "to_uri", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else if(strncmp(in->s, "to_tag", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "toroute", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 9;
			else if(strncmp(in->s, "to_cseq", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 10;
			else if(strncmp(in->s, "from_rs", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 11;
			else if(strncmp(in->s, "h_entry", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 21;
			else goto error;
		break;
		case 8: 
			if(strncmp(in->s, "from_uri", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 12;
			else if(strncmp(in->s, "from_tag", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 13;
			else if(strncmp(in->s, "lifetime", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 14;
			else if(strncmp(in->s, "start_ts", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 15;
			else goto error;
		break;
		case 9: 
			if(strncmp(in->s, "from_cseq", 9)==0)
				sp->pvp.pvn.u.isname.name.n = 16;
			else goto error;
		break;
		case 10: 
			if(strncmp(in->s, "to_contact", 10)==0)
				sp->pvp.pvn.u.isname.name.n = 17;
			else goto error;
		break;
		case 11: 
			if(strncmp(in->s, "to_bindaddr", 11)==0)
				sp->pvp.pvn.u.isname.name.n = 18;
			else goto error;
		break;
		case 12: 
			if(strncmp(in->s, "from_contact", 12)==0)
				sp->pvp.pvn.u.isname.name.n = 19;
			else goto error;
		break;
		case 13: 
			if(strncmp(in->s, "from_bindaddr", 20)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV name %.*s\n", in->len, in->s);
	return -1;
}

void dlg_set_ctx_iuid(dlg_cell_t *dlg)
{
	_dlg_ctx.iuid.h_entry = dlg->h_entry;
	_dlg_ctx.iuid.h_id = dlg->h_id;
}

void dlg_reset_ctx_iuid(void)
{
	_dlg_ctx.iuid.h_entry = 0;
	_dlg_ctx.iuid.h_id = 0;
}

dlg_cell_t* dlg_get_ctx_dialog(void)
{
	return dlg_get_by_iuid(&_dlg_ctx.iuid);
}

dlg_ctx_t* dlg_get_dlg_ctx(void)
{
	return &_dlg_ctx;
}

int spiral_detect_reset(struct sip_msg *foo, unsigned int flags, void *bar)
{
	spiral_detected = -1;

	return 0;
}
