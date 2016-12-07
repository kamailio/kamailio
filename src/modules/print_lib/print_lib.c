/*$Id$
 *
 * Example library using ser module (it doesn't do anything usefull)
 *
 *
 * Copyright (C) 2007 iptelorg GmbH
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
 /* History:
 * --------
 *  2007-03-16  created by andrei
 */




#include "../../sr_module.h"
#include "../../lib/print/print.h"

MODULE_VERSION

static int print_f(struct sip_msg*, char*,char*);
static int mod_init(void);


static cmd_export_t cmds[]={
	{"print_stderr", print_f, 1, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{0,0,0}
};

struct module_exports exports = {
	"print_stderr",
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
	stderr_println("print_lib - initializing");
	return 0;
}


static int print_f(struct sip_msg* msg, char* s1, char* s2)
{
	stderr_println(s1);
	return 1;
}

