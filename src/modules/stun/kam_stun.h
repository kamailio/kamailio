/*
 * Portions Copyright (C) 2013 Crocodile RCS Ltd
 *
 * Based on "ser_stun.h". Copyright (C) 2001-2003 FhG Fokus
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
/*!
 * \file
 * \brief STUN :: Configuration
 * \ingroup stun
 */

#ifndef _kam_stun_h
#define _kam_stun_h

#include "../../str.h"
#include "../../tcp_conn.h"
#include "../../ip_addr.h"
#include "../../stun.h"

/* STUN message types supported by Kamailio */
#define BINDING_REQUEST		0x0001
#define BINDING_RESPONSE	0x0101
#define BINDING_ERROR_RESPONSE	0x0111

/* common STUN attributes */
#define MAPPED_ADDRESS_ATTR	0x0001
#define USERNAME_ATTR		0x0006
#define MESSAGE_INTEGRITY_ATTR	0x0008
#define ERROR_CODE_ATTR		0x0009
#define UNKNOWN_ATTRIBUTES_ATTR	0x000A

/* STUN attributes defined by rfc5389 */
#define REALM_ATTR		0x0014
#define NONCE_ATTR		0x0015
#define XOR_MAPPED_ADDRESS_ATTR	0x0020 
#define FINGERPRINT_ATTR	0x8028
#define SOFTWARE_ATTR		0x8022
#define ALTERNATE_SERVER_ATTR	0x8023

/* STUN attributes defined by rfc3489 */
#define RESPONSE_ADDRESS_ATTR	0x0002
#define CHANGE_REQUEST_ATTR	0x0003
#define SOURCE_ADDRESS_ATTR	0x0004
#define CHANGED_ADDRESS_ATTR	0x0005
#define REFLECTED_FROM_ATTR	0x000b

/* STUN error codes supported by Kamailio */
#define RESPONSE_OK		200
#define TRY_ALTERNATE_ERR	300
#define BAD_REQUEST_ERR		400
#define UNAUTHORIZED_ERR	401
#define UNKNOWN_ATTRIBUTE_ERR	420
#define STALE_CREDENTIALS_ERR	430
#define INTEGRITY_CHECK_ERR	431
#define MISSING_USERNAME_ERR	432
#define USE_TLS_ERR		433
#define MISSING_REALM_ERR	434
#define MISSING_NONCE_ERR	435
#define UNKNOWN_USERNAME_ERR	436
#define STALE_NONCE_ERR		438
#define SERVER_ERROR_ERR	500
#define GLOBAL_FAILURE_ERR	600

#define TRY_ALTERNATE_TXT      "Try Alternate"
#define BAD_REQUEST_TXT        "Bad Request"
#define UNAUTHORIZED_TXT       "Unauthorized"
#define UNKNOWN_ATTRIBUTE_TXT  "Unknown Attribute"
#define STALE_CREDENTIALS_TXT  "Stale Credentials"
#define INTEGRITY_CHECK_TXT    "Integrity Check Failure"
#define MISSING_USERNAME_TXT   "Missing Username"
#define USE_TLS_TXT            "Use TLS"
#define MISSING_REALM_TXT      "Missing Realm"
#define MISSING_NONCE_TXT      "Missing Nonce"
#define UNKNOWN_USERNAME_TXT   "Unknown Username"
#define STALE_NONCE_TXT        "Stale Nonce"
#define SERVER_ERROR_TXT       "Server Error"
#define GLOBAL_FAILURE_TXT     "Global Failure"

/* other stuff */
#define MAGIC_COOKIE_2B		0x2112	/* because of XOR for port */
#define MANDATORY_ATTR		0x7fff
#define PAD4			4
#define PAD64			64
#define STUN_MSG_LEN		516
#define IPV4_LEN		4
#define IPV6_LEN		16
#define IPV4_FAMILY		0x0001
#define IPV6_FAMILY		0x0002
#define	FATAL_ERROR		-1
#define IP_ADDR			4
#define XOR			1

#ifndef SHA_DIGEST_LENGTH
#define SHA_DIGEST_LENGTH	20
#endif

/** padd len to a multiple of sz.
 *  sz must be a power of the form 2^k (e.g. 2, 4, 8, 16 ...)
 */
#define PADD_TO(len, sz)	(((len) + (sz)-1) & (~((sz) - 1)))

#define PADDED_TO_FOUR(len)	PADD_TO(len, 4)
#define PADDED_TO_SIXTYFOUR(len) PADD_TO(len, 64)

struct stun_ip_addr {
	USHORT_T family; /* 0x01: IPv4; 0x02: IPv6 */
	USHORT_T port;
	UINT_T ip[IP_ADDR];
};

struct stun_buffer {
	str buf;
	USHORT_T empty;	/* number of free bytes in buf before it'll be necessary
			   to realloc the buf */
};

struct stun_unknown_att {
	USHORT_T type;
	struct stun_unknown_att* next;
};

struct stun_msg {
	struct stun_hdr hdr;
	struct stun_ip_addr ip_addr; /* XOR values for rfc3489bis, normal values
					for rfc3489 */
	struct stun_buffer msg;
	UCHAR_T old; /* true: the format of message is in accordance with
			rfc3489 */ 
};

int process_stun_msg(char* buf, unsigned len, struct receive_info* ri);

#endif  /* _kam_stun_h */
