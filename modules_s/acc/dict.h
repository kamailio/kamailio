/*
 * Include file for Radius 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* see http://www.iptel.org/ietf/aaa/draft-schulzrinne-sipping-radius-accounting-00.txt */

/*
 * WARNING: Don't forget to update the dictionary if you update this file !!!
 */

#ifndef _DICT_H
#define _DICT_H

struct attr {
	const char *n;
	int v;
};

struct val {
	const char *n;
	int v;
};

#define	A_USER_NAME			0
#define	A_SERVICE_TYPE			1
#define	A_CALLED_STATION_ID		2
#define	A_CALLING_STATION_ID		3
#define	A_ACCT_STATUS_TYPE		4
#define	A_ACCT_SESSION_ID		5
#define	A_SIP_METHOD			6
#define	A_SIP_RESPONSE_CODE		7
#define	A_SIP_CSEQ			8
#define	A_SIP_TO_TAG			9
#define	A_SIP_FROM_TAG			10
#define	A_SIP_TRANSLATED_REQUEST_URI	11
#define	A_DIGEST_RESPONSE		12
#define	A_DIGEST_ATTRIBUTES		13
#define	A_SIP_URI_USER			14
#define	A_SIP_RPID			15
#define	A_DIGEST_REALM			16
#define	A_DIGEST_NONCE			17
#define	A_DIGEST_METHOD			18
#define	A_DIGEST_URI			19
#define	A_DIGEST_QOP			20
#define	A_DIGEST_ALGORITHM		21
#define	A_DIGEST_BODY_DIGEST		22
#define	A_DIGEST_CNONCE			23
#define	A_DIGEST_NONCE_COUNT		24
#define	A_DIGEST_USER_NAME		25
#define	A_SIP_GROUP			26
#define	A_CISCO_AVPAIR			27
#define A_VM_EMAIL                      28
#define A_VM_LANGUAGE                   29
#define	A_MAX				30

#define	V_STATUS_START			0
#define	V_STATUS_STOP			1
#define	V_STATUS_FAILED			2
#define	V_CALL_CHECK			3
#define	V_EMERGENCY_CALL		4
#define	V_SIP_SESSION			5
#define	V_GROUP_CHECK			6
#define	V_VM_INFO		        7
#define	V_MAX				8

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
			LOG(L_ERR, "ERROR: %s: can't get code for the "	\
				   "%s attribute\n", fn, at[i].n);	\
			return e1;					\
		}							\
		at[i].v = da->value;					\
	}								\
	for (i = 0; i < V_MAX; i++) {					\
		if (vl[i].n == NULL)					\
			continue;					\
		dv = rc_dict_findval(rh, vl[i].n);			\
		if (dv == NULL) {					\
			LOG(L_ERR, "ERROR: %s: can't get code for the "	\
				   "%s attribute value\n", fn, vl[i].n);\
			return e2;					\
		}							\
		vl[i].v = dv->value;					\
	}								\
}

#endif
