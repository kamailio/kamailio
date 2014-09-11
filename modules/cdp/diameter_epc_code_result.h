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

#ifndef DIAMETER_EPC_CODE_RESULT_H_
#define DIAMETER_EPC_CODE_RESULT_H_

/**	EPC Specific Result Codes				*/
enum{
/** 1001 to 1999	Informational			*/
/** 2001 to 2999	Success					*/
/** 4001 to 4999	Transient Failures		*/
	RC_EPC_DIAMETER_END_USER_SERVICE_DENIED				= 4010, //TS 32.299
	RC_EPC_DIAMETER_CREDIT_CONTROL_NOT_APPLICABLE		= 4011, //TS 32.299
	RC_EPC_DIAMETER_CREDIT_LIMIT_REACHED				= 4012, //TS 32.299
	RC_EPC_DIAMETER_AUTHENTICATION_DATA_UNAVAILABLE		= 4181,
/** 5001 to 5999	Permanent Failures		*/
	RC_EPC_DIAMETER_ERROR_USER_UNKNOWN					= 5001,
	RC_EPC_DIAMETER_ERROR_IDENTITY_NOT_REGISTERED		= 5003, //TS 29.273
	RC_EPC_DIAMETER_AUTHORIZATION_REJECTED				= 5003, //TS 32.299
	RC_EPC_DIAMETER_ERROR_ROAMING_NOT_ALLOWED			= 5004, //TS 29.273
	RC_EPC_DIAMETER_ERROR_IDENTITY_ALREADY_REGISTERED	= 5005, //TS 29.273	
	RC_EPC_DIAMETER_USER_UNKNOWN						= 5030, //TS 32.299
	RC_EPC_DIAMETER_RATING_FAILED						= 5031, //TS 32.299
	RC_EPC_DIAMETER_ERROR_UNKNOWN_EPS_SUBSCRIPTION		= 5420,
	RC_EPC_DIAMETER_ERROR_RAT_NOT_ALLOWED				= 5421,
	RC_EPC_DIAMETER_ERROR_EQUIPMENT_UNKNOWN				= 5422,
	RC_EPC_DIAMETER_ERROR_USER_NO_NON_3GPP_SUBSCRIPTION = 5450, //TS 29.273
	RC_EPC_DIAMETER_ERROR_USER_NO_APN_SUBSCRIPTION		= 5451, //TS 29.273
	RC_EPC_DIAMETER_ERROR_RAT_TYPE_NOT_ALLOWED			= 5452, //TS 29.273
};



#endif /*DIAMETER_EPC_CODE_RESULT_H_*/
