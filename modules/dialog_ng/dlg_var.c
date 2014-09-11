/**
 * $Id$
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#include "../../route.h"

#include "dlg_var.h"
#include "dlg_hash.h"
#include "dlg_profile.h"
#include "dlg_db_handler.h"

dlg_ctx_t _dlg_ctx;
extern int spiral_detected;

/*! global variable table, in case the dialog does not exist yet */
struct dlg_var * var_table = 0;
/*! ID of the current message */
int msg_id;

int dlg_cfg_cb(struct sip_msg *foo, unsigned int flags, void *bar)
{
        memset(&_dlg_ctx, 0, sizeof(dlg_ctx_t));
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
	var->next = NULL;
	var->vflags = DLG_FLAG_NEW;
	/* set key */
	var->key.len = key->len;
	var->key.s = (char*)shm_malloc(var->key.len);
	if (var->key.s==NULL) {
		shm_free(var);			
		LM_ERR("no more shm mem\n");
		return NULL;
	}
	memcpy(var->key.s, key->s, key->len);
	/* set value */
	var->value.len = val->len;
	var->value.s = (char*)shm_malloc(var->value.len);
	if (var->value.s==NULL) {
		shm_free(var->key.s);			
		shm_free(var);			
		LM_ERR("no more shm mem\n");
		return NULL;
	}
	memcpy(var->value.s, val->s, val->len);
	return var;
}

/*! Delete the current var-list */
void free_local_varlist() {
	struct dlg_var *var;
	while (var_table) {
		var = var_table;
		var_table = var_table->next;
		shm_free(var->key.s);
		shm_free(var->value.s);
		shm_free(var);
	}
}

/*! Retrieve the local var-list pointer */
struct dlg_var * get_local_varlist_pointer(struct sip_msg *msg, int clear_pointer) {
	struct dlg_var *var;
	/* New list, delete the old one */
	if (msg->id != msg_id) {
		free_local_varlist();
		msg_id = msg->id;
	}
	var = var_table;
	if (clear_pointer)
		var_table = NULL;
	return var;
}

/* Adds, updates and deletes dialog variables */
int set_dlg_variable_unsafe(struct dlg_cell *dlg, str *key, str *val, int new)
{
	struct dlg_var * var = NULL;
	struct dlg_var * it;
	struct dlg_var * it_prev;
	struct dlg_var ** var_list;
	
	if (dlg) 
		var_list = &dlg->vars;
	else 
		var_list = &var_table;

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
		LM_ERR("dialog variable <%.*s> does not exist in variable list\n", key->len, key->s);
		return -1;
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
		var_list = var_table;

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
	varlist = var_table;
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

str * api_get_dlg_variable(str *callid, str *ftag, str *ttag, str *key) {
    struct dlg_cell *dlg;
    
    unsigned int dir = DLG_DIR_NONE;
    dlg = get_dlg(callid, ftag, ttag, &dir); //increments ref count!
    
    if (!dlg) {
        LM_ERR("Asked to tear down non existent dialog\n");
        return NULL;
    }
    
    unref_dlg(dlg, 1);
    
    return get_dlg_variable(dlg, key);
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

int api_set_dlg_variable(str *callid, str *ftag, str *ttag, str *key, str *val) {
    struct dlg_cell *dlg;
    
    unsigned int dir = DLG_DIR_NONE;
    dlg = get_dlg(callid, ftag, ttag, &dir); //increments ref count!
    
    if (!dlg) {
        LM_ERR("Asked to tear down non existent dialog\n");
        return -1;
    }
    
    unref_dlg(dlg, 1);
     
    return set_dlg_variable(dlg, key, val);
    
   
}

int set_dlg_variable(struct dlg_cell *dlg, str *key, str *val)
{
    if( !dlg || !key || key->len > strlen(key->s) || (val && val->len > strlen(val->s)))
    {
        LM_ERR("BUG - bad parameters\n");
        return -1;
    }

    dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));

    if( !val)
    {
        if (set_dlg_variable_unsafe(dlg, key, NULL, 1)!=0) {
            LM_ERR("failed to delete dialog variable <%.*s>\n", key->len,key->s);
            goto error;
        }
    } else {
        if (set_dlg_variable_unsafe(dlg, key, val, 1)!=0) {
            LM_ERR("failed to store dialog values <%.*s>\n",key->len,key->s);
            goto error;
        }
    }

    dlg->dflags &= DLG_FLAG_CHANGED_VARS;
    dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));

    if ( dlg_db_mode==DB_MODE_REALTIME )
        update_dialog_dbinfo(dlg);

    print_lists(dlg);

    return 0;

error:
    dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
    return -1;
}

int pv_get_dlg_variable(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	struct dlg_cell *dlg;
	str * value;

	if (param==NULL || param->pvn.type!=PV_NAME_INTSTR || param->pvn.u.isname.type!=AVP_NAME_STR || param->pvn.u.isname.name.s.s==NULL) {
		LM_CRIT("BUG - bad parameters\n");
		return -1;
	}

	/* Retrieve the current dialog */
	dlg=get_current_dialog( msg);

	if (dlg) {
		/* Lock the dialog */
		dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));
	} else {
		/* Verify the local list */
		get_local_varlist_pointer(msg, 0);
	}

	value = get_dlg_variable_unsafe(dlg, &param->pvn.u.isname.name.s);

	print_lists(dlg);

	/* unlock dialog */
	if (dlg) dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));

	if (value)
		return pv_get_strval(msg, param, res, value);


	return 0;
}

