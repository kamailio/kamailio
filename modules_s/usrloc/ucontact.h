/* 
 * $Id$ 
 *
 * Usrloc contact structure
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * ---------
 * 2003-03-12 added replication mark and three zombie states (nils)
 */


#ifndef UCONTACT_H
#define UCONTACT_H


#include <stdio.h>
#include <time.h>
#include "../../str.h"

typedef enum cstate {
	CS_NEW,        /* New contact - not flushed yet */
	CS_SYNC,       /* Synchronized contact with the database */
	CS_DIRTY,      /* Update contact - not flushed yet */
	/* Attention the sequence of this entries is used in conditions.
	 * Changes here can break things! */
	CS_ZOMBIE_N,   /* Removed contact - not flushed yet */
	CS_ZOMBIE_S,   /* Removed contact - synchronized with db */
	CS_ZOMBIE_D    /* Removed contact - updated and not flushed yet */
} cstate_t;


/*
 * Flags that can be associated with a Contact
 */
typedef enum flags {
	FL_NONE        = 0,          /* No flags set */
	FL_NAT         = 1 << 0,     /* Contact is behind NAT */
	FL_INVITE      = 1 << 1,     /* Contact supports INVITE and related methods */
	FL_N_INVITE    = 1 << 2,     /* Contact doesn't support INVITE and related methods */
	FL_MESSAGE     = 1 << 3,     /* Contact supports MESSAGE */
	FL_N_MESSAGE   = 1 << 4,     /* Contact doesn't support MESSAGE */
	FL_SUBSCRIBE   = 1 << 5,     /* Contact supports SUBSCRIBE and NOTIFY */
	FL_N_SUBSCRIBE = 1 << 6,     /* Contact doesn't support SUBSCRIBE and NOTIFY */
	FL_ALL         = 0xFFFFFFFF  /* All flags set */
} flags_t;


typedef struct ucontact {
	str* domain;            /* Pointer to domain name */
	str* aor;               /* Pointer to the address of record string in record structure*/
	str c;                  /* Contact address */
	time_t expires;         /* expires parameter */
	float q;                /* q parameter */
	str callid;             /* Call-ID header field */
        int cseq;               /* CSeq value */
	unsigned int replicate; /* replication marker */
	cstate_t state;         /* State of the contact */
	unsigned int flags;     /* Various flags (NAT, supported methods etc) */
	struct ucontact* next;  /* Next contact in the linked list */
	struct ucontact* prev;  /* Previous contact in the linked list */
} ucontact_t;


/*
 * Create a new contact structure
 */
int new_ucontact(str* _dom, str* _aor, str* _contact, time_t _e, float _q, 
		 str* _callid, int _cseq, unsigned int _flags, int _rep, ucontact_t** _c);


/*
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c);


/*
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c);


/*
 * Update existing contact in memory with new values
 */
int mem_update_ucontact(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs,
			unsigned int _set, unsigned int _res);


/* ===== State transition functions - for write back cache scheme ======== */


/*
 * Update state of the contact if we
 * are using write-back scheme
 */
void st_update_ucontact(ucontact_t* _c);


/*
 * Update state of the contact if we
 * are using write-back scheme
 * Returns 1 if the contact should be
 * deleted from memory immediatelly,
 * 0 otherwise
 */
int st_delete_ucontact(ucontact_t* _c);


/*
 * Called when the timer is about to delete
 * an expired contact, this routine returns
 * 1 if the contact should be removed from
 * the database and 0 otherwise
 */
int st_expired_ucontact(ucontact_t* _c);


/*
 * Called when the timer is about flushing the contact,
 * updates contact state and returns 1 if the contact
 * should be inserted, 2 if updated and 0 otherwise
 */
int st_flush_ucontact(ucontact_t* _c);


/* ==== Database related functions ====== */


/*
 * Insert contact into the database
 */
int db_insert_ucontact(ucontact_t* _c);


/*
 * Update contact in the database
 */
int db_update_ucontact(ucontact_t* _c);


/*
 * Delete contact from the database
 */
int db_delete_ucontact(ucontact_t* _c);


/* ====== Module interface ====== */


/*
 * Update ucontact with new values without replication
 */
int update_ucontact(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs,
		    unsigned int _set, unsigned int _res);

/*
 * Update ucontact with new values with addtional replication argument
 */
int update_ucontact_rep(ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs, int _rep,
			unsigned int _set, unsigned int _res);


#endif /* UCONTACT_H */
