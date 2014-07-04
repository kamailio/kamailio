/*$Id$
 *
 * Example ser module, it will just print its string parameter to stdout
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * --------
 *  2003-03-10  module export interface updated to the new format (andrei)
 *  2003-03-11  flags export parameter added (janakj)
 *  2006-01-07  str export parameter added, overloading test (tma)
 */




#include "../../sr_module.h"
#include "../../route_struct.h"
#include "../../str.h"
#include <stdio.h>

MODULE_VERSION

static int print_fixup_f_1(void **param, int param_no);
static int print_fixup_f_2(void **param, int param_no);
static int print_f_0(struct sip_msg*, char*, char*);
static int print_f_1(struct sip_msg*, char*, char*);
static int print_f_2(struct sip_msg*, char*, char*);
static int print_f1(struct sip_msg*, char*, char*);
static int print_f2(struct sip_msg*, char*, char*);
static int print_f3(struct sip_msg*, char*, char*, char*);
static int print_f_var(struct sip_msg*, int argc, action_u_t argv[]);
static int mod_init(void);

/* the parameters are not used, they are only meant as an example*/
char* string_param = 0;
int int_param = 0;
str str_param = STR_STATIC_INIT("");

static cmd_export_t cmds[]={
	{"print", print_f_0, 0, 0, REQUEST_ROUTE},   // overload test
	{"print", print_f_1, 1, print_fixup_f_1, REQUEST_ROUTE},
	{"print", print_f_2, 2, print_fixup_f_2, REQUEST_ROUTE},
	{"print1", print_f1, 1, 0, REQUEST_ROUTE},
	{"print2", print_f2, 2, fixup_var_str_12, REQUEST_ROUTE},
	{"print3", (cmd_function)print_f3, 3, 0, REQUEST_ROUTE},
	{"printv", (cmd_function)print_f_var, VAR_PARAM_NO, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"string_param", PARAM_STRING, &string_param},
	{"str_param",    PARAM_STR, &str_param},
	{"int_param",    PARAM_INT, &int_param},
	{0,0,0}
};

struct module_exports exports = {
	"print_stdout",
	cmds,
	0,        /* RPC methods */
	params,

	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "print - initializing\n");
	DBG("print: string_param = '%s'\n", string_param);
	DBG("print: str_param = '%.*s'\n", str_param.len, str_param.s);
	DBG("print: int_param = %d\n", int_param);
	WARN("this is an example module, it has no practical use\n");
	return 0;
}


static int print_fixup_f(void **param, int param_no) {
	action_u_t *a;
	int n, i;
	n = fixup_get_param_count(param, param_no);
	for (i=1; i<=n; i++) {
		a = fixup_get_param(param, param_no, i);
		DBG("param #%d: '%s'\n", i, a->u.string);
	}
	return 1;
}

static int print_f_0(struct sip_msg* msg, char* s1, char* s2)
{
	printf("<null>\n");
	return 1;
}

static int print_f_1(struct sip_msg* msg, char* s1, char* s2)
{
	printf("%s\n",s1);
	return 1;
}

static int print_f_2(struct sip_msg* msg, char* s1, char* s2)
{
	printf("%s%s\n",s1, s2);
	return 1;
}

static int print_fixup_f_1(void **param, int param_no) {
	DBG("print: print_fixup_f_1('%s')\n", (char*)*param);
	return print_fixup_f(param, param_no);
}

static int print_fixup_f_2(void **param, int param_no) {
	DBG("print: print_fixup_f_2('%s')\n", (char*)*param);
	return print_fixup_f(param, param_no);
}



/* 1 parameter, no fixup version */
static int print_f1(struct sip_msg* msg, char* s1, char* not_used)
{
	printf("%s\n", s1);
	return 1;
}


/* 2 parameters, fparam fixup version */
static int print_f2(struct sip_msg* msg, char* s1, char* s2)
{
	str a, b;
	if (get_str_fparam(&a, msg, (fparam_t*)s1) !=0 ||
		 get_str_fparam(&b, msg, (fparam_t*)s2) !=0) {
		BUG("get_str_fparam failed\n");
		return -1;
	}
	printf("%.*s%.*s\n", a.len, a.s, b.len, b.s);
	return 1;
}


/* 3 parameters, no fixup version */
static int print_f3(struct sip_msg* msg, char* s1, char* s2, char* s3)
{
	printf("%s%s%s\n", s1, s2, s3);
	return 1;
}


/* variable number of parameters, no fixup version */
static int print_f_var(struct sip_msg* msg, int argc, action_u_t argv[])
{
	int i;
	for (i = 0; i < argc; i++)
		printf("%s", argv[i].u.string);
	printf("\n");
	return 1;
}
