/*
 * Copyright (C) 2007 Elena-Ramona Modroiu
 * Copyright (C) 2013 Olle E. Johansson
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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../shm_init.h"
#include "../../ut.h"
#include "../../pvar.h"

#include "pv_shv.h"

int shvar_locks_no=16;
gen_lock_set_t* shvar_locks=0;

static sh_var_t *sh_vars = 0;
static str shv_cpy = {0, 0};

/*
 * Initialize locks
 */
int shvar_init_locks(void)
{
	int i;

	/* already initialized */
	if(shvar_locks!=0)
		return 0;

	i = shvar_locks_no;
	do {
		if ((( shvar_locks=lock_set_alloc(i))!=0)&&
				(lock_set_init(shvar_locks)!=0))
		{
			shvar_locks_no = i;
			LM_INFO("locks array size %d\n", shvar_locks_no);
			return 0;

		}
		if (shvar_locks){
			lock_set_dealloc(shvar_locks);
			shvar_locks=0;
		}
		i--;
		if(i==0)
		{
			LM_ERR("failed to allocate locks\n");
			return -1;
		}
	} while (1);
}


void shvar_unlock_locks(void)
{
	unsigned int i;

	if (shvar_locks==0)
		return;

	for (i=0;i<shvar_locks_no;i++) {
#ifdef GEN_LOCK_T_PREFERED
		lock_release(&shvar_locks->locks[i]);
#else
		shvar_release_idx(i);
#endif
	};
}


void shvar_destroy_locks(void)
{
	if (shvar_locks !=0){
		lock_set_destroy(shvar_locks);
		lock_set_dealloc(shvar_locks);
	}
}

#ifndef GEN_LOCK_T_PREFERED
void shvar_lock_idx(int idx)
{
	lock_set_get(shvar_locks, idx);
}

void shvar_release_idx(int idx)
{
	lock_set_release(shvar_locks, idx);
}
#endif

/*
 * Get lock
 */
void lock_shvar(sh_var_t *shv)
{
	if(shv==NULL)
		return;
#ifdef GEN_LOCK_T_PREFERED
	lock_get(shv->lock);
#else
	ul_lock_idx(shv->lockidx);
#endif
}


/*
 * Release lock
 */
void unlock_shvar(sh_var_t *shv)
{
	if(shv==NULL)
		return;
#ifdef GEN_LOCK_T_PREFERED
	lock_release(shv->lock);
#else
	ul_release_idx(shv->lockidx);
#endif
}


sh_var_t* add_shvar(str *name)
{
	sh_var_t *sit;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	if(!shm_initialized())
	{
		LM_ERR("shm not intialized - cannot define shm now\n");
		return 0;
	}

	if(shvar_init_locks()!=0)
	{
		LM_ERR("cannot init shv locks\n");
		return 0;
	}

	for(sit=sh_vars; sit; sit=sit->next)
	{
		if(sit->name.len==name->len
				&& strncmp(name->s, sit->name.s, name->len)==0)
			return sit;
	}
	sit = (sh_var_t*)shm_malloc(sizeof(sh_var_t));
	if(sit==0)
	{
		LM_ERR("out of shm\n");
		return 0;
	}
	memset(sit, 0, sizeof(sh_var_t));
	sit->name.s = (char*)shm_malloc((name->len+1)*sizeof(char));

	if(sit->name.s==0)
	{
		LM_ERR("out of shm!\n");
		shm_free(sit);
		return 0;
	}
	sit->name.len = name->len;
	strncpy(sit->name.s, name->s, name->len);
	sit->name.s[sit->name.len] = '\0';

	if(sh_vars!=0)
		sit->n = sh_vars->n + 1;
	else
		sit->n = 1;

#ifdef GEN_LOCK_T_PREFERED
	sit->lock = &shvar_locks->locks[sit->n%shvar_locks_no];
#else
	sit->lockidx = sit->n%shvar_locks_no;
#endif

	sit->next = sh_vars;

	sh_vars = sit;

	return sit;
}

