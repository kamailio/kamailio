/*
 * Include file for RADIUS
 *
 * Copyright (C) 2001-2003 FhG FOKUS
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
 */
/*!
 * \file
 * \brief Kamailio core :: Radius include file
 * \ingroup core
 * Module: \ref core
 *
 * \note WARNING: Don't forget to update sip_router/etc/dictionary.sip-router if you 
 * update this file !
 */

#ifndef _RAD_DICT_H
#define _RAD_DICT_H

struct attr {
	const char *n;
	int v;
};

struct val {
	const char *n;
	int v;
};

typedef enum rad_attr {
	     /* Standard attributes according to RFC2865 and RFC2866 */
	A_USER_NAME = 0,
	A_NAS_IP_ADDRESS,
	A_NAS_PORT,
	A_SERVICE_TYPE,
	A_CALLED_STATION_ID,
	A_CALLING_STATION_ID,
	A_ACCT_STATUS_TYPE,
	A_ACCT_SESSION_ID,
	A_ACCT_SESSION_TIME,

	     /* Attributes according to draft-schulzrinne-sipping-radius-accounting-00 */
	A_SIP_METHOD,
	A_SIP_RESPONSE_CODE,
	A_SIP_CSEQ,
	A_SIP_TO_TAG,
	A_SIP_FROM_TAG,
	A_SIP_BRANCH_ID,
	A_SIP_TRANSLATED_REQUEST_ID,
	A_SIP_SOURCE_IP_ADDRESS,
	A_SIP_SOURCE_PORT,
	
	     /* Attributes according to draft-sterman-aaa-sip-00 */
	A_DIGEST_RESPONSE,
	A_DIGEST_REALM,
	A_DIGEST_NONCE,
	A_DIGEST_METHOD,
	A_DIGEST_URI,
	A_DIGEST_QOP,
	A_DIGEST_ALGORITHM,
	A_DIGEST_BODY_DIGEST,
	A_DIGEST_CNONCE,
	A_DIGEST_NONCE_COUNT,
	A_DIGEST_USER_NAME,

	     /* To be deprecated in the future */

	     /* SER-specific attributes */
	A_SER_FROM,
	A_SER_FLAGS,
	A_SER_ORIGINAL_REQUEST_ID,
	A_SER_TO,
	A_SER_DIGEST_USERNAME,
	A_SER_DIGEST_REALM,
	A_SER_REQUEST_TIMESTAMP,
	A_SER_TO_DID,
	A_SER_FROM_UID,
	A_SER_FROM_DID,
	A_SER_TO_UID,
	A_SER_RESPONSE_TIMESTAMP,
	A_SER_ATTR,
	A_SER_SERVICE_TYPE,
	A_SER_DID,
	A_SER_UID,
	A_SER_DOMAIN,
	A_SER_URI_USER,
	A_SER_URI_SCHEME,
	A_SER_SERVER_ID,

	     /* CISCO Vendor Specific Attributes */
	A_CISCO_AVPAIR,
	A_MAX
} rad_attr_t;


typedef enum rad_val {
	 /* Acct-Status-Type */
	V_START = 0,
	V_STOP,
	V_INTERIM_UPDATE,
	V_FAILED,

	     /* Service-Type */
	V_SIP_SESSION,
	V_CALL_CHECK,

	     /* SER-Service-Type */
	V_GET_URI_ATTRS,
	V_GET_USER_ATTRS,
	V_DIGEST_AUTHENTICATION,
	V_GET_DOMAIN_ATTRS,
	V_GET_GLOBAL_ATTRS,
	V_LOOKUP_DOMAIN,

	V_MAX
} rad_val_t;


/*
 * Search the RADIUS dictionary for codes of all attributes
 * and values defined above
 */
#define	INIT_AV(rh, at, vl, fn, e1, e2)					\
{									\
	int i;								\
	DICT_ATTR *da;							\
	DICT_VALUE *dv;							\
									\
	for (i = 0; i < A_MAX; i++) {					\
		if (at[i].n == NULL)					\
			continue;					\
		da = rc_dict_findattr(rh, at[i].n);			\
		if (da == NULL) {					\
			LM_ERR("%s: can't get code for %s attr\n",	\
					fn, at[i].n);			\
			return e1;					\
		}							\
		at[i].v = da->value;					\
	}								\
	for (i = 0; i < V_MAX; i++) {					\
		if (vl[i].n == NULL)					\
			continue;					\
		dv = rc_dict_findval(rh, vl[i].n);			\
		if (dv == NULL) {					\
			LM_ERR("%s: can't get code for %s attr value\n",\
					fn, vl[i].n);			\
			return e2;					\
		}							\
		vl[i].v = dv->value;					\
	}								\
}

#endif /* _RAD_DICT_H */
