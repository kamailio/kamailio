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
 * This file implements all scalars defined in the KAMAILIO-SIP-SERVER-MIB.  
 * For a full description of the scalars, please see KAMAILIO-SIP-SERVER-MIB.
 *
 */
#ifndef KAMAILIOSIPSERVEROBJECTS_H
#define KAMAILIOSIPSERVEROBJECTS_H

/* function declarations */
void init_kamailioSIPServerObjects(void);
Netsnmp_Node_Handler handle_kamailioSIPProxyStatefulness;
Netsnmp_Node_Handler handle_kamailioSIPProxyRecordRoute;
Netsnmp_Node_Handler handle_kamailioSIPProxyAuthMethod;
Netsnmp_Node_Handler handle_kamailioSIPNumProxyRequireFailures;
Netsnmp_Node_Handler handle_kamailioSIPRegMaxContactExpiryDuration;
Netsnmp_Node_Handler handle_kamailioSIPRegMaxUsers;
Netsnmp_Node_Handler handle_kamailioSIPRegCurrentUsers;
Netsnmp_Node_Handler handle_kamailioSIPRegDfltRegActiveInterval;
Netsnmp_Node_Handler handle_kamailioSIPRegUserLookupCounter;
Netsnmp_Node_Handler handle_kamailioSIPRegAcceptedRegistrations;
Netsnmp_Node_Handler handle_kamailioSIPRegRejectedRegistrations;

#define PROXY_STATEFULNESS_STATELESS            1
#define PROXY_STATEFULNESS_TRANSACTION_STATEFUL 2
#define PROXY_STATEFULNESS_CALL_STATEFUL        3

#define SIP_AUTH_METHOD_NONE   (128 >> 0)
#define SIP_AUTH_METHOD_TLS    (128 >> 1)
#define SIP_AUTH_METHOD_DIGEST (128 >> 2)

#endif /* KAMAILIOSIPSERVEROBJECTS_H */
