/* $Id$
 *
 * modules/plug-in structures declarations
 *
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
 *  2003-03-10  changed module exports interface: added struct cmd_export
 *               and param_export (andrei)
 *  2003-03-16  Added flags field to cmd_export_ (janakj)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2004-03-12  extra flag USE_FUNC_PARAM added to modparam type -
 *              instead of copying the param value, a func is called (bogdan)
 *  2004-09-19  switched to version.h for the module versions checks (andrei)
 *  2004-12-03  changed param_func_t to (modparam_t, void*), killed
 *               param_func_param_t   (andrei)
 */


#ifndef sr_module_h
#define sr_module_h

#include "parser/msg_parser.h" /* for sip_msg */
#include "version.h"
#include "rpc.h"
#include "route_struct.h"
#include "str.h"

typedef  struct module_exports* (*module_register)();
typedef  int (*cmd_function)(struct sip_msg*, char*, char*);
typedef  int (*fixup_function)(void** param, int param_no);
typedef  int (*response_function)(struct sip_msg*);
typedef  void (*onbreak_function)(struct sip_msg*);
typedef void (*destroy_function)();
typedef int (*init_function)(void);
typedef int (*child_init_function)(int rank);


#define PARAM_STRING     (1U<<0)  /* String (char *) parameter type */
#define PARAM_INT        (1U<<1)  /* Integer parameter type */
#define PARAM_STR        (1U<<2)  /* struct str parameter type */
#define PARAM_USE_FUNC   (1U<<(8*sizeof(int)-1))
#define PARAM_TYPE_MASK(_x)   ((_x)&(~PARAM_USE_FUNC))

/* temporary, for backward compatibility only until all modules adjust it */
#define STR_PARAM PARAM_STRING
#define INT_PARAM PARAM_INT
#define USE_FUNC_PARAM PARAM_USE_FUNC

typedef unsigned int modparam_t;

typedef int (*param_func_t)( modparam_t type, void* val);

#define REQUEST_ROUTE 1  /* Function can be used in request route blocks */
#define FAILURE_ROUTE 2  /* Function can be used in reply route blocks */
#define ONREPLY_ROUTE 4  /* Function can be used in on_reply */
#define BRANCH_ROUTE  8  /* Function can be used in branch_route blocks */
#define ONSEND_ROUTE   16  /* Function can be used in onsend_route blocks */

/* Macros - used as rank in child_init function */
#define PROC_MAIN      0  /* Main ser process */
#define PROC_TIMER    -1  /* Timer attendant process */
#define PROC_RPC      -2  /* RPC type process */
#define PROC_FIFO      PROC_RPC  /* FIFO attendant process */
#define PROC_TCP_MAIN -4  /* TCP main process */
#define PROC_UNIXSOCK -5  /* Unix socket server */
#define PROC_NOCHLDINIT -128 /* no child init functions will be called
                                if this rank is used in fork_process() */

#define PROC_MIN PROC_UNIXSOCK /* Minimum process rank */

#define MODULE_VERSION \
	char *module_version=SER_FULL_VERSION; \
	char *module_flags=SER_COMPILE_FLAGS;

struct cmd_export_ {
	char* name;             /* null terminated command name */
	cmd_function function;  /* pointer to the corresponding function */
	int param_no;           /* number of parameters used by the function */
	fixup_function fixup;   /* pointer to the function called to "fix" the
							   parameters */
	int flags;              /* Function flags */
};


struct param_export_ {
	char* name;             /* null terminated param. name */
	modparam_t type;        /* param. type */
	void* param_pointer;    /* pointer to the param. memory location */
};


enum {
	FPARAM_UNSPEC = 0,
	FPARAM_STRING = (1 << 0),
	FPARAM_STR    = (1 << 1),
	FPARAM_INT    = (1 << 2),
	FPARAM_REGEX  = (1 << 3),
	FPARAM_AVP    = (1 << 5),
	FPARAM_SELECT = (1 << 6),
	FPARAM_SUBST  = (1 << 7)
};

/*
 * Function parameter
 */
typedef struct fparam {
        char* orig;                       /* The original value */
        int type;                         /* Type of parameter */
        union {
		char* asciiz;             /* Zero terminated ASCII string */
		struct _str str;          /* pointer/len string */
		int i;                    /* Integer value */
		regex_t* regex;           /* Compiled regular expression */
		avp_ident_t avp;          /* AVP identifier */
	        select_t* select;         /* select structure */ 
	        struct subst_expr* subst; /* Regex substitution */
	} v;
} fparam_t;


typedef struct cmd_export_ cmd_export_t;
typedef struct param_export_ param_export_t;

struct module_exports {
	char* name;                     /* null terminated module name */

	cmd_export_t* cmds;             /* null terminated array of the exported
									   commands */
	rpc_export_t* rpc_methods;      /* null terminated array of exported rpc methods */
	param_export_t* params;         /* null terminated array of the exported
									   module parameters */