/* call it with lock set */
sh_var_t* set_shvar_value(sh_var_t* shv, int_str *value, int flags)
{
	if(shv==NULL)
		return NULL;
	if(value==NULL)
	{
		if(shv->v.flags&VAR_VAL_STR)
		{
			shm_free(shv->v.value.s.s);
			shv->v.flags &= ~VAR_VAL_STR;
		}
		memset(&shv->v.value, 0, sizeof(int_str));

		return shv;
	}

	if(flags&VAR_VAL_STR)
	{
		if(shv->v.flags&VAR_VAL_STR)
		{ /* old and new value is str */
			if(value->s.len>shv->v.value.s.len)
			{ /* not enough space to copy */
				shm_free(shv->v.value.s.s);
				memset(&shv->v.value, 0, sizeof(int_str));
				shv->v.value.s.s =
					(char*)shm_malloc((value->s.len+1)*sizeof(char));
				if(shv->v.value.s.s==0)
				{
					LM_ERR("out of shm\n");
					goto error;
				}
			}
		} else {
			memset(&shv->v.value, 0, sizeof(int_str));
			shv->v.value.s.s =
					(char*)shm_malloc((value->s.len+1)*sizeof(char));
			if(shv->v.value.s.s==0)
			{
				LM_ERR("out of shm!\n");
				goto error;
			}
			shv->v.flags |= VAR_VAL_STR;
		}
		strncpy(shv->v.value.s.s, value->s.s, value->s.len);
		shv->v.value.s.len = value->s.len;
		shv->v.value.s.s[value->s.len] = '\0';
	} else {
		if(shv->v.flags&VAR_VAL_STR)
		{
			shm_free(shv->v.value.s.s);
			shv->v.flags &= ~VAR_VAL_STR;
			memset(&shv->v.value, 0, sizeof(int_str));
		}
		shv->v.value.n = value->n;
	}

	return shv;
error:
	/* set the var to init value */
	memset(&shv->v.value, 0, sizeof(int_str));
	shv->v.flags &= ~VAR_VAL_STR;
	return NULL;
}

sh_var_t* get_shvar_by_name(str *name)
{
	sh_var_t *it;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	for(it=sh_vars; it; it=it->next)
	{
		if(it->name.len==name->len
				&& strncmp(name->s, it->name.s, name->len)==0)
			return it;
	}
	return 0;
}

void reset_shvars(void)
{
	sh_var_t *it;
	for(it=sh_vars; it; it=it->next)
	{
		if(it->v.flags&VAR_VAL_STR)
		{
			shm_free(it->v.value.s.s);
			it->v.flags &= ~VAR_VAL_STR;
		}
		memset(&it->v.value, 0, sizeof(int_str));
	}
}

void destroy_shvars(void)
{
	sh_var_t *it;
	sh_var_t *it0;

	it = sh_vars;
	while(it)
	{
		it0 = it;
		it = it->next;
		shm_free(it0->name.s);
		if(it0->v.flags&VAR_VAL_STR)
			shm_free(it0->v.value.s.s);
		shm_free(it0);
	}

	sh_vars = 0;
}


/********* PV functions *********/
int pv_parse_shvar_name(pv_spec_p sp, str *in)
{
	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
	
	sp->pvp.pvn.type = PV_NAME_PVAR;
	sp->pvp.pvn.u.dname = (void*)add_shvar(in);

	if(sp->pvp.pvn.u.dname==NULL)
	{
		LM_ERR("cannot register shvar [%.*s]\n", in->len, in->s);
		return -1;
	}

	return 0;
}

int pv_get_shvar(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	int len = 0;
	char *sval = NULL;
	sh_var_t *shv=NULL;
	
	if(msg==NULL || res==NULL)
		return -1;

	if(param==NULL || param->pvn.u.dname==0)
		return pv_get_null(msg, param, res);
	
	shv= (sh_var_t*)param->pvn.u.dname;

	lock_shvar(shv);
	if(shv->v.flags&VAR_VAL_STR)
	{
		if(shv_cpy.s==NULL || shv_cpy.len < shv->v.value.s.len)
		{
			if(shv_cpy.s!=NULL)
				pkg_free(shv_cpy.s);
			shv_cpy.s = (char*)pkg_malloc(shv->v.value.s.len*sizeof(char));
			if(shv_cpy.s==NULL)
			{
				unlock_shvar(shv);
				LM_ERR("no more pkg mem\n");
				return pv_get_null(msg, param, res);
			}
		}
		strncpy(shv_cpy.s, shv->v.value.s.s, shv->v.value.s.len);
		shv_cpy.len = shv->v.value.s.len;
		
		unlock_shvar(shv);
		
		res->rs = shv_cpy;
		res->flags = PV_VAL_STR;
	} else {
		res->ri = shv->v.value.n;
		
		unlock_shvar(shv);
		
		sval = sint2str(res->ri, &len);
		res->rs.s = sval;
		res->rs.len = len;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	}
	return 0;
}

