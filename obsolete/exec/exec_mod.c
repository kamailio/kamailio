/*
 * execution module
 *
 * $Id$
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
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 */


#include <stdio.h>

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"

#include "exec.h"
#include "kill.h"
#include "exec_hf.h"

MODULE_VERSION

unsigned int time_to_kill=0;

static int mod_init( void );

inline static int w_exec_dset(struct sip_msg* msg, char* cmd, char* foo);
inline static int w_exec_msg(struct sip_msg* msg, char* cmd, char* foo);

inline static void exec_shutdown();

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"exec_dset", w_exec_dset, 1, fixup_var_str_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{"exec_msg",  w_exec_msg,  1, fixup_var_str_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"time_to_kill", PARAM_INT, &time_to_kill},
	{"setvars",      PARAM_INT, &setvars     },
	{0, 0, 0}
};


#ifdef STATIC_EXEC
struct module_exports exec_exports = {
#else
struct module_exports exports= {
#endif
	"exec",
	cmds,           /* Exported functions */
	0,              /* RPC methods */
	params,         /* Exported parameters */
	mod_init, 	/* initialization module */
	0,		/* response function */
	exec_shutdown,	/* destroy function */
	0,		/* oncancel function */
	0		/* per-child init function */
};

void exec_shutdown()
{
	if (time_to_kill) destroy_kill();
}


static int mod_init( void )
{
	fprintf( stderr, "exec - initializing\n");
	if (time_to_kill) initialize_kill();
	return 0;
}

inline static int w_exec_dset(struct sip_msg* msg, char* p1, char* foo)
{
        str cmd;
	str *uri;
	environment_t *backup;
	int ret;

	if (get_str_fparam(&cmd, msg, (fparam_t*)p1) < 0) {
	    ERR("Error while obtaining command name\n");
	    return -1;
	}

	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LOG(L_ERR, "ERROR: w_exec_msg: no env created\n");
			return -1;
		}
	}

	if (msg->new_uri.s && msg->new_uri.len)
		uri=&msg->new_uri;
	else
		uri=&msg->first_line.u.request.uri;

	ret=exec_str(msg, &cmd, uri->s, uri->len);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}


inline static int w_exec_msg(struct sip_msg* msg, char* p1, char* foo)
{
        str cmd;
	environment_t *backup;
	int ret;

	if (get_str_fparam(&cmd, msg, (fparam_t*)p1) < 0) {
	    ERR("Error while obtaining command name\n");
	    return -1;
	}

	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LOG(L_ERR, "ERROR: w_exec_msg: no env created\n");
			return -1;
		}
	}
	ret=exec_msg(msg, &cmd);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}
