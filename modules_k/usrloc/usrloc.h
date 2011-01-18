/*
 * $Id$
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
 */

/*! \file
 *  \brief USRLOC - module API exports interface
 *  \ingroup usrloc
 */

#ifndef USRLOC_H
#define USRLOC_H

#include <time.h>
#include "ul_callback.h"
#include "../../qvalue.h"
#include "../../str.h"

#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2
#define DB_ONLY       3

/*forward declaration necessary for udomain*/

struct udomain;
typedef struct udomain udomain_t;

/*!
 * \brief States for in-memory contacts in regards to contact storage handler (db, in-memory, ldap etc)
 */
typedef enum cstate {
	CS_NEW,        /*!< New contact - not flushed yet */
	CS_SYNC,       /*!< Synchronized contact with the database */
	CS_DIRTY       /*!< Update contact - not flushed yet */
} cstate_t;


/*! \brief Flags that can be associated with a Contact */
typedef enum flags {
	FL_NONE        = 0,          /*!< No flags set */
	FL_MEM         = 1 << 0,     /*!< Update memory only */
	FL_ALL         = (int)0xFFFFFFFF  /*!< All flags set */
} flags_t;

/*! \brief Valid contact is a contact that either didn't expire yet or is permanent */
#define VALID_CONTACT(c, t)   ((c->expires>t) || (c->expires==0))

struct hslot; /*!< Hash table slot */
struct socket_info;
/*! \brief Main structure for handling of registered Contact data */
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


/*! \brief Informations related to a contact */
typedef struct ucontact_info {
	str received;             /*!< Received interface */
	str* path;                /*!< Path informations */
	time_t expires;           /*!< Contact expires */
	qvalue_t q;               /*!< Q-value */
	str* callid;              /*!< call-ID */
	int cseq;                 /*!< CSEQ number */
	unsigned int flags;       /*!< message flags */
	unsigned int cflags;      /*!< contact flags */
	str *user_agent;          /*!< user agent header */
	struct socket_info *sock; /*!< socket informations */
	unsigned int methods;     /*!< supported methods */
	time_t last_modified;     /*!< last modified */
} ucontact_info_t;

typedef struct udomain_head{
    str* name;
} udomain_head_t;

/*! \brief
 * Basic hash table element
 */
typedef struct urecord {
	str* domain;                   /*!< Pointer to domain we belong to
                                    * ( null terminated string) */
	str aor;                       /*!< Address of record */
	unsigned int aorhash;          /*!< Hash over address of record */
	ucontact_t* contacts;          /*!< One or more contact fields */

	struct hslot* slot;            /*!< Collision slot in the hash table
                                    * array we belong to */
	struct urecord* prev;          /*!< Next item in the hash entry */
	struct urecord* next;          /*!< Previous item in the hash entry */
} urecord_t;

/*!
 * \brief Create and insert a new record
 * \param _d domain to insert the new record
 * \param _aor address of the record
 * \param _r new created record
 * \return return 0 on success, -1 on failure
 */
typedef int (*insert_urecord_t)(struct udomain* _d, str* _aor, struct urecord** _r);
int insert_urecord(struct udomain* _d, str* _aor, struct urecord** _r);


/*!
 * \brief Obtain a urecord pointer if the urecord exists in domain
 * \param _d domain to search the record
 * \param _aor address of record
 * \param _r new created record
 * \return 0 if a record was found, 1 if nothing could be found
 */
typedef int  (*get_urecord_t)(struct udomain* _d, str* _aor, struct urecord** _r);
int get_urecord(struct udomain* _d, str* _aor, struct urecord** _r);


/*!
 * \brief Delete a urecord from domain
 * \param _d domain where the record should be deleted
 * \param _aor address of record
 * \param _r deleted record
 * \return 0 on success, -1 if the record could not be deleted
 */
typedef int  (*delete_urecord_t)(struct udomain* _d, str* _aor, struct urecord* _r);
int delete_urecord(struct udomain* _d, str* _aor, struct urecord* _r);


/*!
 * \brief Update ucontact with new values
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \param _ci new contact informations
 * \return 0 on success, -1 on failure
 */
typedef int (*update_ucontact_t)(struct urecord* _r, struct ucontact* _c,
		struct ucontact_info* _ci);
int update_ucontact(struct urecord* _r, struct ucontact* _c, struct ucontact_info* _ci);

/*!
 * \brief Release urecord previously obtained through get_urecord
 * \warning Failing to calls this function after get_urecord will
 * result in a memory leak when the DB_ONLY mode is used. When
 * the records is later deleted, e.g. with delete_urecord, then
 * its not necessary, as this function already releases the record.
 * \param _r released record
 */
