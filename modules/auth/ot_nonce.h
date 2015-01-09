/*
 * Digest Authentication Module
 * 
 * one-time nonce support
 *
 * Copyright (C) 2008 iptelorg GmbH
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
/*
 * Defines: 
 *  USE_OT_NONCE   - if not defined no one-time nonce specific code will be
 *                    compiled
 *  OTN_CELL_T_LONG - uses unsigned long instead os unsigned int for the
 *                    array cells
 */


#ifndef _ot_nonce_h
#define _ot_nonce_h

extern int otn_enabled;

/* instead of storing only the 2^k size we store also k
 * for faster operations */
extern unsigned otn_in_flight_k;    /* maximum in-flight nonces (k in 2^k) */
extern unsigned otn_in_flight_no   ; /* 2^k == 1<<otn_in_flight_no */

#ifdef USE_OT_NONCE

#include "nid.h" /* nid_t */
#include "../../atomic_ops.h"


/* default number of maximum in-flight nonces */
#define DEFAULT_OTN_IN_FLIGHT (1024*1024U) /*  1M nonces => 128k mem. */
#define MIN_OTN_IN_FLIGHT      (128*1024U)  /*  warn if < then 128k nonces */

#define MAX_OTN_IN_FLIGHT    (2*1024*1024*1024U) /* warn if size > 250Mb */

#define MIN_OTN_PARTITION   65536U /* warn if < 65k nonces per partition*/

#ifdef OTN_CELL_T_LONG
typedef unsigned long otn_cell_t;
#else
typedef unsigned int otn_cell_t;
#endif

int init_ot_nonce();
void destroy_ot_nonce();


enum otn_check_ret{ 
	OTN_OK=0, OTN_INV_POOL=-1, OTN_ID_OVERFLOW=-2, OTN_REPLAY=-3 
};

/* check if nonce w/ index i is valid & expected and record receiving it */
enum otn_check_ret otn_check_id(nid_t i, unsigned pool);

/* re-init the stored nonce state for nonce id in pool pool_no */
nid_t otn_new(nid_t id, unsigned char pool_no);

#endif /* USE_OT_NONCE */
#endif /* _ot_nonce_h */

