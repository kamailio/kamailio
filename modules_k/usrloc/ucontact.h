/* 
 * $Id$ 
 *
 * Usrloc contact structure
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 * 2003-03-12 added replication mark and three zombie states (nils)
 * 2005-07-11 added FL_NAT_SIPPING for nat pinging with SIP method
 *             instead of UDP package (bogdan)
 */

/*! \file
 *  \brief USRLOC - Usrloc contact structure
 *  \ingroup usrloc
 */


#ifndef UCONTACT_H
#define UCONTACT_H


#include <stdio.h>
#include <time.h>
#include "../../qvalue.h"
#include "../../str.h"



/*! \brief States for in-memory contacts in regards to contact storage handler (db, in-memory, ldap etc) */
typedef enum cstate {
	CS_NEW,        /*!< New contact - not flushed yet */
	CS_SYNC,       /*!< Synchronized contact with the database */
	CS_DIRTY       /*!< Update contact - not flushed yet */
} cstate_t;


/*! \brief
 * Flags that can be associated with a Contact
 */
typedef enum flags {
	FL_NONE        = 0,          /*!< No flags set */
	FL_MEM         = 1 << 0,     /*!< Update memory only */
	FL_ALL         = (int)0xFFFFFFFF  /*!< All flags set */
} flags_t;


/*! \brief
 * Main structure for handling of registered Contact: data 
 */
typedef struct ucontact {
	str* domain;            /*!< Pointer to domain name (NULL terminated) */
	str* aor;               /*!< Pointer to the AOR string in record structure*/
	str c;                  /*!< Contact address */
	str received;           /*!< IP+port+protocol we received the REGISTER from */
	str path;               /*!< Path header */
	time_t expires;         /*!< Expires parameter */
	qvalue_t q;             /*!< q parameter */
	str callid;             /*!< Call-ID header field of registration */
	int cseq;               /*!< CSeq value */
	cstate_t state;         /*!< State of the contact (\ref cstate) */
	unsigned int flags;     /*!< Various flags (NAT, ping type, etc) */
	unsigned int cflags;    /*!< Custom contact flags (from script) */
	str user_agent;         /*!< User-Agent header field */
	struct socket_info *sock; /*!< received socket */
	time_t last_modified;   /*!< When the record was last modified */
	unsigned int methods;   /*!< Supported methods */
	struct ucontact* next;  /*!< Next contact in the linked list */
	struct ucontact* prev;  /*!< Previous contact in the linked list */
} ucontact_t;

typedef struct ucontact_info {
	str received;
	str* path;
	time_t expires;
	qvalue_t q;
	str* callid;
	int cseq;
	unsigned int flags;
	unsigned int cflags;
	str *user_agent;
	struct socket_info *sock;
	unsigned int methods;
	time_t last_modified;
} ucontact_info_t;

/*! \brief
 * ancient time used for marking the contacts forced to expired
 */
#define UL_EXPIRED_TIME 10

/*
 * Valid contact is a contact that either didn't expire yet or is permanent
 */
#define VALID_CONTACT(c, t)   ((c->expires>t) || (c->expires==0))


/*! \brief
 * Create a new contact structure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact,
		ucontact_info_t* _ci);


/*! \brief
 * Free all memory associated with given contact structure
 */
void free_ucontact(ucontact_t* _c);


/*! \brief
 * Print contact, for debugging purposes only
 */
void print_ucontact(FILE* _f, ucontact_t* _c);


/*! \brief
 * Update existing contact in memory with new values
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t *_ci);


/* ===== State transition functions - for write back cache scheme ======== */


/*! \brief
 * Update state of the contact if we
 * are using write-back scheme
 */
void st_update_ucontact(ucontact_t* _c);


/*! \brief
 * Update state of the contact if we
 * are using write-back scheme
 * Returns 1 if the contact should be
 * deleted from memory immediately,
 * 0 otherwise
 */
int st_delete_ucontact(ucontact_t* _c);


/*! \brief
 * Called when the timer is about to delete
 * an expired contact, this routine returns
 * 1 if the contact should be removed from
 * the database and 0 otherwise
 */
int st_expired_ucontact(ucontact_t* _c);


/*! \brief
 * Called when the timer is about flushing the contact,
 * updates contact state and returns 1 if the contact
 * should be inserted, 2 if updated and 0 otherwise
 */
int st_flush_ucontact(ucontact_t* _c);


/* ==== Database related functions ====== */


/*! \brief
 * Insert contact into the database
 */
int db_insert_ucontact(ucontact_t* _c);


/*! \brief
 * Update contact in the database
 */
int db_update_ucontact(ucontact_t* _c);


/*! \brief
 * Delete contact from the database
 */
int db_delete_ucontact(ucontact_t* _c);


/* ====== Module interface ====== */

struct urecord;

/*! \brief
 * Update ucontact with new values
 */
typedef int (*update_ucontact_t)(struct urecord* _r, ucontact_t* _c,
		ucontact_info_t* _ci);

int update_ucontact(struct urecord* _r, ucontact_t* _c, ucontact_info_t* _ci);

#endif /* UCONTACT_H */
