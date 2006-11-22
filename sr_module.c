/* $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-10  switched to new module_exports format: updated find_export,
 *               find_export_param, find_module (andrei)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-19  Support for flags in find_export (janakj)
 *  2003-03-29  cleaning pkg_mallocs introduced (jiri)
 *  2003-04-24  module version checking introduced (jiri)
 *  2004-09-19  compile flags are checked too (andrei)
 *  2005-01-07  removed find_module-overloading problems, added 
 *               find_export_record
 *  2006-02-07  added fix_flag (andrei)
 */


#include "sr_module.h"
#include "dprint.h"
#include "error.h"
#include "mem/mem.h"
#include "core_cmd.h"
#include "ut.h"
#include "re.h"
#include "route_struct.h"
#include "flags.h"
#include "trim.h"

#include <regex.h>
#include <dlfcn.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>


struct sr_module* modules=0;

#ifdef STATIC_EXEC
	extern struct module_exports* exec_exports();
#endif
#ifdef STATIC_TM
	extern struct module_exports* tm_exports();
#endif

#ifdef STATIC_MAXFWD
	extern struct module_exports* maxfwd_exports();
#endif

#ifdef STATIC_AUTH
        extern struct module_exports* auth_exports();
#endif

#ifdef STATIC_RR
        extern struct module_exports* rr_exports();
#endif

#ifdef STATIC_USRLOC
        extern struct module_exports* usrloc_exports();
#endif

#ifdef STATIC_SL
        extern struct module_exports* sl_exports();
#endif


