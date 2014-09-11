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


#ifndef S_CSCF_SIP_MESSAGES_H_
#define S_CSCF_SIP_MESSAGES_H_


#define MSG_200_SAR_OK					"OK - SAR succesful and registrar saved"

#define MSG_400_BAD_REQUEST				"Bad Request"
#define MSG_400_BAD_Contact 			"Bad Request - Error parsing Contact header" 

#define MSG_401_CHALLENGE				"Unauthorized - Challenging the UE"
#define MSG_407_CHALLENGE				"Unauthorized - Challenging the UE"

#define MSG_403_NO_PRIVATE				"Forbidden - Private identity not found (Authorization: username)"
#define MSG_403_NO_PUBLIC				"Forbidden - Public identity not found (To:)"
#define MSG_403_NO_NONCE				"Forbidden - Nonce not found (Authorization: nonce)"
#define MSG_403_UNKOWN_RC				"Forbidden - HSS responded with unknown Result Code"
#define MSG_403_UNKOWN_EXPERIMENTAL_RC	"Forbidden - HSS responded with unknown Experimental Result Code"
#define MSG_403_USER_UNKNOWN			"Forbidden - HSS User Unknown"
#define MSG_403_IDENTITIES_DONT_MATCH	"Forbidden - HSS Identities don't match"
#define MSG_403_AUTH_SCHEME_UNSOPPORTED "Forbidden - HSS Authentication Scheme Unsupported"
#define MSG_403_UNABLE_TO_COMPLY		"Forbidden - HSS Unable to comply"
#define MSG_403_NO_AUTH_DATA			"Forbidden - HSS returned no authentication vectors"

#define MSG_423_INTERVAL_TOO_BRIEF 		"Interval too brief"

#define MSG_480_HSS_ERROR 				"Temporarily unavailable - error retrieving av"
#define MSG_480_DIAMETER_ERROR			"Temporarily Unavailable - Diameter Cx interface failed"
#define MSG_480_DIAMETER_TIMEOUT		"Temporarily unavailable - TimeOut in MAR/A HSS"
#define MSG_480_DIAMETER_TIMEOUT_SAR	"Temporarily unavailable - TimeOut in SAR/A HSS"
#define MSG_480_DIAMETER_MISSING_AVP	"Temporarily unavailable - Missing AVP in UAA from HSS"

#define MSG_500_PACK_AV					"Server Internal Error - while packing auth vectors"
#define MSG_500_SAR_FAILED				"Server Internal Error - Server Assignment failed"
#define MSG_500_UPDATE_CONTACTS_FAILED	"Server Internal Error - Update Contacts failed"
#define MSG_514_HSS_AUTH_FAILURE		"HSS unauthenticated - did not provide the right H(A1) in MAA"

#endif //S_CSCF_SIP_MESSAGES_H_
