/*
 * $Id$
 *
 * Perl module for Kamailio
 *
 * Copyright (C) 2006 Collax GmbH 
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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

#include <string.h>
#include <stdio.h>

#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/parse_param.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../parser/parse_uri.h"

#include "perlfunc.h"
#include "app_perl_mod.h"


/*
 * Check for existence of a function.
 */
int perl_checkfnc(char *fnc) {

	if (get_cv(fnc, 0)) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * Run function without paramters
 */

int perl_exec_simple(char* fnc, char* args[], int flags) {

	app_perl_reset_interpreter();
	if (perl_checkfnc(fnc)) {
		LM_DBG("running perl function \"%s\"", fnc);

		call_argv(fnc, flags, args);
	} else {
		LM_ERR("unknown function '%s' called.\n", fnc);
		return -1;
	}

	return 1;
}

int perl_exec_simple1(struct sip_msg* _msg, char* fnc, char* str2) {
	char *args[] = { NULL };

	return perl_exec_simple(fnc, args, G_DISCARD | G_NOARGS | G_EVAL);
}

int perl_exec_simple2(struct sip_msg* _msg, char* fnc, char* param) {
	char *args[] = { param, NULL };

	return perl_exec_simple(fnc, args, G_DISCARD | G_EVAL);
}

/*
 * Run function, with current SIP message as a parameter
 */
int perl_exec1(struct sip_msg* _msg, char* fnc, char *foobar) {
	return perl_exec2(_msg, fnc, NULL);
}

int perl_exec2(struct sip_msg* _msg, char* fnc, char* mystr) {
	int retval;
	SV *m;
	str reason;

	app_perl_reset_interpreter();

	dSP;

	if (!perl_checkfnc(fnc)) {
		LM_ERR("unknown perl function called.\n");
		reason.s = "Internal error";
		reason.len = sizeof("Internal error")-1;
		if (slb.freply(_msg, 500, &reason) == -1)
		{
			LM_ERR("failed to send reply\n");
		}
		return -1;
	}
	
	switch ((_msg->first_line).type) {
	case SIP_REQUEST:
		if (parse_sip_msg_uri(_msg) < 0) {
			LM_ERR("failed to parse Request-URI\n");

			reason.s = "Bad Request-URI";
			reason.len = sizeof("Bad Request-URI")-1;
			if (slb.freply(_msg, 400, &reason) == -1) {
				LM_ERR("failed to send reply\n");
			}
			return -1;
		}
		break;
	case SIP_REPLY:
		break;
	default:
		LM_ERR("invalid firstline");
		return -1;
	}

	ENTER;				/* everything created after here */
	SAVETMPS;			/* ...is a temporary variable.   */
	PUSHMARK(SP);		/* remember the stack pointer    */

	m = sv_newmortal();
	sv_setref_pv(m, "Kamailio::Message", (void *)_msg);
	SvREADONLY_on(SvRV(m));

	XPUSHs(m);			/* Our reference to the stack... */

	if (mystr)
		XPUSHs(sv_2mortal(newSVpv(mystr, strlen(mystr))));
					/* Our string to the stack... */

	PUTBACK;			/* make local stack pointer global */

	call_pv(fnc, G_EVAL|G_SCALAR);		/* call the function     */
	SPAGAIN;			/* refresh stack pointer         */
	/* pop the return value from stack */
	retval = POPi;

	PUTBACK;
	FREETMPS;			/* free that return value        */
	LEAVE;				/* ...and the XPUSHed "mortal" args.*/

	return retval;
}