int pv_set_shvar(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int_str isv;
	int flags;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(param->pvn.u.dname==0)
	{
		LM_ERR("error - cannot find shvar\n");
		goto error;
	}
	lock_shvar((sh_var_t*)param->pvn.u.dname);
	if(val == NULL)
	{
		isv.n = 0;
		set_shvar_value((sh_var_t*)param->pvn.u.dname, &isv, 0);
		goto done;
	}
	flags = 0;
	if(val->flags&PV_TYPE_INT)
	{
		isv.n = val->ri;
	} else {
		isv.s = val->rs;
		flags |= VAR_VAL_STR;
	}
	if(set_shvar_value((sh_var_t*)param->pvn.u.dname, &isv, flags)==NULL)
	{
		LM_ERR("error - cannot set shvar [%.*s] \n",
				((sh_var_t*)param->pvn.u.dname)->name.len,
				((sh_var_t*)param->pvn.u.dname)->name.s);
		goto error;
	}
done:
	unlock_shvar((sh_var_t*)param->pvn.u.dname);
	return 0;
error:
	unlock_shvar((sh_var_t*)param->pvn.u.dname);
	return -1;
}

struct mi_root* mi_shvar_set(struct mi_root* cmd_tree, void* param)
{
	str sp;
	str name;
	int ival;
	int_str isv;
	int flags;
	struct mi_node* node;
	sh_var_t *shv = NULL;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM_S));
	name = node->value;
	if(name.len<=0 || name.s==NULL)
	{
		LM_ERR("bad shv name\n");
		return init_mi_tree( 500, MI_SSTR("bad shv name"));
	}
	shv = get_shvar_by_name(&name);
	if(shv==NULL)
		return init_mi_tree(404, MI_SSTR("Not found"));

	node = node->next;
	if(node == NULL)
		return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM_S));
	sp = node->value;
	if(sp.s == NULL)
		return init_mi_tree(500, MI_SSTR("type not found"));
	flags = 0;
	if(sp.s[0]=='s' || sp.s[0]=='S')
		flags = VAR_VAL_STR;

	node= node->next;
	if(node == NULL)
		return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM_S));

	sp = node->value;
	if(sp.s == NULL)
	{
		return init_mi_tree(500, MI_SSTR("value not found"));
	}
	if(flags == 0)
	{
		if(str2sint(&sp, &ival))
		{
			LM_ERR("bad integer value\n");
			return init_mi_tree( 500, MI_SSTR("bad integer value"));
		}
		isv.n = ival;
	} else {
		isv.s = sp;
	}

	lock_shvar(shv);
	if(set_shvar_value(shv, &isv, flags)==NULL)
	{
		unlock_shvar(shv);
		LM_ERR("cannot set shv value\n");
		return init_mi_tree( 500, MI_SSTR("cannot set shv value"));
	}

	unlock_shvar(shv);
	LM_DBG("$shv(%.*s) updated\n", name.len, name.s);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

struct mi_root* mi_shvar_get(struct mi_root* cmd_tree, void* param)
{
	struct mi_root* rpl_tree = NULL;
	struct mi_node* node;
	struct mi_attr* attr = NULL;
	str name;
	int ival;
	sh_var_t *shv = NULL;

	node = cmd_tree->node.kids;
	if(node != NULL)
	{
		name = node->value;
		if(name.len<=0 || name.s==NULL)
		{
			LM_ERR("bad shv name\n");
			return init_mi_tree( 500, MI_SSTR("bad shv name"));
		}
		shv = get_shvar_by_name(&name);
		if(shv==NULL)
			return init_mi_tree(404, MI_SSTR("Not found"));
		
		rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==NULL)
			return NULL;