int pv_set_dlg_variable(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val)
{
	struct dlg_cell *dlg;

	/* Retrieve the current dialog */
	dlg=get_current_dialog( msg);
	
	if (dlg) {
		/* Lock the dialog */
		dlg_lock(d_table, &(d_table->entries[dlg->h_entry]));
	} else {
		/* Verify the local list */
		get_local_varlist_pointer(msg, 0);
	}

	if (param==NULL || param->pvn.type!=PV_NAME_INTSTR || param->pvn.u.isname.type!=AVP_NAME_STR || param->pvn.u.isname.name.s.s==NULL ) {
		LM_CRIT("BUG - bad parameters\n");
		return -1;
	}

	if (val==NULL || val->flags&(PV_VAL_NONE|PV_VAL_NULL|PV_VAL_EMPTY)) {
		/* if NULL, remove the value */
		if (set_dlg_variable_unsafe(dlg, &param->pvn.u.isname.name.s, NULL, 1)!=0) {
			LM_ERR("failed to delete dialog variable <%.*s>\n", param->pvn.u.isname.name.s.len,param->pvn.u.isname.name.s.s);
			/* unlock dialog */
			if (dlg) dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
			return -1;
		}
	} else {
		/* if value, must be string */
		if ( !(val->flags&PV_VAL_STR)) {
			LM_ERR("non-string values are not supported\n");
			/* unlock dialog */
			if (dlg) dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
			return -1;
		}

		if (set_dlg_variable_unsafe(dlg, &param->pvn.u.isname.name.s, &val->rs, 1)!=0) {
			LM_ERR("failed to store dialog values <%.*s>\n",param->pvn.u.isname.name.s.len,param->pvn.u.isname.name.s.s);
			/* unlock dialog */
			if (dlg) dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
			return -1;
		}
	}
	/* unlock dialog */
	if (dlg) {
		dlg->dflags &= DLG_FLAG_CHANGED_VARS;		
		dlg_unlock(d_table, &(d_table->entries[dlg->h_entry]));
		if ( dlg_db_mode==DB_MODE_REALTIME )
			update_dialog_dbinfo(dlg);

	}
	print_lists(dlg);

	return 0;
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
			_dlg_ctx.set = (_dlg_ctx.dlg==NULL)?0:1;
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

	if(val==NULL)
		n = 0;
	else
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
			if(val->flags&PV_VAL_STR) {
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
	if(param==NULL)
		return -1;
	if(_dlg_ctx.dlg == NULL)
		return pv_get_null(msg, param, res);
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->h_id);
		case 2:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->state);
/*
		case 3:
			if(_dlg_ctx.dlg->route_set[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->route_set[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->route_set[DLG_CALLEE_LEG]);
		case 4:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->dflags);
		case 5:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->sflags);
		case 6:
			if(_dlg_ctx.dlg->callid.s==NULL
					|| _dlg_ctx.dlg->callid.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->callid);
		case 7:
			if(_dlg_ctx.dlg->to_uri.s==NULL
					|| _dlg_ctx.dlg->to_uri.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->to_uri);
		case 8:
			if(_dlg_ctx.dlg->tag[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->tag[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->tag[DLG_CALLEE_LEG]);
		case 9:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->toroute);
		case 10:
			if(_dlg_ctx.dlg->cseq[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->cseq[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->cseq[DLG_CALLEE_LEG]);
		case 11:
			if(_dlg_ctx.dlg->route_set[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->route_set[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->route_set[DLG_CALLER_LEG]);
		case 12:
			if(_dlg_ctx.dlg->from_uri.s==NULL
					|| _dlg_ctx.dlg->from_uri.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->from_uri);
		case 13:
			if(_dlg_ctx.dlg->tag[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->tag[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->tag[DLG_CALLER_LEG]);
		case 14:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->lifetime);
		case 15:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->start_ts);
		case 16:
			if(_dlg_ctx.dlg->cseq[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->cseq[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->cseq[DLG_CALLER_LEG]);
		case 17:
			if(_dlg_ctx.dlg->contact[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->contact[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->contact[DLG_CALLEE_LEG]);
		case 18:
			if(_dlg_ctx.dlg->bind_addr[DLG_CALLEE_LEG]==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->bind_addr[DLG_CALLEE_LEG]->sock_str);
		case 19:
			if(_dlg_ctx.dlg->contact[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->contact[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->contact[DLG_CALLER_LEG]);
		case 20:
			if(_dlg_ctx.dlg->bind_addr[DLG_CALLER_LEG]==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->bind_addr[DLG_CALLER_LEG]->sock_str);
		case 21:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->h_entry);
*/
		default:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->ref);
	}
	return 0;
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

void dlg_set_ctx_dialog(struct dlg_cell *dlg)
{
	_dlg_ctx.dlg = dlg;
}

struct dlg_cell* dlg_get_ctx_dialog(void)
{
	return _dlg_ctx.dlg;
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
