/* 
 * $Id$ 
 *
 * Usrloc contact structure
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 * 2005-07-11 added FL_NAT_SIPPING for nat pinging with SIP method
 *             instead of UDP package (bogdan)
 */


#ifndef UCONTACT_H
#define UCONTACT_H


#include <stdio.h>
#include <time.h>
#include "../../qvalue.h"
#include "../../str.h"



typedef enum cstate {
	CS_NEW,        /* New contact - not flushed yet */
	CS_SYNC,       /* Synchronized contact with the database */
	CS_DIRTY       /* Update contact - not flushed yet */
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
	FL_PERMANENT   = 1 << 7,     /* Permanent contact (does not expire) */
	FL_MEM         = 1 << 8,     /* Update memory only -- used for REGISTER replication */
	FL_NAT_SIPPING = 1 << 9,     /* Use SIP ping if nated */
	FL_ALL         = 0xFFFFFFFF  /* All flags set */
} flags_t;


typedef struct ucontact {
	str* domain;            /* Pointer to domain name (NULL terminated) */
	str* aor;               /* Pointer to the AOR string in record structure*/
	str c;                  /* Contact address */
	str received;           /* IP+port+protocol we recved the REGISTER from */
	str path;               /* Path header */
	time_t expires;         /* expires parameter */
	qvalue_t q;             /* q parameter */
	str callid;             /* Call-ID header field */
	int cseq;               /* CSeq value */
	cstate_t state;         /* State of the contact */
	unsigned int flags;     /* Various flags (NAT, supported methods etc) */
	str user_agent;         /* User-Agent header field */
	struct socket_info *sock; /* received soket */
	time_t last_modified;   /* when the record was last modified */
	unsigned int methods;   /* Supported methods */
	struct ucontact* next;  /* Next contact in the linked list */
	struct ucontact* prev;  /* Previous contact in the linked list */
} ucontact_t;

typedef struct ucontact_info {
	str received;
	str* path;
	time_t expires;
	qvalue_t q;
	str* callid;
	int cseq;
	unsigned int flags1;
	unsigned int flags2;
	str *user_agent;
	struct socket_info *sock;
	unsigned int methods;
	time_t last_modified;
} ucontact_info_t;

/*
 * Valid contact is a contact that either didn't expire yet or is permanent
 */
#define VALID_CONTACT(c, t) \
	(((c->expires > t) || (c->flags & FL_PERMANENT)))


/*
 * Create a new contact structure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact,
		ucontact_info_t* _ci);


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
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t *_ci);


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
 * deleted from memory immediately,
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
 * Update ucontact with new values
 */
typedef int (*update_ucontact_t)(ucontact_t* _c, ucontact_info_t* _ci);
int update_ucontact(ucontact_t* _c, ucontact_info_t* _ci);

#endif /* UCONTACT_H */
