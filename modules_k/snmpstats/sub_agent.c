/*
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 *
 * This file defines all functions required to establish a relationship with a
 * master agent.  
 */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "sub_agent.h"

/* Bring in the NetSNMP headers */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/* Bring in the initialization functions for all scalars */
#include "openserSIPCommonObjects.h"
#include "openserSIPServerObjects.h"
#include "openserObjects.h"

/* Bring in the initialization functions for all tables */
#include "openserSIPPortTable.h"
#include "openserSIPMethodSupportedTable.h"
#include "openserSIPStatusCodesTable.h"
#include "openserSIPRegUserTable.h"
#include "openserSIPContactTable.h"
#include "openserSIPRegUserLookupTable.h"
#include "openserMIBNotifications.h"

#include "../../dprint.h"

static int keep_running;

/* The function handles Handles shutting down of the sub_agent process. */
static void sigterm_handler(int signal) 
{
	/* Just exit.  The master agent will clean everything up for us */
	exit(0);
}

/* This function:
 *
 *   1) Registers itself with the Master Agent
 *
 *   2) Initializes all of the SNMPStats modules scalars and tables, while
 *      simultaneously registering their respective SNMP OID's and handlers 
 *      with the master agent.
 *
 *   3) Repeatedly checks for new SNMP messages to process
 *
 * Note: This function never returns, so it should always be called from a 
 *       sub-process. 
 *
 */
static int initialize_agentx(void) 
{
	/* We register with a master agent */
	register_with_master_agent(AGENT_PROCESS_NAME);
	
	/* Initialize all scalars, and let the master agent know we want to
	 * handle all OID's pertaining to these scalars. */
	init_openserSIPCommonObjects();
	init_openserSIPServerObjects();
	init_openserObjects();

	/* Initialiaze all the tables, and let the master agent know we want to
	 * handle all the OID's pertaining to these tables */
	init_openserSIPPortTable();
	init_openserSIPMethodSupportedTable();
	init_openserSIPStatusCodesTable();
	init_openserSIPRegUserTable();
	init_openserSIPContactTable();
	init_openserSIPRegUserLookupTable();

	/* In case we recevie a request to stop (kill -TERM or kill -INT) */
	keep_running = 1;

	while(keep_running) {
		agent_check_and_process(1); /* 0 == don't block */
	}

	snmp_shutdown(AGENT_PROCESS_NAME);
	SOCK_CLEANUP;
	exit (0);

	return 0;
}

/* Creates a child that will become the AgentX sub-agent.  The child will
 * insulate itself from the rest of OpenSER by overriding most of signal
 * handlers. */
void agentx_child(int rank)
{
	struct sigaction new_sigterm_handler;
	struct sigaction default_handlers;
	struct sigaction sigpipe_handler;

	/* Setup a SIGTERM handler */
	sigfillset(&new_sigterm_handler.sa_mask);
	new_sigterm_handler.sa_flags   = 0;
	new_sigterm_handler.sa_handler = sigterm_handler;
	sigaction(SIGTERM, &new_sigterm_handler, NULL);

	/* We don't want OpenSER's normal handlers doing anything when
	 * we die.  As far as OpenSER knows this process never existed.
	 * So override all signal handlers to the OS default. */
	sigemptyset(&default_handlers.sa_mask);
	default_handlers.sa_flags = 0;
	default_handlers.sa_handler = SIG_DFL;

	sigaction(SIGCHLD, &default_handlers, NULL);
	sigaction(SIGINT,  &default_handlers, NULL);
	sigaction(SIGHUP,  &default_handlers, NULL);
	sigaction(SIGUSR1, &default_handlers, NULL);
	sigaction(SIGUSR2, &default_handlers, NULL);

	/* It is possible that the master agent will unregister us if we
	 * take too long to respond to an SNMP request.  This would
	 * happen if a large number of users/contacts have been
	 * registered between snmp requests to the user/contact tables.
	 * In this situation we may try to write to a closed socket when
	 * we are done processing, resulting in a SIGPIPE.  This doesn't
	 * need to be fatal however, because we can re-establish our
	 * connection.  Therefore we set ourselves up to ignore the
	 * SIGPIPE. */
	sigpipe_handler.sa_flags = SA_RESTART;
	sigpipe_handler.sa_handler = SIG_IGN;

	sigaction(SIGPIPE, &sigpipe_handler, NULL);

	initialize_agentx();
}



/* This function opens up a connection with the master agent specified in
 * the snmpstats modules configuration file */
void register_with_master_agent(char *name_to_register_under) 
{
	/* Set ourselves up as an AgentX Client. */
	netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

	/* Initialize TCP if necessary.  (Its here for WIN32) compatibility. */
	SOCK_STARTUP;

	/* Read in our configuration file to determine master agent ping times
	 * what port communication is to take place over, etc. */
	init_agent("snmpstats");

	/* Use a name we can register our agent under. */
	init_snmp(name_to_register_under);
}

