/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
  
#ifndef IMS_ICSCF_SIP_MESSAGES_H_
#define IMS_ICSCF_SIP_MESSAGES_H_

#define MSG_400_NO_PUBLIC_FROM "Bad Request - Public ID in P-Asserted-Identity or From header missing"
#define MSG_400_NO_PUBLIC "Bad Request - Public ID in To header or Request-URI missing" 
#define MSG_400_NO_PRIVATE "Bad Request - Private ID in Authorization / username token missing" 
#define MSG_400_MALFORMED_CONTACT "Bad Request - Error parsing Contact parameters"
#define MSG_400_NO_VISITED "Bad Request - P-Visited-Network-ID header missing" 

#define MSG_403_UNKOWN_EXPERIMENTAL_RC "Forbidden - HSS responded with unknown Experimental Result Code"
#define MSG_403_UNABLE_TO_COMPLY "Forbidden - HSS Unable to comply"
#define MSG_403_UNKOWN_RC "Forbidden - HSS responded with unknown Result Code"
#define MSG_403_USER_UNKNOWN "Forbidden - HSS User Unknown"
#define MSG_403_IDENTITIES_DONT_MATCH "Forbidden - HSS Identities don't match"
#define MSG_403_ROAMING_NOT_ALLOWED "Forbidden - HSS Roaming not allowed"
#define MSG_403_IDENTITY_NOT_REGISTERED "Forbidden - HSS Identity not registered"
#define MSG_403_AUTHORIZATION_REJECTED "Forbidden - HSS Authorization Rejected"

#define MSG_480_DIAMETER_ERROR "Temporarily Unavailable - Diameter Cx interface failed"
#define MSG_480_DIAMETER_MISSING_AVP "Temporarily unavailable - Missing AVP in UAA from HSS"
#define MSG_480_NOT_REGISTERED "Temporarily Unavailable - HSS Identity not registered"

#define MSG_500_ERROR_SAVING_LIST "Server Error while saving S-CSCF list on I-CSCF"
#define MSG_500_SERVER_ERROR_OUT_OF_MEMORY "Server Error - Out of memory" 

#define MSG_600_EMPTY_LIST "Busy everywhere - Empty list of S-CSCFs"
#define MSG_600_FORWARDING_FAILED "Busy everywhere - Forwarding to S-CSCF failed"

#define MSG_604_USER_UNKNOWN "Does not exist anywhere - HSS User Unknown"

#endif
