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
 *
 */

#ifndef _SER_USR_AVP_H_
#define _SER_USR_AVP_H_

#include <sys/types.h>
#include <regex.h>


/*
 *   LIST with the allocated flags, their meaning and owner
 *   flag no.    owner            description
 *   -------------------------------------------------------
 *     0        avp_core          avp has a string name
 *     1        avp_core          avp has a string value
 *     2        avp_core          regex search in progress
 *     3        avpops module     avp was loaded from DB
 *     4        lcr module        contact avp qvalue change
 *     5        core              avp is in user list
 *     6        core              avp is in domain list
 *     7        core              avp is in global list
 *     8        core              avp is in the from avp list
 *     9        core              avp is in the to avp list
 *    10	core		  avp name with positive index
 *    11	core		  avp name with negative index
 */

#include "str.h"


#define AVP_UID          "uid"           /* Unique user identifier */
#define AVP_DID          "did"           /* Unique domain identifier */
#define AVP_REALM        "digest_realm"  /* Digest realm */
#define AVP_FR_TIMER     "fr_timer"      /* Value of final response timer */
#define AVP_FR_INV_TIMER "fr_inv_timer"  /* Value of final response invite timer */
#define AVP_RPID         "rpid"          /* Remote-Party-ID */
#define AVP_GFLAGS       "gflags"        /* global flags */

struct str_int_data {
	str name;
	int val;
};

struct str_str_data {
	str name;
	str val;
};

typedef union {
	int  n;
	str  s;
	regex_t* re;
} int_str;

#define avp_id_t	unsigned short
#define avp_flags_t	unsigned int
#define avp_name_t	int_str
#define avp_value_t	int_str
#define avp_index_t	unsigned short

union usr_avp_data{
	void *p; /* forces alignment */
	long l;
	char data[sizeof(void*)]; /* used to access other types, var length */
};

typedef struct usr_avp {
	avp_id_t id;
	/* Flags that are kept for the AVP lifetime */
	avp_flags_t flags;
	struct usr_avp *next;
	union usr_avp_data d; /* var length */
} avp_t;

typedef avp_t* avp_list_t;

/* AVP identification */
typedef struct avp_ident {
	avp_flags_t flags;
	avp_name_t name;
	avp_index_t index;
} avp_ident_t;

/*
 * AVP search state
 */
typedef struct search_state {
	avp_flags_t flags;  /* Type of search and additional flags */
	avp_id_t id;
	avp_name_t name;
	avp_t* avp;            /* Current AVP */
//	regex_t* search_re;    /* Compiled regular expression */
} avp_search_state_t;

/* avp aliases structs*/
typedef struct avp_spec {
	avp_flags_t type;
	avp_name_t name;
	avp_index_t index;
} avp_spec_t;

/* AVP types */
#define AVP_NAME_STR     (1<<0)
#define AVP_VAL_STR      (1<<1)
#define AVP_NAME_RE      (1<<2)

/* AVP classes */
#define AVP_CLASS_URI    (1<<4)
#define AVP_CLASS_USER   (1<<5)
#define AVP_CLASS_DOMAIN (1<<6)
#define AVP_CLASS_GLOBAL (1<<7)

/* AVP track (either from or to) */
#define AVP_TRACK_FROM   (1<<8)
#define AVP_TRACK_TO     (1<<9)
#define AVP_TRACK_ALL    (AVP_TRACK_FROM|AVP_TRACK_TO)

#define AVP_CLASS_ALL (AVP_CLASS_URI|AVP_CLASS_USER|AVP_CLASS_DOMAIN|AVP_CLASS_GLOBAL)

/* AVP name index */
#define AVP_INDEX_FORWARD	(1<<10)
#define AVP_INDEX_BACKWARD	(1<<11)
#define AVP_INDEX_ALL		(AVP_INDEX_FORWARD | AVP_INDEX_BACKWARD)

/* AVP DB flag used by avpops module - defined in avpops
 * - kept here for reference */
// #define AVP_IS_IN_DB    (1<<12)

#define AVP_CUSTOM_FLAGS	13

#define GALIAS_CHAR_MARKER  '$'

#define AVP_NAME_VALUE_MASK     0x0007
#define AVP_CORE_MASK           0x00ff
#define AVP_SCRIPT_MASK         0xff00
#define avp_core_flags(f)       ((f)&0x00ff)
#define avp_script_flags(f)     (((f)<<8)&0xff00)
#define avp_get_script_flags(f) (((f)&0xff00)>>8)

#define is_avp_str_name(a)      ((a)->flags&AVP_NAME_STR)
#define is_avp_str_val(a)       ((a)->flags&AVP_VAL_STR)


#define AVP_IS_ASSIGNABLE(ident) ( ((ident).flags & AVP_NAME_RE) == 0 && (((ident).flags & AVP_NAME) == 0 || (((ident)->flags & AVP_NAME) && (ident).name.s.len)) )
/* Initialize memory structures */
int init_avps(void);

/* add avp to the list of avps */
int add_avp(avp_flags_t flags, avp_name_t name, avp_value_t val);
int add_avp_before(avp_t *avp, avp_flags_t flags, avp_name_t name, avp_value_t val);
int add_avp_list(avp_list_t* list, avp_flags_t flags, avp_name_t name, avp_value_t val);

/* Delete avps with given type and name */
void delete_avp(avp_flags_t flags, avp_name_t name);

int destroy_avps(avp_flags_t flags, avp_name_t name, int all);

/* search functions */
avp_t *search_first_avp( avp_flags_t flags, avp_name_t name,
			 avp_value_t *val, struct search_state* state);
avp_t *search_avp_by_index( avp_flags_t flags, avp_name_t name,
                            avp_value_t *val, avp_index_t index);

avp_t *search_avp (avp_ident_t ident, avp_value_t* val, struct search_state* state);
avp_t *search_next_avp(struct search_state* state, avp_value_t *val);

/* Reset one avp list */
int reset_avp_list(int flags);

/* free functions */
void reset_avps(void);

void destroy_avp(avp_t *avp);
void destroy_avp_list(avp_list_t *list );
void destroy_avp_list_unsafe(avp_list_t *list );

/* get func */
void get_avp_val(avp_t *avp, avp_value_t *val );
str* get_avp_name(avp_t *avp);

avp_list_t get_avp_list(avp_flags_t flags);
avp_list_t* set_avp_list(avp_flags_t flags, avp_list_t* list);


/* global alias functions (manipulation and parsing)*/
int add_avp_galias_str(char *alias_definition);
int lookup_avp_galias(str *alias, int *type, int_str *avp_name);
int add_avp_galias(str *alias, int type, int_str avp_name);
int parse_avp_ident( str *name, avp_ident_t* attr);
int parse_avp_name( str *name, int *type, int_str *avp_name, int *index);
int parse_avp_spec( str *name, int *type, int_str *avp_name, int *index);
int km_parse_avp_spec( str *name, int *type, int_str *avp_name);
void free_avp_name( avp_flags_t *type, int_str *avp_name);
/* Free an ident obtained with parse_avp_ident() */
void free_avp_ident(avp_ident_t* attr);

/* AVP flags functions */
#define MAX_AVPFLAG  ((unsigned int)( sizeof(avp_flags_t) * CHAR_BIT - 1 - AVP_CUSTOM_FLAGS))

avp_flags_t register_avpflag(char* name);
avp_flags_t get_avpflag_no(char* name);

#endif