		node = add_mi_node_child(&rpl_tree->node, MI_DUP_VALUE,
				"VAR",3, name.s, name.len);
  		if(node == NULL)
  			goto error;
		lock_shvar(shv);
		if(shv->v.flags&VAR_VAL_STR)
		{
			attr = add_mi_attr (node, MI_DUP_VALUE, "type", 4, "string", 6);
			if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			attr = add_mi_attr (node, MI_DUP_VALUE, "value", 5,
					shv->v.value.s.s, shv->v.value.s.len);
	  		if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			unlock_shvar(shv);
		} else {
			ival = shv->v.value.n;
			unlock_shvar(shv);
			attr = add_mi_attr (node, MI_DUP_VALUE, "type",4, "integer", 7);
			if(attr == 0)
				goto error;
			name.s = sint2str(ival, &name.len);
			attr = add_mi_attr (node, MI_DUP_VALUE, "value",5,
					name.s, name.len);
	  		if(attr == 0)
				goto error;
		}

		goto done;
	}

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return NULL;

	for(shv=sh_vars; shv; shv=shv->next)
	{
		node = add_mi_node_child(&rpl_tree->node, MI_DUP_VALUE,
				"VAR", 3, shv->name.s, shv->name.len);
  		if(node == NULL)
  			goto error;

		lock_shvar(shv);
		if(shv->v.flags&VAR_VAL_STR)
		{
			attr = add_mi_attr (node, MI_DUP_VALUE, "type", 4, "string", 6);
			if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			attr = add_mi_attr (node, MI_DUP_VALUE, "value", 5,
					shv->v.value.s.s, shv->v.value.s.len);
	  		if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			unlock_shvar(shv);
		} else {
			ival = shv->v.value.n;
			unlock_shvar(shv);
			attr = add_mi_attr (node, MI_DUP_VALUE, "type",4, "integer", 7);
			if(attr == 0)
				goto error;
			name.s = sint2str(ival, &name.len);
			attr = add_mi_attr (node, MI_DUP_VALUE, "value",5,
					name.s, name.len);
	  		if(attr == 0)
				goto error;
		}
	}

done:
	return rpl_tree;
error:
	if(rpl_tree!=NULL)
		free_mi_tree(rpl_tree);
	return NULL;
}


void rpc_shv_get(rpc_t* rpc, void* c)
{
	str varname;
	int allvars = 0;
	sh_var_t *shv = NULL;
	void* th;
        void* ih;
        void* vh;

	if (rpc->scan(c, "S", &varname) != 1) {
		allvars = 1;
        }

	if (!allvars) {
		/* Get one variable value */
		shv = get_shvar_by_name(&varname);
		if(shv==NULL) {
			rpc->fault(c, 404, "Variable not found");
			return;
		}
		if (rpc->add(c, "{",  &ih) < 0)
        	{
               		rpc->fault(c, 500, "Internal error creating rpc");
                	return;
        	}
		
		lock_shvar(shv);
		if(shv->v.flags&VAR_VAL_STR)
		{
			if(rpc->struct_add(ih, "sss", "name", varname.s, "type", "string", "value", shv->v.value.s.s) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data (str)");
				unlock_shvar(shv);
				return;
			}
		} else {
			if(rpc->struct_add(ih, "ssd", "name", varname.s, "type", "int", "value", shv->v.value.n) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data (int)");
				unlock_shvar(shv);
				return;
			}
		}
		unlock_shvar(shv);

		return;
	}
	if (rpc->add(c, "{", &th) < 0)
       	{
         	rpc->fault(c, 500, "Internal error creating rpc");
               	return;
       	}

	if(rpc->struct_add(th, "{", "items", &ih) < 0)
               {
                         rpc->fault(c, 500, "Internal error creating rpc th");
                         return;
               }

	for(shv=sh_vars; shv; shv=shv->next)
	{
		lock_shvar(shv);
		if(rpc->struct_add(ih, "{", "shv", &vh) < 0)
               {
                         rpc->fault(c, 500, "Internal error creating rpc th");
                         return;
               }
		if(shv->v.flags&VAR_VAL_STR)
		{
			if(rpc->struct_add(vh, "sss", "name", shv->name.s, "type", "string", "value", shv->v.value.s.s) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data");
				unlock_shvar(shv);
				return;
			}
		} else {
			if(rpc->struct_add(vh, "ssd", "name", shv->name.s, "type", "int", "value", shv->v.value.n) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data");
				unlock_shvar(shv);
				return;
			}
		}
		unlock_shvar(shv);
	}

	return ;
}

