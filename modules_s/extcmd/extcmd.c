/*
 * $Id$
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../resolve.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump_rpl.h"
#include "../im/im_load.h"
#include "../tm/tm_load.h"
#include "extcmd_funcs.h"


#define append_str(_p,_s,_l) \
	{memcpy((_p),(_s),(_l));\
	(_p) += (_l);}


static int extcmd_init(void);
static int extcmd_child_init(int rank);
//static int dump_message(struct sip_msg*, char*, char* );



/* parameters */
char *my_address = 0;
int  my_port = 7890;

/* global variables */
struct tm_binds tmb;
struct im_binds imb;
int    server_sock;
int    rpl_pipe[2];
int    req_pipe[2];


struct module_exports exports= {
	"extcmd",
	(char*[]){
				"extcmd_dump_req"
			},
	(cmd_function[]){
					dump_request
					},
	(int[]){
				0
			},
	(fixup_function[]){
				0
		},
	1,

	(char*[]) {   /* Module parameter names */
		"listen_address",
		"listen_port"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&my_address,
		&my_port
	},
	2,      /* Number of module paramers */

	extcmd_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function)  0,   /* module exit function */
	0,
	(child_init_function) extcmd_child_init  /* per-child init function */
};




static int extcmd_init(void)
{
	load_tm_f  load_tm;
	load_im_f  load_im;
	struct hostent* he;
	union sockaddr_union me;

	DBG("EXTCMD - initializing\n");

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT))) {
		LOG(L_ERR, "ERROR: extcmd: global_init: cannot import load_tm\n");
		goto error;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) 
		goto error;

	/* import the IM auto-loading function */
	if ( !(load_im=(load_im_f)find_export("load_im", 1))) {
		LOG(L_ERR, "ERROR: extcmd: global_init: cannot import load_im\n");
		goto error;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_im( &imb )==-1) 
		goto error;

	/* check the parametre */
	if ( !my_address) {
		LOG(L_ERR,"ERROR:extcmd_init: no address specified for listening!!\n");
		goto error;
	}

	/* build a sockaddr_union for the listen socket */
	he = resolvehost(my_address);
	if (!he) {
		LOG(L_ERR,"ERROR:extcmd_init: cannot solve \"%s\"!!\n",my_address);
		goto error;
	}
	if (hostent2su( &me, he, 0, htons(my_port))==-1)
		goto error;

	/* create the socket server */
	server_sock = socket( he->h_addrtype, SOCK_STREAM, 0);
	if (server_sock<0) {
		LOG(L_ERR,"ERROR:extcmd_init: cannot create server socket! %s\n",
			strerror(errno));
		goto error;
	}

	/* bind the socket! */
	if (bind( server_sock, (struct sockaddr*)&me, sizeof(me) )==-1) {
		LOG(L_ERR,"ERROR:extcmd_init: cannot bind to %s:%d! Reason=%s\n",
			my_address, my_port, strerror(errno) );
		goto error;
	}

	/* now, let's create the pipes for rpl and req */
	if ( pipe(rpl_pipe)==-1 )  {
		LOG(L_ERR,"ERROR:extcmd_init: cannot reply pipe! Reason=%s\n",
			strerror(errno) );
		goto error;
	}
	if ( pipe(req_pipe)==-1 )  {
		LOG(L_ERR,"ERROR:extcmd_init: cannot request pipe! Reason=%s\n",
			strerror(errno) );
		goto error;
	}


	return 0;
error:
	return -1;
}




int extcmd_child_init(int rank)
{
	int foo;

	/* only the child 0 will fork */
	if (rank==0) {
		/* creats a process that listen for connetions */
		if ( (foo=fork())<0 ) {
			LOG(L_ERR,"ERROR: extcmd_child_init: cannot fork \n");
			goto error;
		}
		/* the child will run the listening rutine ;-) */
		if (!foo) {
			close( rpl_pipe[1] );
			close( req_pipe[1] );
			extcmd_server_process( server_sock );
		}
	}

	/* close the reading ends of pipes */
	close( rpl_pipe[0] );
	close( req_pipe[0] );

	return 0;
error:
	return-1;
}