/* initializes statically built (compiled in) modules*/
int register_builtin_modules()
{
	int ret;

	ret=0;
#ifdef STATIC_TM
	ret=register_module(tm_exports,"built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_EXEC
	ret=register_module(exec_exports,"built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_MAXFWD
	ret=register_module(maxfwd_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_AUTH
	ret=register_module(auth_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_RR
	ret=register_module(rr_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_USRLOC
	ret=register_module(usrloc_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_SL
	ret=register_module(sl_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

	return ret;
}



/* registers a module,  register_f= module register  functions
 * returns <0 on error, 0 on success */
int register_module(struct module_exports* e, char* path, void* handle)
{
	int ret;
	struct sr_module* mod;

	ret=-1;

	/* add module to the list */
	if ((mod=pkg_malloc(sizeof(struct sr_module)))==0){
		LOG(L_ERR, "load_module: memory allocation failure\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset(mod,0, sizeof(struct sr_module));
	mod->path=path;
	mod->handle=handle;
	mod->exports=e;
	mod->next=modules;
	modules=mod;
	return 0;
error:
	return ret;
}

#ifndef DLSYM_PREFIX
/* define it to null */
#define DLSYM_PREFIX
#endif

static inline int version_control(void *handle, char *path)
{
	char **m_ver;
	char **m_flags;
	char* error;

	m_ver=(char **)dlsym(handle, DLSYM_PREFIX "module_version");
	if ((error=(char *)dlerror())!=0) {
		LOG(L_ERR, "ERROR: no version info in module <%s>: %s\n",
			path, error );
		return 0;
	}
	m_flags=(char **)dlsym(handle, DLSYM_PREFIX "module_flags");
	if ((error=(char *)dlerror())!=0) {
		LOG(L_ERR, "ERROR: no compile flags info in module <%s>: %s\n",
			path, error );
		return 0;
	}
	if (!m_ver || !(*m_ver)) {
		LOG(L_ERR, "ERROR: no version in module <%s>\n", path );
		return 0;
	}
	if (!m_flags || !(*m_flags)) {
		LOG(L_ERR, "ERROR: no compile flags in module <%s>\n", path );
		return 0;
	}

	if (strcmp(SER_FULL_VERSION, *m_ver)==0){
		if (strcmp(SER_COMPILE_FLAGS, *m_flags)==0)
			return 1;
		else {
			LOG(L_ERR, "ERROR: module compile flags mismatch for %s "
						" \ncore: %s \nmodule: %s\n",
						path, SER_COMPILE_FLAGS, *m_flags);
			return 0;
		}
	}
	LOG(L_ERR, "ERROR: module version mismatch for %s; "
		"core: %s; module: %s\n", path, SER_FULL_VERSION, *m_ver );
	return 0;
}

/* returns 0 on success , <0 on error */
int load_module(char* path)
{
	void* handle;
	char* error;
	struct module_exports* exp;
	struct sr_module* t;

#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif
	handle=dlopen(path, RTLD_NOW); /* resolve all symbols now */
	if (handle==0){
		LOG(L_ERR, "ERROR: load_module: could not open module <%s>: %s\n",
					path, dlerror() );
		goto error;
	}

	for(t=modules;t; t=t->next){
		if (t->handle==handle){
			LOG(L_WARN, "WARNING: load_module: attempting to load the same"
						" module twice (%s)\n", path);
			goto skip;
		}
	}
	/* version control */
	if (!version_control(handle, path)) {
		exit(0);
	}
	/* launch register */
	exp = (struct module_exports*)dlsym(handle, DLSYM_PREFIX "exports");
	if ( (error =(char*)dlerror())!=0 ){
		LOG(L_ERR, "ERROR: load_module: %s\n", error);
		goto error1;
	}
	if (register_module(exp, path, handle)<0) goto error1;
	return 0;

error1:
	dlclose(handle);
error:
skip:
	return -1;
}




/* searches the module list and returns pointer to the "name" function record or
 * 0 if not found
 * flags parameter is OR value of all flags that must match
 */
cmd_export_t* find_export_record(char* name, int param_no, int flags)
{
	struct sr_module* t;
	cmd_export_t* cmd;

	for(t=modules;t;t=t->next){
		for(cmd=t->exports->cmds; cmd && cmd->name; cmd++){
			if((strcmp(name, cmd->name)==0)&&
			   (cmd->param_no==param_no) &&
			   ((cmd->flags & flags) == flags)
			  ){
				DBG("find_export_record: found <%s> in module %s [%s]\n",
				    name, t->exports->name, t->path);
				return cmd;
			}
		}
	}
	DBG("find_export_record: <%s> not found \n", name);
	return 0;
}


cmd_function find_export(char* name, int param_no, int flags)
{
	cmd_export_t* cmd;
	cmd = find_export_record(name, param_no, flags);
	return cmd?cmd->function:0;
}


rpc_export_t* find_rpc_export(char* name, int flags)
{
	struct sr_module* t;
	rpc_export_t* rpc;

	     /* Scan the list of core methods first, they are always
	      * present
	      */
	for(rpc = core_rpc_methods; rpc && rpc->name; rpc++) {
		if ((strcmp(name, rpc->name) == 0) &&
		    ((rpc->flags & flags) == flags)
		    ) {
			return rpc;
		}
	}
	     /* Continue with modules if not found */
	for(t = modules; t; t = t->next) {
		for(rpc = t->exports->rpc_methods; rpc && rpc->name; rpc++) {
			if ((strcmp(name, rpc->name) == 0) &&
			    ((rpc->flags & flags) == flags)
			   ) {
				return rpc;
			}
		}
	}
	return 0;
}


/*
 * searches the module list and returns pointer to "name" function in module "mod"
 * 0 if not found
 * flags parameter is OR value of all flags that must match
 */
cmd_function find_mod_export(char* mod, char* name, int param_no, int flags)
{
	struct sr_module* t;
	cmd_export_t* cmd;

	for (t = modules; t; t = t->next) {
		if (strcmp(t->exports->name, mod) == 0) {
			for (cmd = t->exports->cmds;  cmd && cmd->name; cmd++) {
				if ((strcmp(name, cmd->name) == 0) &&
				    (cmd->param_no == param_no) &&
				    ((cmd->flags & flags) == flags)
				   ){
					DBG("find_mod_export: found <%s> in module %s [%s]\n",
					    name, t->exports->name, t->path);
					return cmd->function;
				}
			}
		}
	}

	DBG("find_mod_export: <%s> in module <%s> not found\n", name, mod);
	return 0;
}


struct sr_module* find_module_by_name(char* mod) {
	struct sr_module* t;

	for(t = modules; t; t = t->next) {
		if (strcmp(mod, t->exports->name) == 0) {
			return t;
		}
	}
	DBG("find_module_by_name: module <%s> not found\n", mod);
	return 0;
}


void* find_param_export(struct sr_module* mod, char* name, modparam_t type_mask, modparam_t *param_type)
{
	param_export_t* param;

	if (!mod)
		return 0;
	for(param=mod->exports->params;param && param->name ; param++) {
		if ((strcmp(name, param->name) == 0) &&
			((param->type & PARAM_TYPE_MASK(type_mask)) != 0)) {
			DBG("find_param_export: found <%s> in module %s [%s]\n",
				name, mod->exports->name, mod->path);
			*param_type = param->type;
			return param->param_pointer;
		}
	}
	DBG("find_param_export: parameter <%s> not found in module <%s>\n",
			name, mod->exports->name);
	return 0;
}


void destroy_modules()
{
	struct sr_module* t, *foo;

	t=modules;
	while(t) {
		foo=t->next;
		if ((t->exports)&&(t->exports->destroy_f)) t->exports->destroy_f();
		pkg_free(t);
		t=foo;
	}
	modules=0;
}

#ifdef NO_REVERSE_INIT

/*
 * Initialize all loaded modules, the initialization
 * is done *AFTER* the configuration file is parsed
 */
int init_modules(void)
{
	struct sr_module* t;

	for(t = modules; t; t = t->next) {
		if ((t->exports) && (t->exports->init_f))
			if (t->exports->init_f() != 0) {
				LOG(L_ERR, "init_modules(): Error while initializing"
							" module %s\n", t->exports->name);
				return -1;
			}
	}
	return 0;
}

/*
 * per-child initialization
 */
int init_child(int rank)
{
	struct sr_module* t;
	char* type;

	switch(rank) {
	case PROC_MAIN:     type = "PROC_MAIN";     break;
	case PROC_TIMER:    type = "PROC_TIMER";    break;
	case PROC_FIFO:     type = "PROC_FIFO";     break;
	case PROC_TCP_MAIN: type = "PROC_TCP_MAIN"; break;
	default:            type = "CHILD";         break;
	}
	DBG("init_child: initializing %s with rank %d\n", type, rank);


	for(t = modules; t; t = t->next) {
		if (t->exports->init_child_f) {
			if ((t->exports->init_child_f(rank)) < 0) {
				LOG(L_ERR, "init_child(): Initialization of child %d failed\n",
						rank);
				return -1;
			}
		}
	}
	return 0;
}

#else


/* recursive module child initialization; (recursion is used to
   process the module linear list in the same order in
   which modules are loaded in config file
*/

static int init_mod_child( struct sr_module* m, int rank )
{
	if (m) {
		/* iterate through the list; if error occurs,
		   propagate it up the stack
		 */
		if (init_mod_child(m->next, rank)!=0) return -1;
		if (m->exports && m->exports->init_child_f) {
			DBG("DEBUG: init_mod_child (%d): %s\n",
					rank, m->exports->name);
			if (m->exports->init_child_f(rank)<0) {
				LOG(L_ERR, "init_mod_child(): Error while initializing"
							" module %s\n", m->exports->name);
				return -1;
			} else {
				/* module correctly initialized */
				return 0;
			}
		}
		/* no init function -- proceed with success */
		return 0;
	} else {
		/* end of list */
		return 0;
	}
}


/*
 * per-child initialization
 */
int init_child(int rank)
{
	return init_mod_child(modules, rank);
}



/* recursive module initialization; (recursion is used to
   process the module linear list in the same order in
   which modules are loaded in config file
*/

static int init_mod( struct sr_module* m )
{
	if (m) {
		/* iterate through the list; if error occurs,
		   propagate it up the stack
		 */
		if (init_mod(m->next)!=0) return -1;
		if (m->exports && m->exports->init_f) {
			DBG("DEBUG: init_mod: %s\n", m->exports->name);
			if (m->exports->init_f()!=0) {
				LOG(L_ERR, "init_mod(): Error while initializing"
							" module %s\n", m->exports->name);
				return -1;
			} else {
				/* module correctly initialized */
				return 0;
			}
		}
		/* no init function -- proceed with success */
		return 0;
	} else {
		/* end of list */
		return 0;
	}
}

/*
 * Initialize all loaded modules, the initialization
 * is done *AFTER* the configuration file is parsed
 */
int init_modules(void)
{
	return init_mod(modules);
}

#endif


action_u_t *fixup_get_param(void **cur_param, int cur_param_no, int required_param_no) {
	action_u_t *a, a2;
        /* cur_param points to a->u.string, get pointer to a */
	a = (void*) ((char *)cur_param - ((char *)&a2.u.string-(char *)&a2));
	return a + required_param_no - cur_param_no;
}

int fixup_get_param_count(void **cur_param, int cur_param_no) {
	action_u_t *a;
	a = fixup_get_param(cur_param, cur_param_no, 0);
	if (a)
		return a->u.number;
	else
		return -1;
}


/* fixes flag params (resolves possible named flags)
 * use PARAM_USE_FUNC|PARAM_STRING as a param. type and create
 * a wrapper function that does just:
 * return fix_flag(type, val, "my_module", "my_param", &flag_var)
 * see also param_func_t.
 */
int fix_flag( modparam_t type, void* val,
					char* mod_name, char* param_name, int* flag)
{
	int num;
	int err;
	int f, len;
	char* s;
	char *p;
	
	if ((type & PARAM_STRING)==0){
		LOG(L_CRIT, "BUG: %s: fix_flag(%s): bad parameter type\n",
					mod_name, param_name);
		return -1;
	}
	s=(char*)val;
	len=strlen(s);
	f=-1;
	/* try to see if it's a number */
	num = str2s(s, len, &err);
	if (err != 0) {
		/* see if it's in the name:<no> format */
		p=strchr(s, ':');
		if (p){
			f= str2s(p+1, strlen(p+1), &err);
			if (err!=0){
				LOG(L_ERR, "ERROR: %s: invalid %s format:"
						" \"%s\"", mod_name, param_name, s);
				return -1;
			}
			*p=0;
		}
		if ((num=get_flag_no(s, len))<0){
			/* not declared yet, declare it */
			num=register_flag(s, f);
		}
		if (num<0){
			LOG(L_ERR, "ERROR: %s: bad %s %s\n", mod_name, param_name, s);
			return -1;
		} else if ((f>0) && (num!=f)){
			LOG(L_ERR, "WARNING: %s: flag %s already defined"
					" as %d (and not %d), using %s:%d\n",
					mod_name, s, num, f, s, num);
		}
	}
	*flag=num;
	return 0;
}

/*
 * Common function parameter fixups
 */

/*
 * Generic parameter fixup function which creates
 * fparam_t structure. type parameter contains allowed
 * parameter types
 */
int fix_param(int type, void** param)
{	
    fparam_t* p;
    str name, s;
    unsigned long num;
    int err;
    
    p = (fparam_t*)pkg_malloc(sizeof(fparam_t));
    if (!p) {
	ERR("No memory left\n");
	return E_OUT_OF_MEM;
    }
    memset(p, 0, sizeof(fparam_t));
    p->orig = *param;
    
    switch(type) {
    case FPARAM_UNSPEC:
	ERR("Invalid type value\n");
	goto error;
	
    case FPARAM_STRING:
	p->v.asciiz = *param;
	break;
	
    case FPARAM_STR:
	p->v.str.s = (char*)*param;
	p->v.str.len = strlen(p->v.str.s);
	break;
	
    case FPARAM_INT:
	num = str2s(*param, strlen(*param), &err);
	if (err == 0) {
	    p->v.i = num;
	} else {
		 /* Not a number */
	    pkg_free(p);
	    return 1;
	}
	break;
	
    case FPARAM_REGEX:
	if ((p->v.regex = pkg_malloc(sizeof(regex_t))) == 0) {
	    ERR("No memory left\n");
	    goto error;
	}
	if (regcomp(p->v.regex, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE)) {
	    pkg_free(p->v.regex);
	    ERR("Bad regular expression '%s'\n", (char*)*param);
	    goto error;
	}
	break;
	
    case FPARAM_AVP:
	name.s = (char*)*param;
	name.len = strlen(name.s);
	trim(&name);
	if (!name.len || name.s[0] != '$') {
		 /* Not an AVP identifier */
	    pkg_free(p);
	    return 1;
	}
	name.s++;
	name.len--;
	
	if (parse_avp_ident(&name, &p->v.avp) < 0) {
	    ERR("Error while parsing attribute name\n");
	    goto error;
	}
	break;
	
    case FPARAM_SELECT:
	name.s = (char*)*param;
	name.len = strlen(name.s);
	trim(&name);
	if (!name.len || name.s[0] != '@') {
		 /* Not a select identifier */
	    pkg_free(p);
	    return 1;
	}
	
	if (parse_select(&name.s, &p->v.select) < 0) {
	    ERR("Error while parsing select identifier\n");
	    goto error;
	}
	break;

    case FPARAM_SUBST:
	s.s = *param;
	s.len = strlen(s.s);
	p->v.subst = subst_parser(&s);
	if (!p->v.subst) {
	    ERR("Error while parsing regex substitution\n");
	    return -1;
	}
	break;
    }
    
    p->type = type;
    *param = (void*)p;
    return 0;
    
 error:
    pkg_free(p);
    return E_UNSPEC;
}


/*
 * Fixup variable string, the parameter can be
 * AVP, SELECT, or ordinary string. AVP and select
 * identifiers will be resolved to their values during
 * runtime
 *
 * The parameter value will be converted to fparam structure
 * This function returns -1 on an error
 */
int fixup_var_str_12(void** param, int param_no)
{
    int ret;
    if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
    if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
    if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
    ERR("Error while fixing parameter, AVP, SELECT, and str conversions failed\n");
    return -1;
}

/* Same as fixup_var_str_12 but applies to the 1st parameter only */
int fixup_var_str_1(void** param, int param_no)
{
    if (param_no == 1) return fixup_var_str_12(param, param_no);
    else return 0;
}

/* Same as fixup_var_str_12 but applies to the 2nd parameter only */
int fixup_var_str_2(void** param, int param_no)
{
    if (param_no == 2) return fixup_var_str_12(param, param_no);
    else return 0;
}


/*
 * Fixup variable integer, the parameter can be
 * AVP, SELECT, or ordinary integer. AVP and select
 * identifiers will be resolved to their values and 
 * converted to int if necessary during runtime
 *
 * The parameter value will be converted to fparam structure
 * This function returns -1 on an error
 */
int fixup_var_int_12(void** param, int param_no)
{
    int ret;
    if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
    if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
    if ((ret = fix_param(FPARAM_INT, param)) <= 0) return ret;
    ERR("Error while fixing parameter, AVP, SELECT, and int conversions failed\n");
    return -1;
}

/* Same as fixup_var_int_12 but applies to the 1st parameter only */
int fixup_var_int_1(void** param, int param_no)
{
    if (param_no == 1) return fixup_var_int_12(param, param_no);
    else return 0;
}

/* Same as fixup_var_int_12 but applies to the 2nd parameter only */
int fixup_var_int_2(void** param, int param_no)
{
    if (param_no == 2) return fixup_var_int_12(param, param_no);
    else return 0;
}


/*
 * The parameter must be a regular expression which must compile, the
 * parameter will be converted to compiled regex
 */
int fixup_regex_12(void** param, int param_no)
{
    int ret;

    if ((ret = fix_param(FPARAM_REGEX, param)) <= 0) return ret;
    ERR("Error while compiling regex in function parameter\n");
    return -1;
}

/* Same as fixup_regex_12 but applies to the 1st parameter only */
int fixup_regex_1(void** param, int param_no)
{
    if (param_no == 1) return fixup_regex_12(param, param_no);
    else return 0;
}

/* Same as fixup_regex_12 but applies to the 2nd parameter only */
int fixup_regex_2(void** param, int param_no)
{
    if (param_no == 2) return fixup_regex_12(param, param_no);
    else return 0;
}

/*
 * The string parameter will be converted to integer
 */
int fixup_int_12(void** param, int param_no)
{
    int ret;

    if ((ret = fix_param(FPARAM_INT, param)) <= 0) return ret;
    ERR("Cannot function parameter to integer\n");
    return -1;

}

/* Same as fixup_int_12 but applies to the 1st parameter only */
int fixup_int_1(void** param, int param_no)
{
    if (param_no == 1) return fixup_int_12(param, param_no);
    else return 0;
}

/* Same as fixup_int_12 but applies to the 2nd parameter only */
int fixup_int_2(void** param, int param_no)
{
    if (param_no == 2) return fixup_int_12(param, param_no);
    else return 0;
}

/*
 * Parse the parameter as static string, do not resolve
 * AVPs or selects, convert the parameter to str structure
 */
int fixup_str_12(void** param, int param_no)
{
    int ret;

    if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
    ERR("Cannot function parameter to string\n");
    return -1;
}

/* Same as fixup_str_12 but applies to the 1st parameter only */
int fixup_str_1(void** param, int param_no)
{
    if (param_no == 1) return fixup_str_12(param, param_no);
    else return 0;
}

/* Same as fixup_str_12 but applies to the 2nd parameter only */
int fixup_str_2(void** param, int param_no)
{
    if (param_no == 2) return fixup_str_12(param, param_no);
    else return 0;
}


/*
 * Get the function parameter value as string
 * Return values:  0 - Success
 *                -1 - Cannot get value
 */
int get_str_fparam(str* dst, struct sip_msg* msg, fparam_t* param)
{
    int_str val;
    int ret;
    avp_t* avp;

    switch(param->type) {
    case FPARAM_REGEX:
    case FPARAM_UNSPEC:
    case FPARAM_INT:
	return -1;

    case FPARAM_STRING:
	dst->s = param->v.asciiz;
	dst->len = strlen(param->v.asciiz);
	break;

    case FPARAM_STR:
	*dst = param->v.str;
	break;

    case FPARAM_AVP:
	avp = search_first_avp(param->v.avp.flags, param->v.avp.name, &val, 0);
	if (!avp) {
	    DBG("Could not find AVP from function parameter '%s'\n", param->orig);
	    return -1;
	}
	if (avp->flags & AVP_VAL_STR) {
	    *dst = val.s;
	} else {
		 /* The caller does not know of what type the AVP will be so
		  * convert int AVPs into string here
		  */
	    dst->s = int2str(val.n, &dst->len);
	}
	break;

    case FPARAM_SELECT:
	ret = run_select(dst, param->v.select, msg);
	if (ret < 0 || ret > 0) return -1;
	break;
    }

    return 0;
}


/*
 * Get the function parameter value as integer
 * Return values:  0 - Success
 *                -1 - Cannot get value
 */
int get_int_fparam(int* dst, struct sip_msg* msg, fparam_t* param)
{
    int_str val;
    int ret;
    avp_t* avp;
    str tmp;

    switch(param->type) {
    case FPARAM_INT:
	*dst = param->v.i;
	return 0;
	
    case FPARAM_REGEX:
    case FPARAM_UNSPEC:
    case FPARAM_STRING:
    case FPARAM_STR:
	return -1;
	
    case FPARAM_AVP:
	avp = search_first_avp(param->v.avp.flags, param->v.avp.name, &val, 0);
	if (!avp) {
	    DBG("Could not find AVP from function parameter '%s'\n", param->orig);
	    return -1;
	}
	if (avp->flags & AVP_VAL_STR) {
	    if (str2int(&val.s, (unsigned int*)dst) < 0) {
		ERR("Could not convert AVP string value to int\n");
		return -1;
	    }
	} else {
	    *dst = val.n;
	}
	break;

    case FPARAM_SELECT:
	ret = run_select(&tmp, param->v.select, msg);
	if (ret < 0 || ret > 0) return -1;
	if (str2int(&tmp, (unsigned int*)dst) < 0) {
	    ERR("Could not convert select result to int\n");
	    return -1;
	}
	break;
    }

    return 0;
}
