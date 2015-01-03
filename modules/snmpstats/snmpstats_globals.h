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
 */

/*!
 * \file
 * \brief SNMP statistic module, globals
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */


#ifndef _SNMP_STATS_GLOBALS_
#define _SNMP_STATS_GLOBALS_

#include "../../sr_module.h"

#define KAMAILIO_OID            1,3,6,1,4,1,34352

/***************************************************************
 * Textual Conventions for BITS types - begins
 *
 * To set a bit  : |= with the define
 * To clear a bit: &= ~(the_define)
 *
 * Example:
 *
 * 1) Setting a minor alarm status:
 *
 * 	currentAlarmStatus |=  TC_ALARM_STATUS_MINOR
 *
 * 2) Clearing a minor alarm status:
 *
 * 	currentAlarmStatus &= ~TC_ALARM_STATUS_MINOR
 */
#define TC_SIP_TRANSPORT_PROTOCOL_OTHER (128>>0)
#define TC_SIP_TRANSPORT_PROTOCOL_UDP   (128>>1)
#define TC_SIP_TRANSPORT_PROTOCOL_TCP   (128>>2)
#define TC_SIP_TRANSPORT_PROTOCOL_SCTP  (128>>3)
#define TC_SIP_TRANSPORT_PROTOCOL_TLS   (128>>4)
#define TC_SIP_TRANSPORT_PROTOCOL_SCTP_TLS   (128>>5)
#define TC_SIP_TRANSPORT_PROTOCOL_WS    (128 >> 6)
#define TC_SIP_TRANSPORT_PROTOCOL_WSS   (128 >> 7)

#define TC_SIP_ENTITY_ROLE_OTHER            (128 >> 0)
#define TC_SIP_ENTITY_ROLE_USER_AGENT       (128 >> 1)
#define TC_SIP_ENTITY_ROLE_PROXY_SERVER     (128 >> 2)
#define TC_SIP_ENTITY_ROLE_REDIRECT_SERVER  (128 >> 3)
#define TC_SIP_ENTITY_ROLE_REGISTRAR_SERVER (128 >> 4)
#define TC_SIP_ENTITY_ROLE_EDGEPROXY_SERVER (128 >> 5)
#define TC_SIP_ENTITY_ROLE_SIPCAPTURE_SERVER (128 >> 6)

#define TC_SIP_OPTION_TAG_REQUIRE       (128 >> 0)
#define TC_SIP_OPTION_TAG_PROXY_REQUIRE (128 >> 1)
#define TC_SIP_OPTION_TAG_SUPPORTED     (128 >> 2)
#define TC_SIP_OPTION_TAG_UNSUPPORTED   (128 >> 3)

#define TC_ALARM_STATUS_UNDER_REPAIR      (128 >> 0)
#define TC_ALARM_STATUS_CRITICAL          (128 >> 1)
#define TC_ALARM_STATUS_MAJOR             (128 >> 2)
#define TC_ALARM_STATUS_MINOR             (128 >> 3)
#define TC_ALARM_STATUS_ALARM_OUTSTANDING (128 >> 4)
#define TC_ALARM_STATUS_UNKNOWN           (128 >> 5)

#define TC_TRANSPORT_PROTOCOL_OTHER (128 >> 0)
#define TC_TRANSPORT_PROTOCOL_UDP   (128 >> 1)
#define TC_TRANSPORT_PROTOCOL_TCP   (128 >> 2)
#define TC_TRANSPORT_PROTOCOL_SCTP  (128 >> 3)
#define TC_TRANSPORT_PROTOCOL_TLS   (128 >> 4)
#define TC_TRANSPORT_PROTOCOL_SCRTP_TLS   (128 >> 5)
#define TC_TRANSPORT_PROTOCOL_WS    (128 >> 6)
#define TC_TRANSPORT_PROTOCOL_WSS   (128 >> 7)

/*
 * Textual Conventions for BITS types - ends
 *************************************************************/



/***************************************************************
 * Textual Conventions for INTEGER types - begins
 */
#define TC_ALARM_STATE_CLEAR    0
#define TC_ALARM_STATE_CRITICAL 1
#define TC_ALARM_STATE_MAJOR    2
#define TC_ALARM_STATE_MINOR    3
#define TC_ALARM_STATE_UNKNOWN  4

#define TC_USAGE_STATE_IDLE    0
#define TC_USAGE_STATE_ACTIVE  1
#define TC_USAGE_STATE_BUSY    2
#define TC_USAGE_STATE_UNKNOWN 3

#define TC_ROWSTATUS_ACTIVE        1
#define TC_ROWSTATUS_NOTINSERVICE  2
#define TC_ROWSTATUS_NOTREADY      3
#define TC_ROWSTATUS_CREATEANDGO   4
#define TC_ROWSTATUS_CREATEANDWAIT 5 
#define TC_ROWSTATUS_DESTROY       6
/*
 * Textual Conventions for INTEGER types - ends
 *************************************************************/


#define TC_TRUE  1
#define TC_FALSE 2

#define SNMPGET_TEMP_FILE "/tmp/kamailio_SNMPAgent.txt"
#define SNMPGET_MAX_BUFFER 80
#define MAX_PROC_BUFFER    256

#define MAX_USER_LOOKUP_COUNTER 255

#define HASH_SIZE 32

extern unsigned int global_UserLookupCounter;

/*******************************************************************************
 * Configuration File Handler Prototypes
 */

/*! Handles setting of the sip entity type parameter. */
int handleSipEntityType( modparam_t type, void* val);

/*! Handles setting of the Msg Queue Depth Minor Threshold */
int set_queue_minor_threshold(modparam_t type, void *val);

/*! Handles setting of the Msg Queue Depth Major Threshold */
int set_queue_major_threshold(modparam_t type, void *val);

/*! Handles setting of the dialog minor threshold */
int set_dlg_minor_threshold(modparam_t type, void *val);

/*! Handles setting of the dialog major threshold */
int set_dlg_major_threshold(modparam_t type, void *val);

/*! Handles setting of the path to the snmpget binary. */
int set_snmpget_path( modparam_t type, void *val);

/*! Handles setting of the snmp community string. */
int set_snmp_community( modparam_t type, void *val);


#endif