typedef void (*release_urecord_t)(struct urecord* _r);
void release_urecord(struct urecord* _r);


/*!
 * \brief Create and insert new contact into urecord
 * \param _r record into the new contact should be inserted
 * \param _contact contact string
 * \param _ci contact information
 * \param _c new created contact
 * \return 0 on success, -1 on failure
 */
typedef int (*insert_ucontact_t)(struct urecord* _r, str* _contact,
		struct ucontact_info* _ci, struct ucontact** _c);
int insert_ucontact(struct urecord* _r, str* _contact,
		struct ucontact_info* _ci, struct ucontact** _c);


/*!
 * \brief Delete ucontact from urecord
 * \param _r record where the contact belongs to
 * \param _c deleted contact
 * \return 0 on success, -1 on failure
 */
typedef int (*delete_ucontact_t)(struct urecord* _r, struct ucontact* _c);
int delete_ucontact(struct urecord* _r, struct ucontact* _c);


/*!
 * \brief Get pointer to ucontact with given contact
 * \param _r record where to search the contacts
 * \param _c contact string
 * \param _callid callid
 * \param _path path
 * \param _cseq CSEQ number
 * \param _co found contact
 * \return 0 - found, 1 - not found, -1 - invalid found,
 * -2 - found, but to be skipped (same cseq)
 */
typedef int (*get_ucontact_t)(struct urecord* _r, str* _c, str* _callid,
		str* _path, int _cseq,
		struct ucontact** _co);
int get_ucontact(struct urecord* _r, str* _c, str* _callid, str* _path,
		int _cseq,
		struct ucontact** _co);

/*! \brief
 * Timer handler for given domain
 */
typedef void (*lock_udomain_t)(struct udomain* _d, str *_aor);
void lock_udomain(struct udomain* _d, str *_aor);


/*!
 * \brief Release lock for a domain
 * \param _d domain
 * \param _aor address of record, uses as hash source for the lock slot
 */
typedef void (*unlock_udomain_t)(struct udomain* _d, str *_aor);
void unlock_udomain(struct udomain* _d, str *_aor);

/*!
 * \brief Registers a new domain with usrloc
 *
 * Registers a new domain with usrloc. If the domain exists,
 * a pointer to existing structure will be returned, otherwise
 * a new domain will be created
 * \param _n domain name
 * \param _d new created domain
 * \return 0 on success, -1 on failure
 */
typedef int (*register_udomain_t)(const char* _n, struct udomain** _d);
int register_udomain(const char* _n, struct udomain** _d);

/*!
 * \brief Get all contacts from the usrloc, in partitions if wanted
 *
 * Return list of all contacts for all currently registered
 * users in all domains. The caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +------------+----------+-----+------+-----+
 * |contact1.len|contact1.s|sock1|flags1|path1|
 * +------------+----------+-----+------+-----+
 * |contact2.len|contact2.s|sock2|flags2|path1|
 * +------------+----------+-----+------+-----+
 * |..........................................|
 * +------------+----------+-----+------+-----+
 * |contactN.len|contactN.s|sockN|flagsN|pathN|
 * +------------+----------+-----+------+-----+
 * |000000000000|
 * +------------+
 *
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
typedef int  (*get_all_ucontacts_t) (void* buf, int len, unsigned int flags,
		unsigned int part_idx, unsigned int part_max);
int get_all_ucontacts(void *, int, unsigned int,
		unsigned int part_idx, unsigned int part_max);

/*! usrloc API export structure */
typedef struct usrloc_api {
	int           use_domain; /*! use_domain module parameter */
	int           db_mode;    /*! db_mode module parameter */
	unsigned int  nat_flag;   /*! nat_flag module parameter */

	register_udomain_t   register_udomain;
	get_udomain_t        get_udomain;
	get_all_ucontacts_t  get_all_ucontacts;

	insert_urecord_t     insert_urecord;
	delete_urecord_t     delete_urecord;
	get_urecord_t        get_urecord;
	lock_udomain_t       lock_udomain;
	unlock_udomain_t     unlock_udomain;

	release_urecord_t    release_urecord;
	insert_ucontact_t    insert_ucontact;
	delete_ucontact_t    delete_ucontact;
	get_ucontact_t       get_ucontact;

	update_ucontact_t    update_ucontact;

	register_ulcb_t      register_ulcb;
} usrloc_api_t;


/*! usrloc API export bind function */
typedef int (*bind_usrloc_t)(usrloc_api_t* api);

#endif
