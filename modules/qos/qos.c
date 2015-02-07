/*
 * QoS module - support for tracking dialogs and SDP
 *
 * Copyright (C) 2007 SOMA Networks, Inc.
 * Written by: Ovidiu Sas (osas)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "qos_load.h"
#include "qos_handlers.h" /* also includes sr_module.h needed by
                             handlers */

MODULE_VERSION

static int mod_init(void);
static void mod_destroy(void);


/* The qos message flag value */
static int qos_flag = -1;

/*
 * Binding to the dialog module
 */
struct dlg_binds dialog_st;
struct dlg_binds *dlg_binds = &dialog_st;


static cmd_export_t cmds[]={
	{"load_qos", (cmd_function)load_qos, 0, 0, 0, 0},
	{0,0,0,0,0,0}
};

/*
 * Script parameters
 */
static param_export_t mod_params[]={
	{ "qos_flag",		INT_PARAM, &qos_flag},
	{ 0,0,0 }
};


struct module_exports exports= {
	"qos",           /* module's name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	mod_params,      /* param exports */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,	         /* extra processes */
	mod_init,        /* module initialization function */
	0,               /* reply processing function */
	mod_destroy,     /* Destroy function */
	0                /* per-child init function */
};

int load_qos( struct qos_binds *qosb)
{
	qosb->register_qoscb = register_qoscb;
	return 1;
}


/**
 * The initialization function, called when the module is loaded by
 * the script. This function is called only once.
 *
 * Bind to the dialog module and setup the callbacks. Also initialize
 * the shared memory to store our interninal information in.
 */
static int mod_init(void) 
{
	if (qos_flag == -1) {
		LM_ERR("no qos flag set!!\n");
		return -1;
	} 
	else if (qos_flag > MAX_FLAG) {
		LM_ERR("invalid qos flag %d!!\n", qos_flag);
		return -1;
	}

	/* init callbacks */
	if (init_qos_callbacks()!=0) {
		LM_ERR("cannot init callbacks\n");
		return -1;
	}

	/* Register the main (static) dialog call back.  */
	if (load_dlg_api(&dialog_st) != 0) {
		LM_ERR("Can't load dialog hooks\n");
		return(-1);
	}

	/* Load dialog hooks */
	dialog_st.register_dlgcb(NULL, DLGCB_CREATED, qos_dialog_created_CB, NULL, NULL);

	/*
	 * We are GOOD-TO-GO.
	 */
	return 0;
}

static void mod_destroy(void)
{
	destroy_qos_callbacks();
}

