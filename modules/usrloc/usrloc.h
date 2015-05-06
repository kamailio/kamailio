/*
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
#ifdef WITH_XAVP
#include "../../xavp.h"
#endif

#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2
#define DB_ONLY       3
#define DB_READONLY   4

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
	FL_RPL         = 1 << 1,     /*!< DMQ replication */
	FL_ALL         = (int)0xFFFFFFFF  /*!< All flags set */
} flags_t;

/*! \brief Valid contact is a contact that either didn't expire yet or is permanent */
#define VALID_CONTACT(c, t)   ((c->expires>t) || (c->expires==0))

struct hslot; /*!< Hash table slot */
struct socket_info;
/*! \brief Main structure for handling of registered Contact data */
typedef struct ucontact {
	str* domain;            /*!< Pointer to domain name (NULL terminated) */
	str ruid;               /*!< Pointer to record internal unique id */
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
	time_t last_keepalive;  /*!< last keepalive timestamp */
	unsigned int methods;   /*!< Supported methods */
	str instance;           /*!< SIP instance value - gruu */
	unsigned int reg_id;    /*!< reg-id parameters */
	int server_id;          /*!< server id */
	int tcpconn_id;         /*!< unique tcp connection id */
	int keepalive;          /*!< keepalive */
#ifdef WITH_XAVP
	sr_xavp_t * xavp;       /*!< per contact xavps */
#endif
	struct ucontact* next;  /*!< Next contact in the linked list */
	struct ucontact* prev;  /*!< Previous contact in the linked list */
} ucontact_t;


/*! \brief Informations related to a contact */
typedef struct ucontact_info {
	str ruid;                 /*!< Pointer to record internal unique id */
	str *c;                   /*!< Contact address */
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
	str instance;             /*!< SIP instance value - gruu */
	unsigned int reg_id;      /*!< reg-id parameters */
	int server_id;            /*!< server id */
	int tcpconn_id;           /*!< connection id */
	int keepalive;            /*!< keepalive */
#ifdef WITH_XAVP
	sr_xavp_t * xavp;         /*!< per contact xavps */
#endif
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

typedef int (*insert_urecord_t)(struct udomain* _d, str* _aor, struct urecord** _r);

typedef int (*get_urecord_t)(struct udomain* _d, str* _aor, struct urecord** _r);

typedef int (*get_urecord_by_ruid_t)(udomain_t* _d, unsigned int _aorhash,
		str *_ruid, struct urecord** _r, struct ucontact** _c);

typedef int  (*delete_urecord_t)(struct udomain* _d, str* _aor, struct urecord* _r);

typedef int  (*delete_urecord_by_ruid_t)(struct udomain* _d, str* _ruid);

typedef int (*update_ucontact_t)(struct urecord* _r, struct ucontact* _c,
		struct ucontact_info* _ci);
typedef void (*release_urecord_t)(struct urecord* _r);

typedef int (*insert_ucontact_t)(struct urecord* _r, str* _contact,
		struct ucontact_info* _ci, struct ucontact** _c);

typedef int (*delete_ucontact_t)(struct urecord* _r, struct ucontact* _c);

typedef int (*get_ucontact_t)(struct urecord* _r, str* _c, str* _callid,
		str* _path, int _cseq,
		struct ucontact** _co);

typedef int (*get_ucontact_by_instance_t)(struct urecord* _r, str* _c,
		ucontact_info_t* _ci, ucontact_t** _co);

typedef void (*lock_udomain_t)(struct udomain* _d, str *_aor);

typedef void (*unlock_udomain_t)(struct udomain* _d, str *_aor);

typedef int (*register_udomain_t)(const char* _n, struct udomain** _d);

typedef int  (*get_all_ucontacts_t) (void* buf, int len, unsigned int flags,
		unsigned int part_idx, unsigned int part_max);

typedef int (*get_udomain_t)(const char* _n, udomain_t** _d);

typedef unsigned int (*ul_get_aorhash_t)(str *_aor);
unsigned int ul_get_aorhash(str *_aor);

typedef int (*ul_set_keepalive_timeout_t)(int _to);
int ul_set_keepalive_timeout(int _to);

typedef int (*ul_refresh_keepalive_t)(unsigned int _aorhash, str *_ruid);
int ul_refresh_keepalive(unsigned int _aorhash, str *_ruid);


typedef void (*ul_set_max_partition_t)(unsigned int m);

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
	delete_urecord_by_ruid_t     delete_urecord_by_ruid;
	get_urecord_t        get_urecord;
	lock_udomain_t       lock_udomain;
	unlock_udomain_t     unlock_udomain;

	release_urecord_t    release_urecord;
	insert_ucontact_t    insert_ucontact;
	delete_ucontact_t    delete_ucontact;
	get_ucontact_t       get_ucontact;

	get_urecord_by_ruid_t       get_urecord_by_ruid;
	get_ucontact_by_instance_t  get_ucontact_by_instance;

	update_ucontact_t    update_ucontact;

	register_ulcb_t      register_ulcb;
	ul_get_aorhash_t     get_aorhash;

	ul_set_keepalive_timeout_t set_keepalive_timeout;
	ul_refresh_keepalive_t     refresh_keepalive;
	ul_set_max_partition_t     set_max_partition;
} usrloc_api_t;


/*! usrloc API export bind function */
typedef int (*bind_usrloc_t)(usrloc_api_t* api);

#endif
