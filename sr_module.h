/* $Id$
 *
 * modules/plugin strtuctures declarations
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */


#ifndef sr_module_h
#define sr_module_h

#include "parser/msg_parser.h" /* for sip_msg */

typedef  struct module_exports* (*module_register)();
typedef  int (*cmd_function)(struct sip_msg*, char*, char*);
typedef  int (*fixup_function)(void** param, int param_no);
typedef  int (*response_function)(struct sip_msg*);
typedef  void (*onbreak_function)(struct sip_msg*);
typedef void (*destroy_function)();
typedef int (*init_function)(void);
typedef int (*child_init_function)(int rank);


typedef enum {
	STR_PARAM,  /* String parameter type */
	INT_PARAM,  /* Integer parameter type */
} modparam_t;       /* Allowed types of parameters */


struct cmd_export_ {
	char* name;             /* null terminated command name */
	cmd_function function;  /* pointer to the corresponding function */
	int param_no;           /* number of parameters used by the function */
	fixup_function fixup;   /* pointer to the function called to "fix" the
							   parameters */
};


struct param_export_ {
	char* name;             /* null terminated param. name */
	modparam_t type;        /* param. type */
	void* param_pointer;    /* pointer to the param. memory location */
};


typedef struct cmd_export_ cmd_export_t;
typedef struct param_export_ param_export_t;

struct module_exports{
	char* name;                     /* null terminated module name */
	
	cmd_export_t* cmds;             /* null terminated array of the exported
									   commands */
	param_export_t* params;         /* null terminated array of the exported
									   module parameters */

	init_function init_f;           /* Initilization function */
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
cmd_function find_export(char* name, int param_no);
struct sr_module* find_module(void *f, cmd_export_t** cmd);
void destroy_modules();
int init_child(int rank);
int init_modules(void);

/*
 * Find a parameter with given type and return it's
 * address in memory
 * If there is no such parameter, NULL is returned
 */
void* find_param_export(char* mod, char* name, modparam_t type);

/* modules function prototypes:
 * struct module_exports* mod_register(); (type module_register)
 * int   foo_cmd(struct sip_msg* msg, char* param);
 *  - returns >0 if ok , <0 on error, 0 to stop processing (==DROP)
 * int   response_f(struct sip_msg* msg)
 *  - returns >0 if ok, 0 to drop message
 */


#endif