	init_function init_f;           /* Initialization function */
	response_function response_f;   /* function used for responses,
									   returns yes or no; can be null */
	destroy_function destroy_f;     /* function called when the module should
									   be "destroyed", e.g: on ser exit;
									   can be null */
	onbreak_function onbreak_f;
	child_init_function init_child_f;  /* function called by all processes
										  after the fork */
};


struct sr_module{
	char* path;
	void* handle;
	struct module_exports* exports;
	struct sr_module* next;
};


struct sr_module* modules; /* global module list*/

int register_builtin_modules();
int register_module(struct module_exports*, char*,  void*);
int load_module(char* path);
cmd_export_t* find_export_record(char* name, int param_no, int flags);
cmd_function find_export(char* name, int param_no, int flags);
cmd_function find_mod_export(char* mod, char* name, int param_no, int flags);
rpc_export_t* find_rpc_export(char* name, int flags);
void destroy_modules();
int init_child(int rank);
int init_modules(void);
struct sr_module* find_module_by_name(char* mod);

/*
 * Find a parameter with given type and return it's
 * address in memory
 * If there is no such parameter, NULL is returned
 */
void* find_param_export(struct sr_module* mod, char* name, modparam_t type_mask, modparam_t *param_type);

/* modules function prototypes:
 * struct module_exports* mod_register(); (type module_register)
 * int   foo_cmd(struct sip_msg* msg, char* param);
 *  - returns >0 if ok , <0 on error, 0 to stop processing (==DROP)
 * int   response_f(struct sip_msg* msg)
 *  - returns >0 if ok, 0 to drop message
 */


/* API function to get other parameters from fixup */
action_u_t *fixup_get_param(void **cur_param, int cur_param_no, int required_param_no);
int fixup_get_param_count(void **cur_param, int cur_param_no);

int fix_flag( modparam_t type, void* val,
					char* mod_name, char* param_name, int* flag);


/*
 * Common function parameter fixups
 */

/*
 * Generic parameter fixup function which creates
 * fparam_t structure. type parameter contains allowed
 * parameter types
 */
int fix_param(int type, void** param);

/*
 * Fixup variable string, the parameter can be
 * AVP, SELECT, or ordinary string. AVP and select
 * identifiers will be resolved to their values during
 * runtime
 *
 * The parameter value will be converted to fparam structure
 * This function returns -1 on an error
 */
int fixup_var_str_12(void** param, int param_no);

/* Same as fixup_var_str_12 but applies to the 1st parameter only */
int fixup_var_str_1(void** param, int param_no);

/* Same as fixup_var_str_12 but applies to the 2nd parameter only */
int fixup_var_str_2(void** param, int param_no);

/*
 * Fixup variable integer, the parameter can be
 * AVP, SELECT, or ordinary integer. AVP and select
 * identifiers will be resolved to their values and 
 * converted to int if necessary during runtime
 *
 * The parameter value will be converted to fparam structure
 * This function returns -1 on an error
 */
int fixup_var_int_12(void** param, int param_no);

/* Same as fixup_var_int_12 but applies to the 1st parameter only */
int fixup_var_int_1(void** param, int param_no);

/* Same as fixup_var_int_12 but applies to the 2nd parameter only */
int fixup_var_int_2(void** param, int param_no);

/*
 * The parameter must be a regular expression which must compile, the
 * parameter will be converted to compiled regex
 */
int fixup_regex_12(void** param, int param_no);

/* Same as fixup_regex_12 but applies to the 1st parameter only */
int fixup_regex_1(void** param, int param_no);

/* Same as fixup_regex_12 but applies to the 2nd parameter only */
int fixup_regex_2(void** param, int param_no);

/*
 * The string parameter will be converted to integer
 */
int fixup_int_12(void** param, int param_no);

/* Same as fixup_int_12 but applies to the 1st parameter only */
int fixup_int_1(void** param, int param_no);

/* Same as fixup_int_12 but applies to the 2nd parameter only */
int fixup_int_2(void** param, int param_no);

/*
 * Parse the parameter as static string, do not resolve
 * AVPs or selects, convert the parameter to str structure
 */
int fixup_str_12(void** param, int param_no);

/* Same as fixup_str_12 but applies to the 1st parameter only */
int fixup_str_1(void** param, int param_no);

/* Same as fixup_str_12 but applies to the 2nd parameter only */
int fixup_str_2(void** param, int param_no);

/*
 * Get the function parameter value as string
 * Return values:  0 - Success
 *                -1 - Cannot get value
 */
int get_str_fparam(str* dst, struct sip_msg* msg, fparam_t* param);

/*
 * Get the function parameter value as integer
 * Return values:  0 - Success
 *                -1 - Cannot get value
 */
int get_int_fparam(int* dst, struct sip_msg* msg, fparam_t* param);

#endif /* sr_module_h */
