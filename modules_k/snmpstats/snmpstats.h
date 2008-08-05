/*
 * $Id$
 *
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 * 
 * Structure and prototype definitions for the SNMPStats module.
 *
 * There are some important points to understanding the SNMPStat modules
 * architecture.
 *
 * 1) The SNMPStats module will fork off a new process in mod_child_init when
 *    the rank is equal to PROC_MAIN_PROCESS.  The sub-process will be
 *    responsible for registering with a master agent (the source of snmp
 *    requests), and handling all received requests. 
 *
 * 2) The Module will register a periodic alarm checking function with a sip
 *    timer using register_timer().  This function checks for alarm conditions,
 *    and will send out traps to the master agent when it detects their
 *    presence.
 *
 * 3) The SNMPStats module is required to run an external application upon
 *    startup, to collect sysUpTime data from the master agent.  This involves
 *    spawning a short-lived process.  For this reason, the module temporarily
 *    installs a new SIGCHLD handler to deal specifically with this process.  It
 *    does not change the normal SIGCHLD behaviour for any process except for
 *    this short lived sysUpTime process. 
 *
 * 4) mod_init() will initialize some interprocess communication buffers, as
 *    well as callback mechanisms for the usrloc module.  To understand what the
 *    interprocess buffer and callbacks are and are for, please see the
 *    respective comments in interprocess_buffer.h, openserSIPRegUserTable.h,
 *    and openserSIPContactTable.h.
 */

#ifndef _SNMP_STATS_
#define _SNMP_STATS_

#include "../../statistics.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "snmpstats_globals.h"
#include "sub_agent.h"

#define SNMPSTATS_MODULE_NAME "snmpstats"
#define SYSUPTIME_OID         ".1.3.6.1.2.1.1.3.0"

/* This is the first function to be called by Kamailio, to initialize the module.
 * This call must always return a value as soon as possible.  If it were not to
 * return, then Kamailio would not be able to initialize any of the other
 * modules. */
static int  mod_init(void);

/* This function is called when Kamailio has finished creating all instances of
 * itself.  It is at this point that we want to create our AgentX sub-agent
 * process, and register a handler for any state changes of our child. */
static int  mod_child_init(int rank);


/* This function is called when Kamailio is shutting down.  When this happens, we
 * log a useful message and kill the AgentX Sub-Agent child process */
static void mod_destroy(void);


static proc_export_t mod_procs[] = {
	{"SNMP AgentX",  0,  0, agentx_child, 1 },
	{0,0,0,0,0}
};


/*
 * This structure defines the SNMPStats parameters that can be configured
 * through the kamailio.cfg configuration file.  
 */
static param_export_t mod_params[] = 
{
	{ "sipEntityType",          STR_PARAM|USE_FUNC_PARAM,
			(void *)handleSipEntityType       },
	{ "MsgQueueMinorThreshold", INT_PARAM|USE_FUNC_PARAM,
			(void *)set_queue_minor_threshold }, 
	{ "MsgQueueMajorThreshold", INT_PARAM|USE_FUNC_PARAM,
			(void *)set_queue_major_threshold }, 
	{ "dlg_minor_threshold",    INT_PARAM|USE_FUNC_PARAM,
			(void *)set_dlg_minor_threshold   },
	{ "dlg_major_threshold",    INT_PARAM|USE_FUNC_PARAM,
			(void *)set_dlg_major_threshold   },
	{ "snmpgetPath",            STR_PARAM|USE_FUNC_PARAM,
			(void *)set_snmpget_path          },
	{ "snmpCommunity",          STR_PARAM|USE_FUNC_PARAM,
			(void *)set_snmp_community        }, 
	{ 0,0,0 }
};


struct module_exports exports = 
{
	SNMPSTATS_MODULE_NAME,   /* module's name */
	DEFAULT_DLFLAGS,         /* dlopen flags */
	0,                       /* exported functions */
	mod_params,              /* param exports */
	0,                       /* exported statistics */
	0,                       /* MI Functions */
	0,                       /* pseudo-variables */
	mod_procs,               /* extra processes */
	mod_init,                /* module initialization function */
	0,                       /* reply processing function */
	mod_destroy,   /* Destroy function */
	mod_child_init /* per-child init function */
};

#endif
