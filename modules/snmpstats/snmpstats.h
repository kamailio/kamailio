/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
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
 *    respective comments in interprocess_buffer.h, kamailioSIPRegUserTable.h,
 *    and kamailioSIPContactTable.h.
 */

/*!
 * \file
 * \brief SNMP statistic module
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */

#ifndef _SNMP_STATS_
#define _SNMP_STATS_

#define SNMPSTATS_MODULE_NAME "snmpstats"
#define SYSUPTIME_OID         ".1.3.6.1.2.1.1.3.0"


#endif
