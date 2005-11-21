/*
 * $Id$
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
 *
 * History:
 * ---------
 *  2004-07-21  created (bogdan)
 *  2004-11-14  global aliases support added
 *  2005-02-14  list with FLAGS USAGE added (bogdan)
 */

#ifndef _SER_URS_AVP_H_
#define _SER_URS_AVP_H_

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
 *
 */

#include "str.h"


#define AVP_UID          "uid"           /* Unique user identifier */
#define AVP_DID          "did"           /* Unique domain identifier */
#define AVP_REALM        "digest_realm"  /* Digest realm */
#define AVP_FR_TIMER     "fr_timer"      /* Value of final response timer */
#define AVP_FR_INV_TIMER "fr_inv_timer"  /* Value of final response invite timer */
#define AVP_RPID         "rpid"          /* Remote-Party-ID */
#define AVP_GFLAGS        "gflags"       /* global flags */


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
	str *s;
	regex_t* re;
} int_str;


typedef struct usr_avp {
	unsigned short id;
	     /* Flags that are kept for the AVP lifetime */
	unsigned short flags;
	struct usr_avp *next;
	void *data;
} avp_t;


/*
 * AVP search state
 */
struct search_state {
	unsigned short flags;  /* Type of search and additional flags */
	unsigned short id;
	int_str name;
	avp_t* avp;            /* Current AVP */
	regex_t* search_re;    /* Compiled regular expression */
};


#define AVP_NAME_STR     (1<<0)
#define AVP_VAL_STR      (1<<1)
#define AVP_NAME_RE      (1<<2)
#define AVP_USER         (1<<5)
#define AVP_DOMAIN       (1<<6)
#define AVP_GLOBAL       (1<<7)

#define ALL_AVP_CLASSES (AVP_USER|AVP_DOMAIN|AVP_GLOBAL)

/* True for user avps */
#define IS_USER_AVP(flags) ((flags) & AVP_USER)

/* True for domain avps */
#define IS_DOMAIN_AVP(flags) ((flags) & AVP_DOMAIN)

/* true for global avps */
#define IS_GLOBAL_AVP(flags) ((flags) & AVP_GLOBAL)

#define GALIAS_CHAR_MARKER  '$'

/* add functions */
int add_avp(unsigned short flags, int_str name, int_str val);

int add_avp_list(avp_t** list, unsigned short flags, int_str name, int_str val);

/* search functions */
avp_t *search_first_avp( unsigned short flags, int_str name,
			 int_str *val, struct search_state* state);
avp_t *search_next_avp(struct search_state* state, int_str *val);

/* free functions */
void reset_user_avps(void);
void reset_domain_avps(void);

void destroy_avp(avp_t *avp);
void destroy_avp_list(avp_t **list );
void destroy_avp_list_unsafe(avp_t **list );

/* get func */
void get_avp_val(avp_t *avp, int_str *val );
str* get_avp_name(avp_t *avp);

avp_t** get_user_avp_list(void);   /* Return current list of user avps */
avp_t** get_domain_avp_list(void); /* Return current list of domain avps */
avp_t** get_global_avp_list(void); /* Return current list of global avps */

avp_t** set_user_avp_list(avp_t **list);   /* Set current list of user avps to list */
avp_t** set_domain_avp_list(avp_t **list); /* Set current list of domain avps to list */
avp_t** set_global_avp_list(avp_t **list); /* Set current list of global avps to list */


/* global alias functions (manipulation and parsing)*/
int add_avp_galias_str(char *alias_definition);
int lookup_avp_galias(str *alias, int *type, int_str *avp_name);
int add_avp_galias(str *alias, int type, int_str avp_name);
int parse_avp_name( str *name, int *type, int_str *avp_name);
int parse_avp_spec( str *name, int *type, int_str *avp_name);

#endif