void rpc_shv_set(rpc_t* rpc, void* c)
{
	str varname, type, value;
	int ival = 0;
	int_str isv;
	sh_var_t *shv = NULL;
	int flags = 0;
	LM_DBG("Entering SHV_set\n");

	if (rpc->scan(c, "S", &varname) != 1) {
		rpc->fault(c, 500, "Missing parameter varname (Parameters: varname type value)");
		return;
        }
	LM_DBG("SHV_set Varname %.*s \n", varname.len, varname.s);
	if (rpc->scan(c, "S", &type) != 1) {
		rpc->fault(c, 500, "Missing parameter type (Parameters: varname type value)");
		return;
        }
	if (strcasecmp(type.s, "int") == 0 ) {
		if (rpc->scan(c, "d", &ival) != 1) {
			rpc->fault(c, 500, "Missing integer parameter value (Parameters: varname type value)");
			return;
        	}
		isv.n = ival;
	} else  if (strcasecmp(type.s, "str") == 0 ) {
		/* String value */
		if (rpc->scan(c, "S", &value) != 1) {
			rpc->fault(c, 500, "Missing parameter value (Parameters: varname type value)");
			return;
        	}
		isv.s = value;
		flags = VAR_VAL_STR;
	} else {
		rpc->fault(c, 500, "Unknown parameter type (Types: int or str)");
		return;
	}

	shv = get_shvar_by_name(&varname);
	if(shv==NULL) {
		rpc->fault(c, 404, "Variable not found");
		return;
	}
		
	lock_shvar(shv);
	if(set_shvar_value(shv, &isv, flags)==NULL)
	{
		rpc->fault(c, 500, "Cannot set shared variable value");
		LM_ERR("cannot set shv value\n");
	} else {
		rpc->rpl_printf(c, "Ok. Variable set to new value.");
	}

	unlock_shvar(shv);
	return;
}

int param_set_xvar( modparam_t type, void* val, int mode)
{
	str s;
	char *p;
	int_str isv;
	int flags;
	int ival;
	script_var_t *pkv;
	sh_var_t *shv;

	if(!shm_initialized()!=0)
	{
		LM_ERR("shm not initialized - cannot set value for PVs\n");
		return -1;
	}

	s.s = (char*)val;
	if(s.s == NULL || s.s[0] == '\0')
		goto error;

	p = s.s;
	while(*p && *p!='=') p++;

	if(*p!='=')
		goto error;
	
	s.len = p - s.s;
	if(s.len == 0)
		goto error;
	p++;
	flags = 0;
	if(*p!='s' && *p!='S' && *p!='i' && *p!='I')
		goto error;

	if(*p=='s' || *p=='S')
		flags = VAR_VAL_STR;
	p++;
	if(*p!=':')
		goto error;
	p++;
	isv.s.s = p;
	isv.s.len = strlen(p);
	if(flags != VAR_VAL_STR) {
		if(str2sint(&isv.s, &ival)<0)
			goto error;
		isv.n = ival;
	}
	if(mode==0) {
		pkv = add_var(&s, VAR_TYPE_ZERO);
		if(pkv==NULL)
			goto error;
		if(set_var_value(pkv, &isv, flags)==NULL)
			goto error;
	} else {
		shv = add_shvar(&s);
		if(shv==NULL)
			goto error;
		if(set_shvar_value(shv, &isv, flags)==NULL)
			goto error;
	}
	
	return 0;
error:
	LM_ERR("unable to set shv parame [%s]\n", s.s);
	return -1;
}

int param_set_var( modparam_t type, void* val)
{
	return param_set_xvar(type, val, 0);
}

int param_set_shvar( modparam_t type, void* val)
{
	return param_set_xvar(type, val, 1);
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
