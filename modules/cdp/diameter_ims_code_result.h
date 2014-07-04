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

#ifndef __DIAMETER_IMS_CODE_RESULT_H
#define __DIAMETER_IMS_CODE_RESULT_H



/**	IMS Specific Result Codes			*/
enum{
/** 1001 to 1999	Informational			*/
/** 2001 to 2999	Success					*/
/**	2001 to 2020 Reserved for TS29.229	*/
	RC_IMS_DIAMETER_FIRST_REGISTRATION 					= 2001,
	RC_IMS_DIAMETER_SUBSEQUENT_REGISTRATION				= 2002,
	RC_IMS_DIAMETER_UNREGISTERED_SERVICE				= 2003,
	RC_IMS_DIAMETER_SUCCESS_SERVER_NAME_NOT_STORED		= 2004,
	RC_IMS_DIAMETER_SERVER_SELECTION					= 2005,
/**	2401 to 2420 Reserved for TS29.109	*/
/** 4001 to 4999	Transient Failures	*/
/**	4100 to 4120 Reserved for TS29.329	*/
	RC_IMS_DIAMETER_USER_DATA_NOT_AVAILABLE 			= 4100,
	RC_IMS_DIAMETER_PRIOR_UPDATE_IN_PROGRESS			= 4101,
/**	41xx to 41yy Reserved for TS32.299	*/
/** 5001 to 5999	Permanent Failures		*/
/**	5001 to 5020 Reserved for TS29.229	*/
	RC_IMS_DIAMETER_ERROR_USER_UNKNOWN					= 5001,
	RC_IMS_DIAMETER_ERROR_IDENTITIES_DONT_MATCH			= 5002,
	RC_IMS_DIAMETER_ERROR_IDENTITY_NOT_REGISTERED		= 5003,
	RC_IMS_DIAMETER_ERROR_ROAMING_NOT_ALLOWED			= 5004,
	RC_IMS_DIAMETER_ERROR_IDENTITY_ALREADY_REGISTERED	= 5005,
	RC_IMS_DIAMETER_ERROR_AUTH_SCHEME_NOT_SUPPORTED		= 5006,
	RC_IMS_DIAMETER_ERROR_IN_ASSIGNMENT_TYPE			= 5007,
	RC_IMS_DIAMETER_ERROR_TOO_MUCH_DATA					= 5008,
	RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA		= 5009,
	RC_IMS_DIAMETER_MISSING_USER_ID						= 5010,
	RC_IMS_DIAMETER_ERROR_FEATURE_UNSUPPORTED			= 5011,
/**	5021 to 5040 Reserved for TS32.299	*/
/**	5041 to 5060 Reserved for TS29.234	*/
/**	5061 to 5080 Reserved for TS29.209	*/
	RC_IMS_DIAMETER_ERROR_INVALID_SERVICE_INFORMATION	= 5061,
	RC_IMS_DIAMETER_ERROR_FILTER_RESTRICTIONS			= 5062,
/**	5061 to 5066 Reserved for TS29.214	*/
	RC_IMS_DIAMETER_ERROR_REQUESTED_SERVICE_NOT_AUTHORIZED		= 5063,
	RC_IMS_DIAMETER_ERROR_DUPLICATED_AF_SESSION			= 5064,
	RC_IMS_DIAMETER_ERROR_IPCAN_SESSION_NOT_AVAILABLE		= 5065,
	RC_IMS_DIAMETER_ERROR_UNAUTHORIZED_NON_EMERGENCY_SESSION	= 5066,
/**	5100 to 5119 Reserved for TS29.329	*/
	RC_IMS_DIAMETER_ERROR_USER_DATA_NOT_RECOGNIZED		= 5100,
	RC_IMS_DIAMETER_ERROR_OPERATION_NOT_ALLOWED			= 5101,
	RC_IMS_DIAMETER_ERROR_USER_DATA_CANNOT_BE_READ		= 5102,
	RC_IMS_DIAMETER_ERROR_USER_DATA_CANNOT_BE_MODIFIED	= 5103,
	RC_IMS_DIAMETER_ERROR_USER_DATA_CANNOT_BE_NOTIFIED	= 5104,
	RC_IMS_DIAMETER_ERROR_TRANSPARENT_DATA_OUT_OF_SYNC	= 5105,
	RC_IMS_DIAMETER_ERROR_SUBS_DATA_ABSENT				= 5106,
	RC_IMS_DIAMETER_ERROR_NO_SUBSCRIPTION_TO_DATA		= 5107,
	RC_IMS_DIAMETER_ERROR_DSAI_NOT_AVAILABLE 			= 5108
/** 5400 to 5419 Reserved for TS29.109	*/
};

#endif /* __DIAMETER_IMS_CODE_RESULT_H */
