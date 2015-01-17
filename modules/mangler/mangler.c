/*
 * mangler module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"


#include "mangler.h"
#include "sdp_mangler.h"
#include "contact_ops.h"
#include "utils.h"
#include "common.h"

#ifdef DEMO

#include "../../modules/tm/t_hooks.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/h_table.h"
struct tm_binds tmb;

#endif


MODULE_VERSION


/*
 * Module destroy function prototype
 */
static void destroy (void);



/*
 * Module initialization function prototype
 */
static int mod_init (void);

#if 0 /* not used -- Wall complains */
/* Header field fixup */
static int fixup_char2str(void** param, int param_no);
static int fixup_char2int (void **param, int param_no);
static int fixup_char2uint (void **param, int param_no);
#endif


char *contact_flds_separator = DEFAULT_SEPARATOR;



static param_export_t params[] = {
								{"contact_flds_separator",PARAM_STRING,&contact_flds_separator},
								{0, 0, 0}
								};	/*no params exported,perhaps I should add pre-compiled expressions */



/*
 * Exported functions
 */
static cmd_export_t cmds[] =
{
	{"sdp_mangle_ip", sdp_mangle_ip, 2,0, REQUEST_ROUTE|ONREPLY_ROUTE}, // fixup_char2str?
	{"sdp_mangle_port",sdp_mangle_port, 1,0, REQUEST_ROUTE|ONREPLY_ROUTE},// fixup_char2int if I use an int as offset
	{"encode_contact",encode_contact,2,0,REQUEST_ROUTE|ONREPLY_ROUTE},//fixup_char2str
	{"decode_contact",decode_contact,0,0,REQUEST_ROUTE},
	{"decode_contact_header",decode_contact_header,0,0,REQUEST_ROUTE|ONREPLY_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"mangler",
	cmds,			/* Exported functions */
	0,                      /* RPC methods */
	params,			/* Exported parameters */
	mod_init,		/* module initialization function */
	0,			/* response function */
	destroy,		/* destroy function */
	0,			/* oncancel function */
	0			/* child initialization function */
};


#ifdef DEMO
/* MANGLING EXAMPLE */
/* ================================================================= */
static void func_invite(struct cell *t,struct sip_msg *msg,int code,void *param)
{
	int i;
	//callback function
	if (!check_transaction_quadruple(msg))
		{
		//we do not have a correct message from/callid/cseq/to
		return;
		}
	i = encode_contact(msg,"enc_prefix","193.175.135.38");
	fprintf(stdout,"decode/encode = returned %d\n",i);fflush(stdout);

	if (t->is_invite)
		{
			if (msg->buf != NULL)
			{
			fprintf(stdout,"INVITE:received \n%s\n",msg->buf);fflush(stdout);
			i = sdp_mangle_port(msg,"1000",NULL);
			fprintf(stdout,"sdp_mangle_port returned %d\n",i);fflush(stdout);
			i = sdp_mangle_ip(msg,"10.0.0.0/16","123.124.125.126");
			fprintf(stdout,"sdp_mangle_ip returned %d\n",i);fflush(stdout);

			}
			else fprintf(stdout,"INVITE:received NULL\n");fflush(stdout);
		}
	else
		{
			fprintf(stdout,"NOT INVITE(REGISTER?) received \n%s\n",msg->buf);fflush(stdout);
			//i = decode_contact(msg,NULL,NULL);
			//fprintf(stdout,"decode/encode = returned %d\n",i);fflush(stdout);
		}
	fflush(stdout);
}

#endif


int
prepare ()
{

	/* using pre-compiled expressions to speed things up*/
	compile_expresions(PORT_REGEX,IP_REGEX);

#ifdef DEMO
	load_tm_f load_tm;


	fprintf(stdout,"===============NEW RUN================\n");
	//register callbacks

	if (!(load_tm=(load_tm_f)find_export("load_tm",NO_SCRIPT,0)))
	{
		printf("Error:FCP:prepare:cannot import load_tm\n");
		return -1;
	}
	if (load_tm(&tmb)==-1) return -1;

	if (tmb.register_tmcb(TMCB_REQUEST_OUT, func_invite, 0, 0) <= 0) return -1;
#endif
	return 0;
}



static int
mod_init (void)
{
	ipExpression = NULL;
	portExpression = NULL;
	prepare ();
	/*
	 * Might consider to compile at load time some regex to avoid compilation
	 * every time I use this functions
	 */
	return 0;
}


static void
destroy (void)
{
	/*free some compiled regex expressions */
	free_compiled_expresions();
#ifdef DEMO
	fprintf(stdout,"Freeing pre-compiled expressions\n");
#endif

	return;
}

#ifdef O
static int fixup_char2int (void **param, int param_no)
{
	int offset,res;
	if (param_no == 1)
	{
		res = sscanf(*param,"%d",&offset);
		if (res != 1)
			{
			LOG (L_ERR,"fixup_char2int:Invalid value %s\n",(char *)(*param));
			return -1;
			}
		free(*param);
		*param = (void *)offset;/* value of offset */
	}

	return 0;
}

static int fixup_char2uint (void **param, int param_no)
{
	int res;
	unsigned int newContentLength;
	if (param_no == 1)
	{
		res = sscanf(*param,"%u",&newContentLength);
		if (res != 1)
			{
			LOG (L_ERR,"fixup_char2uint:Invalid value %s\n",(char *)*param);
			return -1;
			}
		free(*param);
		*param = (void *)newContentLength;
	}

	return 0;
}



static int fixup_char2str(void** param, int param_no)
{
	str* s;

	if (param_no == 1)
	{
		s = (str*)pkg_malloc(sizeof(str));
		if (!s)
		{
			LOG(L_ERR, "fixup_char2str: No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}
	else if (param_no == 2)
	{
		s = (str*)pkg_malloc(sizeof(str));
		if (!s)
		{
			LOG(L_ERR, "fixup_char2str: No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}
#endif
