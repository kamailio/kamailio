/*
 * Digest Authentication Module
 * Nonce related functions
 * 
 * Copyright (C) 2001-2003 FhG Fokus
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

#ifndef NONCE_H
#define NONCE_H

#include "../../parser/msg_parser.h"
#include "../../parser/digest/digest.h"
#include "../../str.h"
#include "../../basex.h"
#include <time.h>


/* auth_extra_checks flags */

#define AUTH_CHECK_FULL_URI (1 << 0)
#define AUTH_CHECK_CALLID   (1 << 1)
#define AUTH_CHECK_FROMTAG  (1 << 2)
#define AUTH_CHECK_SRC_IP   (1 << 3)
/* nonce format:
 * base64(bin_nonce)
 * bin_nonce =  expire_timestamp(4) | since_timestamp(4) | \
 *   MD5(expire | since | secret1) (16)  \
 *   [|   MD5(info(auth_extra_checks) | secret2) (16) ]
 * if nonce-count or one-time nonces are enabled, the format changes to:
 *  bin_nonce = 
 * bin_nonce =  expire_timestamp(4) | since_timestamp(4) | 
 *  MD5(expire | since | nid | pf | secret1) [ | MD5... ] | nid(4) | pf(1) 
 * where pf is 1 byte, first 2 bits are flags, and the other 6 are 
 * the pool no:
 * bit7 : on => nid & pool are valid for nonce-count
 * bit6 : on => nid & pool are valid for one-time nonce
 */
#if defined USE_NC || defined USE_OT_NONCE
#define NF_VALID_NC_ID 128 
#define NF_VALID_OT_ID  64 

#define NF_POOL_NO_MASK  63
#endif

#if defined USE_NC || defined USE_OT_NONCE
#define nonce_nid_extra_size (sizeof(unsigned int)+sizeof(unsigned char))

#else /* USE_NC || USE_OT_NONCE*/

#define nonce_nid_extra_size 0
#endif /* USE_NC || USE_OT_NONCE */

/* nonce structure, complete (maximum size) */
struct bin_nonce_str{
	int expire;
	int since;
	char md5_1[16];
	char md5_2[16]; /* optional */
#if defined USE_NC || defined USE_OT_NONCE
	unsigned int nid_i;
	unsigned char nid_pf; /* pool no & flags:
						 	  bits 7, 6 = flags, bits 5..0 pool no*/ 
#endif /* USE_NC || USE_OT_NONCE */
};

/* nonce structure, small version  (no auth_extra_checks secondary md5) */
struct bin_nonce_small_str{
	int expire;
	int since;
	char md5_1[16];
#if defined USE_NC || defined USE_OT_NONCE
	unsigned int nid_i;
	unsigned char nid_pf; /* pool no & flags:
							  bits 7, 6 = flags, bits 5..0 pool no*/ 
#endif /* USE_NC || USE_OT_NONCE */
};

/* nonce union */
union bin_nonce{
	struct bin_nonce_str n;
	struct bin_nonce_small_str n_small;
	unsigned char raw[sizeof(struct bin_nonce_str)];
};


/* fill an union bin_nonce*, before computing the md5 */
#define BIN_NONCE_PREPARE_COMMON(bn, expire_val, since_val) \
	do{\
		(bn)->n.expire=htonl(expire_val); \
		(bn)->n.since=htonl(since_val); \
	}while(0)

#if defined USE_NC || defined USE_OT_NONCE
#define BIN_NONCE_PREPARE(bn, expire_v, since_v, id_v, pf_v, cfg, msg)  \
	do{ \
		BIN_NONCE_PREPARE_COMMON(bn, expire_v, since_v); \
		if (cfg && msg){ \
			(bn)->n.nid_i=htonl(id_v); \
			(bn)->n.nid_pf=(pf_v); \
		}else{ \
			(bn)->n_small.nid_i=htonl(id_v); \
			(bn)->n_small.nid_pf=(pf_v); \
		} \
	}while(0)
#else /* USE_NC || USE_OT_NONCE */
#define BIN_NONCE_PREPARE(bn, expire, since, id, pf, cfg, msg)  \
	BIN_NONCE_PREPARE_COMMON(bn, expire, since)
#endif /* USE_NC || USE_OT_NONCE */



/* maximum nonce length in binary form (not converted to base64/hex):
 * expires_t | since_t | MD5(expires_t | since_t | s1) | \
 *   MD5(info(auth_extra_checks, s2))   => 4  + 4 + 16 + 16 = 40 bytes
 * or if nc_enabled:
 * expires_t | since_t | MD5...| MD5... | nonce_id | flag+pool_no(1 byte)
 * => 4 + 4 + 16 + 16 + 4 + 1 = 45 bytes
 * (sizeof(struct) cannot be used safely since structs can be padded
 *  by the compiler if not defined with special attrs)
 */
#if defined USE_NC || defined USE_OT_NONCE
#define MAX_BIN_NONCE_LEN (4 + 4 + 16 + 16 + 4 +1)
#define MAX_NOCFG_BIN_NONCE_LEN (4 + 4 + 16 + 4 + 1)

#define get_bin_nonce_len(cfg, nid_enabled) \
	( ( (cfg)?MAX_BIN_NONCE_LEN:MAX_NOCFG_BIN_NONCE_LEN ) - \
		(!(nid_enabled))*nonce_nid_extra_size )

#else /* USE_NC || USE_OT_NONCE */
#define MAX_BIN_NONCE_LEN (4 + 4 + 16 + 16)
#define MAX_NOCFG_BIN_NONCE_LEN (4 + 4 + 16)

#define get_bin_nonce_len(cfg, nid_enabled) \
		( (cfg)?MAX_BIN_NONCE_LEN:MAX_NOCFG_BIN_NONCE_LEN )

#endif /* USE_NC || USE_OT_NONCE */

/* minimum nonce length in binary form (not converted to base64/hex):
 * expires_t | since_t | MD5(expires_t | since_t | s1) => 4 + 4 + 16 = 24 
 * If nc_enabled the nonce will be bigger:
 * expires_t | since_t | MD5... | nonce_id | flag+pool_no(1 byte) 
 * => 4 + 4 + 16 + 4 + 1 = 29, but we always return the minimum */
#define MIN_BIN_NONCE_LEN (4 + 4 + 16)


/*
 * Maximum length of nonce string in bytes
 * nonce = expires_TIMESTAMP[4 chars] since_TIMESTAMP[4 chars] \
 * MD5SUM(expires_TIMESTAMP, since_TIMESTAMP, SECRET1)[16 chars] \
 * MD5SUM(info(auth_extra_checks), SECRET2)[16 chars] \
 * [nid [4 chars]  pflags[1 char]]
 */
#define MAX_NONCE_LEN  base64_enc_len(MAX_BIN_NONCE_LEN)
/*
 * Minimum length of the nonce string
 * nonce = expires_TIMESTAMP[4 chars] since_TIMESTAMP[4 chars] 
 * MD5SUM(expires_TIMESTAMP, since_TIMESTAMP, SECRET1)[16 chars]
 */
#define MIN_NONCE_LEN base64_enc_len(MIN_BIN_NONCE_LEN)

/*
 * length of nonces when no auth extra checks (cfg==0) are enabled
 */
#define MAX_NOCFG_NONCE_LEN base64_enc_len(MAX_NOCFG_BIN_NONCE_LEN)


/* Extra authentication checks for REGISTER messages */
extern int auth_checks_reg;
/* Extra authentication checks for out-of-dialog requests */
extern int auth_checks_ood;
/* Extra authentication checks for in-dialog requests */
extern int auth_checks_ind;

/* maximum time drift accepted for the nonce creation time
 * (e.g. nonce generated by another proxy in the same cluster with the
 * clock slightly in the future)
 */
extern unsigned int  nonce_auth_max_drift;


int get_auth_checks(struct sip_msg* msg);


/*
 * get the configured nonce len
 */
#define get_nonce_len(cfg, nid_enabled) \
		base64_enc_len(get_bin_nonce_len(cfg, nid_enabled))


/*
 * Calculate nonce value
 */
int calc_nonce(char* nonce, int* nonce_len, int cfg, int since, int expires,
#if defined USE_NC || defined USE_OT_NONCE
				unsigned int n_id, unsigned char pf,
#endif /* USE_NC || USE_OT_NONCE */
				str* secret1, str* secret2, struct sip_msg* msg);


/*
 * Check nonce value received from UA
 */
int check_nonce(auth_body_t* auth, str* secret1, str* secret2,
					struct sip_msg* msg);



#endif /* NONCE_H */
