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


/*!
 * \file
 * \brief Kamailio core :: Attribute value pair handling (AVP)
 * \ingroup core
 * Module: \ref core
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

#include "sr_module.h"
#include "dprint.h"
#include "str.h"
#include "ut.h"
#include "mem/shm_mem.h"
#include "mem/mem.h"
#include "usr_avp.h"

enum idx {
	IDX_FROM_URI = 0,
	IDX_TO_URI,
	IDX_FROM_USER,
	IDX_TO_USER,
	IDX_FROM_DOMAIN,
	IDX_TO_DOMAIN,
	IDX_MAX
};


struct avp_galias {
	str alias;
	struct avp_spec  avp;
	struct avp_galias *next;
};

static struct avp_galias *galiases = 0;

static avp_list_t def_list[IDX_MAX];    /* Default AVP lists */
static avp_list_t* crt_list[IDX_MAX];  /* Pointer to current AVP lists */

/* Global AVP related variables go to shm mem */
static avp_list_t* def_glist;
static avp_list_t** crt_glist;

/* AVP flags */
int registered_avpflags_no = 0;
static char *registered_avpflags[MAX_AVPFLAG];

/* Initialize AVP lists in private memory and allocate memory
 * for shared lists
 */
int init_avps(void)
{
	int i;
	     /* Empty default lists */
	memset(def_list, 0, sizeof(avp_list_t) * IDX_MAX);

	     /* Point current pointers to default lists */
	for(i = 0; i < IDX_MAX; i++) {
		crt_list[i] = &def_list[i];
	}

	def_glist = (avp_list_t*)shm_malloc(sizeof(avp_list_t));
	crt_glist = (avp_list_t**)shm_malloc(sizeof(avp_list_t*));
	if (!def_glist || !crt_glist) {
		LM_ERR("No memory to allocate default global AVP list\n");
		return -1;
	}
	*def_glist = 0;
	*crt_glist = def_glist;
	return 0;
}


/*
 * Select active AVP list based on the value of flags
 */
static avp_list_t* select_list(avp_flags_t flags)
{
	if (flags & AVP_CLASS_URI) {
		if (flags & AVP_TRACK_TO) {
			return crt_list[IDX_TO_URI];
		} else {
			return crt_list[IDX_FROM_URI];
		}
	} else if (flags & AVP_CLASS_USER) {
		if (flags & AVP_TRACK_TO) {
			return crt_list[IDX_TO_USER];
		} else {
			return crt_list[IDX_FROM_USER];
		}
	} else if (flags & AVP_CLASS_DOMAIN) {
		if (flags & AVP_TRACK_TO) {
			return crt_list[IDX_TO_DOMAIN];
		} else {
			return crt_list[IDX_FROM_DOMAIN];
		}
	} else if (flags & AVP_CLASS_GLOBAL) {
		return *crt_glist;
	}

	return NULL;
}

inline static avp_id_t compute_ID( str *name )
{
	char *p;
	avp_id_t id;

	id=0;
	for( p=name->s+name->len-1 ; p>=name->s ; p-- )
		id ^= *p;
	return id;
}


avp_t *create_avp (avp_flags_t flags, avp_name_t name, avp_value_t val)
{
	avp_t *avp;
	str *s;
	struct str_int_data *sid;
	struct str_str_data *ssd;
	int len;

	if (name.s.s == 0 && name.s.len == 0) {
		LM_ERR("0 ID or NULL NAME AVP!");
		goto error;
	}

	/* compute the required mem size */
	len = sizeof(struct usr_avp);
	if (flags&AVP_NAME_STR) {
		if ( name.s.s==0 || name.s.len==0) {
			LM_ERR("EMPTY NAME AVP!");
			goto error;
		}
		if (flags&AVP_VAL_STR) {
			len += sizeof(struct str_str_data)-sizeof(union usr_avp_data)
				+ name.s.len + 1 /* Terminating zero for regex search */
				+ val.s.len + 1; /* Value is zero terminated */
		} else {
			len += sizeof(struct str_int_data)-sizeof(union usr_avp_data)
				+ name.s.len + 1; /* Terminating zero for regex search */
		}
	} else if (flags&AVP_VAL_STR) {
		len += sizeof(str)-sizeof(union usr_avp_data) + val.s.len + 1;
	}

	avp = (struct usr_avp*)shm_malloc( len );
	if (avp==0) {
		LM_ERR("no more shm mem\n");
		return 0;
	}

	avp->flags = flags;
	avp->id = (flags&AVP_NAME_STR)? compute_ID(&name.s) : name.n ;
	avp->next = NULL;

	switch ( flags&(AVP_NAME_STR|AVP_VAL_STR) )
	{
		case 0:
			/* avp type ID, int value */
			avp->d.l = val.n;
			break;
		case AVP_NAME_STR:
			/* avp type str, int value */
			sid = (struct str_int_data*)&avp->d.data[0];
			sid->val = val.n;
			sid->name.len =name.s.len;
			sid->name.s = (char*)sid + sizeof(struct str_int_data);
			memcpy( sid->name.s , name.s.s, name.s.len);
			sid->name.s[name.s.len] = '\0'; /* Zero terminator */
			break;
		case AVP_VAL_STR:
			/* avp type ID, str value */
			s = (str*)&avp->d.data[0];
			s->len = val.s.len;
			s->s = (char*)s + sizeof(str);
			memcpy( s->s, val.s.s , s->len);
			s->s[s->len] = 0;
			break;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			ssd = (struct str_str_data*)&avp->d.data[0];
			ssd->name.len = name.s.len;
			ssd->name.s = (char*)ssd + sizeof(struct str_str_data);
			memcpy( ssd->name.s , name.s.s, name.s.len);
			ssd->name.s[name.s.len]='\0'; /* Zero terminator */
			ssd->val.len = val.s.len;
			ssd->val.s = ssd->name.s + ssd->name.len + 1;
			memcpy( ssd->val.s , val.s.s, val.s.len);
			ssd->val.s[ssd->val.len] = 0;
			break;
	}
	return avp;
error:
	return 0;
}

int add_avp_list(avp_list_t* list, avp_flags_t flags, avp_name_t name, avp_value_t val)
{
	avp_t *avp;

	assert(list != 0);

	if ((avp = create_avp(flags, name, val))) {
		avp->next = *list;
		*list = avp;
		return 0;
	}

	return -1;
}


int add_avp(avp_flags_t flags, avp_name_t name, avp_value_t val)
{
	avp_flags_t avp_class;
	avp_list_t* list;

	     /* Add avp to uri class if no class has been
	      * specified by the caller
	      */
	if ((flags & AVP_CLASS_ALL) == 0) flags |= AVP_CLASS_URI;
	if ((flags & AVP_TRACK_ALL) == 0) flags |= AVP_TRACK_FROM;
	if (!(list = select_list(flags)))
		return -1;

	if (flags & AVP_CLASS_URI) avp_class = AVP_CLASS_URI;
	else if (flags & AVP_CLASS_USER) avp_class = AVP_CLASS_USER;
	else if (flags & AVP_CLASS_DOMAIN) avp_class = AVP_CLASS_DOMAIN;
	else avp_class = AVP_CLASS_GLOBAL;

	     /* Make that only the selected class is set
	      * if the caller set more classes in flags
	      */
	return add_avp_list(list, flags & (~(AVP_CLASS_ALL) | avp_class), name, val);
}

int add_avp_before(avp_t *avp, avp_flags_t flags, avp_name_t name, avp_value_t val)
{
	avp_t *new_avp;

	if (!avp) {
		return add_avp(flags, name, val);
	}

	if ((flags & AVP_CLASS_ALL) == 0) flags |= (avp->flags & AVP_CLASS_ALL);
	if ((flags & AVP_TRACK_ALL) == 0) flags |= (avp->flags & AVP_TRACK_ALL);

	if ((avp->flags & (AVP_CLASS_ALL|AVP_TRACK_ALL)) != (flags & (AVP_CLASS_ALL|AVP_TRACK_ALL))) {
		LM_ERR("Source and target AVPs have different CLASS/TRACK\n");
		return -1;
	}
	if ((new_avp=create_avp(flags, name, val))) {
		new_avp->next=avp->next;
		avp->next=new_avp;
		return 0;
	}
	return -1;
}

/* get value functions */
inline str* get_avp_name(avp_t *avp)
{
	struct str_int_data *sid;
	struct str_str_data *ssd;
	
	switch ( avp->flags&(AVP_NAME_STR|AVP_VAL_STR) )
	{
		case 0:
			/* avp type ID, int value */
		case AVP_VAL_STR:
			/* avp type ID, str value */
			return 0;
		case AVP_NAME_STR:
			/* avp type str, int value */
			sid = (struct str_int_data*)&avp->d.data[0];
			return &sid->name;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			ssd = (struct str_str_data*)&avp->d.data[0];
			return &ssd->name;
	}

	LM_ERR("unknown avp type (name&val) %d\n", avp->flags&(AVP_NAME_STR|AVP_VAL_STR));
	return 0;
}


inline void get_avp_val(avp_t *avp, avp_value_t *val)
{
	str *s;
	struct str_int_data *sid;
	struct str_str_data *ssd;
	
	if (avp==0 || val==0)
		return;

	switch ( avp->flags&(AVP_NAME_STR|AVP_VAL_STR) ) {
		case 0:
			/* avp type ID, int value */
			val->n = avp->d.l;
			break;
		case AVP_NAME_STR:
			/* avp type str, int value */
			sid = (struct str_int_data*)&avp->d.data[0];
			val->n = sid->val;
			break;
		case AVP_VAL_STR:
			/* avp type ID, str value */
			s = (str*)&avp->d.data[0];
			val->s = *s;
			break;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			ssd = (struct str_str_data*)&avp->d.data[0];
			val->s = ssd->val;
			break;
	}
}


/* Return the current list of user attributes */
avp_list_t get_avp_list(avp_flags_t flags)
{
	avp_list_t *list;

	list = select_list(flags);
	return (list ? *list : NULL);
}


/*
 * Compare given id with id in avp, return true if they match
 */
static inline int match_by_id(avp_t* avp, avp_id_t id)
{
	if (avp->id == id && (avp->flags&AVP_NAME_STR)==0) {
		return 1;
	}
	return 0;
}


/*
 * Compare given name with name in avp, return true if they are same
 */
static inline int match_by_name(avp_t* avp, avp_id_t id, str* name)
{
	str* avp_name;
	if (id==avp->id && avp->flags&AVP_NAME_STR &&
	    (avp_name=get_avp_name(avp))!=0 && avp_name->len==name->len
	    && !strncasecmp( avp_name->s, name->s, name->len) ) {
		return 1;
	}
	return 0;
}


/*
 * Compare name with name in AVP using regular expressions, return
 * true if they match
 */
static inline int match_by_re(avp_t* avp, regex_t* re)
{
	regmatch_t pmatch;
	str * avp_name;
	     /* AVP identifiable by name ? */
	if (!(avp->flags&AVP_NAME_STR)) return 0;
	if ((avp_name=get_avp_name(avp))==0) /* valid AVP name ? */
		return 0;
	if (!avp_name->s) /* AVP name validation */
		return 0;
	if (regexec(re, avp_name->s, 1, &pmatch,0)==0) { /* re match ? */
		return 1;
	}
	return 0;
}


avp_t *search_first_avp(avp_flags_t flags, avp_name_t name, avp_value_t *val, struct search_state* s)
{
	avp_ident_t id;
	id.flags = flags;
	id.name = name;
	id.index = 0;
	return search_avp (id, val, s);
}

avp_t *search_avp (avp_ident_t ident, avp_value_t* val, struct search_state* state)
{
	avp_t* ret;
	static struct search_state st;
	avp_list_t* list;

	if (ident.name.s.s==0 && ident.name.s.len == 0) {
		LM_ERR("0 ID or NULL NAME AVP!");
		return 0;
	}

	switch (ident.flags & AVP_INDEX_ALL) {
		case AVP_INDEX_BACKWARD:
		case AVP_INDEX_FORWARD:
			WARN("AVP specified with index, but not used for search\n");
			break;
	}

	if (!state) state = &st;

	if ((ident.flags & AVP_CLASS_ALL) == 0) {
		     /* The caller did not specify any class to search in, so enable
		      * all of them by default
		      */
		ident.flags |= AVP_CLASS_ALL;

		if ((ident.flags & AVP_TRACK_ALL) == 0) {
		    /* The caller did not specify even the track to search in, so search
		     * in the track_from
		     */
			ident.flags |= AVP_TRACK_FROM;
		}
	}

	if (!(list = select_list(ident.flags)))
		return NULL;

	state->flags = ident.flags;
	state->avp = *list;
	state->name = ident.name;

	if (ident.flags & AVP_NAME_STR) {
		state->id = compute_ID(&ident.name.s);
	}

        ret = search_next_avp(state, val);

	     /* Make sure that search next avp stays in the same class as the first
	      * avp found
	      */
	if (state && ret) state->flags = (ident.flags & ~AVP_CLASS_ALL) | (ret->flags & AVP_CLASS_ALL);
	return ret;
}

avp_t *search_next_avp(struct search_state* s, avp_value_t *val )
{
	int matched;
	avp_t* avp;
	avp_list_t *list;

	if (s == 0) {
		LM_ERR("Invalid parameter value\n");
		return 0;
	}

	switch (s->flags & AVP_INDEX_ALL) {
		case AVP_INDEX_BACKWARD:
		case AVP_INDEX_FORWARD:
			WARN("AVP specified with index, but not used for search\n");
			break;
	}

	while(1) {
		for( ; s->avp; s->avp = s->avp->next) {
			if (s->flags & AVP_NAME_RE) {
				matched = match_by_re(s->avp, s->name.re);
			} else if (s->flags & AVP_NAME_STR) {
				matched = match_by_name(s->avp, s->id, &s->name.s);
			} else {
				matched = match_by_id(s->avp, s->name.n);
			}
			if (matched) {
				avp = s->avp;
				s->avp = s->avp->next;
				if (val) get_avp_val(avp, val);
				return avp;
			}
		}

		if (s->flags & AVP_CLASS_URI) {
			s->flags &= ~AVP_CLASS_URI;
			list = select_list(s->flags);
		} else if (s->flags & AVP_CLASS_USER) {
			s->flags &= ~AVP_CLASS_USER;
			list = select_list(s->flags);
		} else if (s->flags & AVP_CLASS_DOMAIN) {
			s->flags &= ~AVP_CLASS_DOMAIN;
			list = select_list(s->flags);
		} else {
			s->flags &= ~AVP_CLASS_GLOBAL;
			return 0;
		}
		if (!list) return 0;
		s->avp = *list;
	}

	return 0;
}

int search_reverse( avp_t *cur, struct search_state* st,
                     avp_index_t index, avp_list_t *ret)
{
	avp_index_t lvl;

	if (!cur)
		return 0;
	lvl = search_reverse(search_next_avp(st, NULL), st, index, ret)+1;
	if (index==lvl)
		*ret=cur;
	return lvl;
}

avp_t *search_avp_by_index( avp_flags_t flags, avp_name_t name,
                            avp_value_t *val, avp_index_t index)
{
	avp_t *ret, *cur;
	struct search_state st;

	if (flags & AVP_NAME_RE) {
		BUG("search_by_index not supported for AVP_NAME_RE\n");
		return 0;
	}
	switch (flags & AVP_INDEX_ALL) {
		case 0:
			ret = search_first_avp(flags, name, val, &st);
			if (!ret || search_next_avp(&st, NULL))
				return 0;
			else
				return ret;
		case AVP_INDEX_ALL:
			BUG("search_by_index not supported for anonymous index []\n");
			return 0;
		case AVP_INDEX_FORWARD:
			ret = NULL;
			cur = search_first_avp(flags & ~AVP_INDEX_ALL, name, NULL, &st);
			search_reverse(cur, &st, index, &ret);
			if (ret && val)
				get_avp_val(ret, val);
			return ret;
		case AVP_INDEX_BACKWARD:
			ret = search_first_avp(flags & ~AVP_INDEX_ALL, name, val, &st);
			for (index--; (ret && index); ret=search_next_avp(&st, val), index--);
			return ret;
	}

	return 0;
}

/* FIXME */
/********* free functions ********/

void destroy_avp(avp_t *avp_del)
{
	int i;
	avp_t *avp, *avp_prev;

	for (i = 0; i < IDX_MAX; i++) {
		for( avp_prev=0,avp=*crt_list[i] ; avp ;
		     avp_prev=avp,avp=avp->next ) {
			if (avp==avp_del) {
				if (avp_prev) {
					avp_prev->next=avp->next;
				} else {
					*crt_list[i] = avp->next;
				}
				shm_free(avp);
				return;
			}
		}
	}

	for( avp_prev=0,avp=**crt_glist ; avp ;
	     avp_prev=avp,avp=avp->next ) {
		if (avp==avp_del) {
			if (avp_prev) {
				avp_prev->next=avp->next;
			} else {
				**crt_glist = avp->next;
			}
			shm_free(avp);
			return;
		}
	}
}


void destroy_avp_list_unsafe(avp_list_t* list)
{
	avp_t *avp, *foo;

	avp = *list;
	while( avp ) {
		foo = avp;
		avp = avp->next;
		shm_free_unsafe( foo );
	}
	*list = 0;
}


inline void destroy_avp_list(avp_list_t* list)
{
	avp_t *avp, *foo;

	LM_DBG("destroying list %p\n", *list);
	avp = *list;
	while( avp ) {
		foo = avp;
		avp = avp->next;
		shm_free( foo );
	}
	*list = 0;
}

int reset_avp_list(int flags)
{
    int i;
    if (flags & AVP_CLASS_URI) {
	if (flags & AVP_TRACK_FROM) i = IDX_FROM_URI;
	else i = IDX_TO_URI;
    } else if (flags & AVP_CLASS_USER) {
	if (flags & AVP_TRACK_FROM) i = IDX_FROM_USER;
	else i = IDX_TO_USER;
    } else if (flags & AVP_CLASS_DOMAIN) {
	if (flags & AVP_TRACK_FROM) i = IDX_FROM_DOMAIN;
	else i = IDX_TO_DOMAIN;
    } else return -1;

    crt_list[i] = &def_list[i];
    destroy_avp_list(crt_list[i]);
    return 0;
}

void reset_avps(void)
{
	int i;
	for(i = 0; i < IDX_MAX; i++) {
		crt_list[i] = &def_list[i];
		destroy_avp_list(crt_list[i]);
	}
}


avp_list_t* set_avp_list( avp_flags_t flags, avp_list_t* list )
{
	avp_list_t* prev;

	if (flags & AVP_CLASS_URI) {
		if (flags & AVP_TRACK_FROM) {
			prev = crt_list[IDX_FROM_URI];
			crt_list[IDX_FROM_URI] = list;
		} else {
			prev = crt_list[IDX_TO_URI];
			crt_list[IDX_TO_URI] = list;
		}
	} else if (flags & AVP_CLASS_USER) {
		if (flags & AVP_TRACK_FROM) {
			prev = crt_list[IDX_FROM_USER];
			crt_list[IDX_FROM_USER] = list;
		} else {
			prev = crt_list[IDX_TO_USER];
			crt_list[IDX_TO_USER] = list;
		}
	} else if (flags & AVP_CLASS_DOMAIN) {
		if (flags & AVP_TRACK_FROM) {
			prev = crt_list[IDX_FROM_DOMAIN];
			crt_list[IDX_FROM_DOMAIN] = list;
		} else {
			prev = crt_list[IDX_TO_DOMAIN];
			crt_list[IDX_TO_DOMAIN] = list;
		}
	} else {
		prev = *crt_glist;
	        *crt_glist = list;
	}

	return prev;
}


/********* global aliases functions ********/

static inline int check_avp_galias(str *alias, int type, int_str avp_name)
{
	struct avp_galias *ga;

	type &= AVP_NAME_STR;

	for( ga=galiases ; ga ; ga=ga->next ) {
		/* check for duplicated alias names */
		if ( alias->len==ga->alias.len &&
		(strncasecmp( alias->s, ga->alias.s, alias->len)==0) )
			return -1;
		/*check for duplicated avp names */
		if (type==ga->avp.type) {
			if (type&AVP_NAME_STR){
				if (avp_name.s.len==ga->avp.name.s.len &&
				    (strncasecmp(avp_name.s.s, ga->avp.name.s.s,
						 avp_name.s.len)==0) )
					return -1;
			} else {
				if (avp_name.n==ga->avp.name.n)
					return -1;
			}
		}
	}
	return 0;
}


int add_avp_galias(str *alias, int type, int_str avp_name)
{
	struct avp_galias *ga;

	if ((type&AVP_NAME_STR && (!avp_name.s.s ||
				   !avp_name.s.len)) ||!alias || !alias->s ||
		!alias->len ){
		LM_ERR("null params received\n");
		goto error;
	}

	if (check_avp_galias(alias,type,avp_name)!=0) {
		LM_ERR("duplicate alias/avp entry\n");
		goto error;
	}

	ga = (struct avp_galias*)pkg_malloc( sizeof(struct avp_galias) );
	if (ga==0) {
		LM_ERR("no more pkg memory\n");
		goto error;
	}

	ga->alias.s = (char*)pkg_malloc( alias->len+1 );
	if (ga->alias.s==0) {
		LM_ERR("no more pkg memory\n");
		goto error1;
	}
	memcpy( ga->alias.s, alias->s, alias->len);
	ga->alias.len = alias->len;

	ga->avp.type = type&AVP_NAME_STR;

	if (type&AVP_NAME_STR) {
		ga->avp.name.s.s = (char*)pkg_malloc( avp_name.s.len+1 );
		if (ga->avp.name.s.s==0) {
			LM_ERR("no more pkg memory\n");
			goto error2;
		}
		ga->avp.name.s.len = avp_name.s.len;
		memcpy( ga->avp.name.s.s, avp_name.s.s, avp_name.s.len);
		ga->avp.name.s.s[avp_name.s.len] = 0;
		LM_DBG("registering <%s> for avp name <%s>\n",
			ga->alias.s, ga->avp.name.s.s);
	} else {
		ga->avp.name.n = avp_name.n;
		LM_DBG("registering <%s> for avp id <%d>\n",
			ga->alias.s, ga->avp.name.n);
	}

	ga->next = galiases;
	galiases = ga;

	return 0;
error2:
	pkg_free(ga->alias.s);
error1:
	pkg_free(ga);
error:
	return -1;
}


int lookup_avp_galias(str *alias, int *type, int_str *avp_name)
{
	struct avp_galias *ga;

	for( ga=galiases ; ga ; ga=ga->next )
		if (alias->len==ga->alias.len &&
		(strncasecmp( alias->s, ga->alias.s, alias->len)==0) ) {
			*type = ga->avp.type;
			*avp_name = ga->avp.name;
			return 0;
		}

	return -1;
}


/* parsing functions */
#define ERR_IF_CONTAINS(name,chr) \
	if (memchr(name->s,chr,name->len)) { \
		LM_ERR("Unexpected control character '%c' in AVP name\n", chr); \
		goto error; \
	}

int parse_avp_name( str *name, int *type, int_str *avp_name, int *index)
{
	int ret;
	avp_ident_t attr;

	ret=parse_avp_ident(name, &attr);
	if (!ret) {
		if (type) *type = attr.flags;
		if (avp_name) *avp_name = attr.name;
		if (index) *index = attr.index;
	}
	return ret;
}


/** parse an avp indentifier.
 *
 * Parses the following avp indentifier forms:
 *       - "i:<number>"  - old form, deprecated  (e.g. i:42)
 *       - "s:<string>"  - old form, deprecated  (e.g. s:foo)
 *       - "<track>.<name>"                      (e.g.: f.bar)
 *       - "<track>.<name>[<index>]"             (e.g.: f.bar[1])
 *       - "<track><class>.<name>"               (e.g:  tu.bar)
 *       - "<track><class>.<name>[<index>]"      (e.g:  fd.bar[2])
 *       - "<string>"                            (e.g.: foo)
 * Where:
 *          \<string\> = ascii string
 *          \<id\>   = ascii string w/o '[', ']', '.' and '/'
 *          \<name\> = \<id\> | '/' regex '/'
 *                   (Note: regex use is deprecated)
 *          \<track\> = 'f' | 't'
 *                   (from or to)
 *          \<class\> = 'r' | 'u' | 'd' | 'g'
 *                    (uri, user, domain or global)
 *          \<index\> = \<number\> | '-' \<number\> | ''
 *                    (the avp index, if missing it means AVP_INDEX_ALL, but
 *                     it's use is deprecated)
 * More examples:
 *       "fr.bar[1]"  - from track, uri class, avp "bar", the value 1.
 *       "tu./^foo/"  - to track,  user class, all avps for which the name
 *                      starts with foo (note RE in avp names are deprecated).
 *        "t.did"     - to track, "did" avp
 *
 * @param name  - avp identifier
 * @param *attr - the result will be stored here
 * @return 0 on success, -1 on error
 */
int parse_avp_ident( str *name, avp_ident_t* attr)
{
	unsigned int id;
	char c;
	char *p;
	str s;

	if (name==0 || name->s==0 || name->len==0) {
		LM_ERR("NULL name or name->s or name->len\n");
		goto error;
	}

	attr->index = 0;
	LM_DBG("Parsing '%.*s'\n", name->len, name->s);
	if (name->len>=2 && name->s[1]==':') { /* old fashion i: or s: */
	        /* WARN("i: and s: avp name syntax is deprecated!\n"); */
		c = name->s[0];
		name->s += 2;
		name->len -= 2;
		if (name->len==0)
			goto error;
		switch (c) {
			case 's': case 'S':
				attr->flags = AVP_NAME_STR;
				attr->name.s = *name;
				break;
			case 'i': case 'I':
				attr->flags = 0;
				if (str2int( name, &id)!=0) {
					LM_ERR("invalid ID <%.*s> - not a number\n",
						name->len, name->s);
					goto error;
				}
				attr->name.n = (int)id;
				break;
			default:
				LM_ERR("unsupported type [%c]\n", c);
				goto error;
		}
	} else if ((p=memchr(name->s, '.', name->len))) {
		if (p-name->s==1) {
			id=name->s[0];
			name->s +=2;
			name->len -=2;
		} else if (p-name->s==2) {
			id=name->s[0]<<8 | name->s[1];
			name->s +=3;
			name->len -=3;
		} else {
			LM_ERR("AVP unknown class prefix '%.*s'\n", name->len, name->s);
			goto error;
		}
		if (name->len==0) {
			LM_ERR("AVP name not specified after the prefix separator\n");
			goto error;
		}
		switch (id) {
			case 'f':
				attr->flags = AVP_TRACK_FROM;
				break;
			case 't':
				attr->flags = AVP_TRACK_TO;
				break;
			case 0x6672: /* 'fr' */
				attr->flags = AVP_TRACK_FROM | AVP_CLASS_URI;
				break;
			case 0x7472: /* 'tr' */
				attr->flags = AVP_TRACK_TO | AVP_CLASS_URI;
				break;				
			case 0x6675: /* 'fu' */
				attr->flags = AVP_TRACK_FROM | AVP_CLASS_USER;
				break;
			case 0x7475: /* 'tu' */
				attr->flags = AVP_TRACK_TO | AVP_CLASS_USER;
				break;
			case 0x6664: /* 'fd' */
				attr->flags = AVP_TRACK_FROM | AVP_CLASS_DOMAIN;
				break;
			case 0x7464: /* 'td' */
				attr->flags = AVP_TRACK_TO | AVP_CLASS_DOMAIN;
				break;
			case 'g':
				attr->flags = AVP_TRACK_ALL | AVP_CLASS_GLOBAL;
				break;
			default:
				if (id < 1<<8)
					LM_ERR("AVP unknown class prefix '%c'\n", id);
				else
					LM_ERR("AVP unknown class prefix '%c%c'\n", id>>8,id);
				goto error;
		}
		if (name->s[name->len-1]==']') {
			p=memchr(name->s, '[', name->len);
			if (!p) {
				LM_ERR("missing '[' for AVP index\n");
				goto error;
			}
			s.s=p+1;
			s.len=name->len-(p-name->s)-2; /* [ and ] */
			if (s.len == 0) {
				attr->flags |= AVP_INDEX_ALL;
			} else {
				if (s.s[0]=='-') {
					attr->flags |= AVP_INDEX_BACKWARD;
					s.s++;s.len--;
				} else {
					attr->flags |= AVP_INDEX_FORWARD;
				}
				if ((str2int(&s, &id) != 0)||(id==0)) {
					LM_ERR("Invalid AVP index '%.*s'\n", s.len, s.s);
					goto error;
				}
				attr->index = id;
			}
			name->len=p-name->s;
		}
		ERR_IF_CONTAINS(name,'.');
		ERR_IF_CONTAINS(name,'[');
		ERR_IF_CONTAINS(name,']');
		if ((name->len > 2) && (name->s[0]=='/') && (name->s[name->len-1]=='/')) {
			attr->name.re=pkg_malloc(sizeof(regex_t));
			if (!attr->name.re) {
				BUG("No free memory to allocate AVP_NAME_RE regex\n");
				goto error;
			}
			name->s[name->len-1]=0;
			if (regcomp(attr->name.re, name->s+1, REG_EXTENDED|REG_NOSUB|REG_ICASE)) {
				pkg_free(attr->name.re);
				attr->name.re=0;
				name->s[name->len-1] = '/';
				goto error;
			}
			name->s[name->len-1] = '/';
			attr->flags |= AVP_NAME_RE;
		} else {
			ERR_IF_CONTAINS(name,'/');
			attr->flags |= AVP_NAME_STR;
			attr->name.s = *name;
		}
	} else {
		/*default is string name*/
		attr->flags = AVP_NAME_STR;
		attr->name.s = *name;
	}

	return 0;
error:
	return -1;
}

void free_avp_ident(avp_ident_t* attr)
{
	if (attr->flags & AVP_NAME_RE) {
		if (! attr->name.re) {
			BUG("attr ident @%p has the regexp flag set, but no regexp.\n",
					attr);
#ifdef EXTRA_DEBUG
			abort();
#endif
		} else {
			regfree(attr->name.re);
			pkg_free(attr->name.re);
		}
	}
}

int km_parse_avp_spec( str *name, int *type, int_str *avp_name)
{
	char *p;
	int index = 0;

	if (name==0 || name->s==0 || name->len==0)
		return -1;

	p = (char*)memchr((void*)name->s, ':', name->len);
	if (p==NULL) {
		/* might be kamailio avp alias or ser avp name style */
		if(lookup_avp_galias( name, type, avp_name)==0)
			return 0; /* found */
	}
	return parse_avp_name( name, type, avp_name, &index);
}


int parse_avp_spec( str *name, int *type, int_str *avp_name, int *index)
{
	str alias;

	if (name==0 || name->s==0 || name->len==0)
		return -1;

	if (name->s[0]==GALIAS_CHAR_MARKER) {
		/* it's an avp alias */
		if (name->len==1) {
			LM_ERR("empty alias\n");
			return -1;
		}
		alias.s = name->s+1;
		alias.len = name->len-1;
		return lookup_avp_galias( &alias, type, avp_name);
	} else {
		return parse_avp_name( name, type, avp_name, index);
	}
}

void free_avp_name(avp_flags_t *type, int_str *avp_name)
{
	if ((*type & AVP_NAME_RE) && (avp_name->re)){
		regfree(avp_name->re);
		pkg_free(avp_name->re);
		avp_name->re=0;
	}
}

int add_avp_galias_str(char *alias_definition)
{
	int_str avp_name;
	char *s;
	str  name;
	str  alias;
	int  type;
	int  index;

	s = alias_definition;
	while(*s && isspace((int)*s))
		s++;

	while (*s) {
		/* parse alias name */
		alias.s = s;
		while(*s && *s!=';' && !isspace((int)*s) && *s!='=')
			s++;
		if (alias.s==s || *s==0 || *s==';')
			goto parse_error;
		alias.len = s-alias.s;
		while(*s && isspace((int)*s))
			s++;
		/* equal sign */
		if (*s!='=')
			goto parse_error;
		s++;
		while(*s && isspace((int)*s))
			s++;
		/* avp name */
		name.s = s;
		while(*s && *s!=';' && !isspace((int)*s))
			s++;
		if (name.s==s)
			goto parse_error;
		name.len = s-name.s;
		while(*s && isspace((int)*s))
			s++;
		/* check end */
		if (*s!=0 && *s!=';')
			goto parse_error;
		if (*s==';') {
			for( s++ ; *s && isspace((int)*s) ; s++ );
			if (*s==0)
				goto parse_error;
		}

		if (parse_avp_name( &name, &type, &avp_name, &index)!=0) {
			LM_ERR("<%.*s> not a valid AVP name\n", name.len, name.s);
			goto error;
		}

		if (add_avp_galias( &alias, type, avp_name)!=0) {
			LM_ERR("add global alias failed\n");
			goto error;
		}
	} /*end while*/

	return 0;
parse_error:
	LM_ERR("parse error in <%s> around pos %ld\n",
		alias_definition, (long)(s-alias_definition));
error:
	return -1;
}


int destroy_avps(avp_flags_t flags, avp_name_t name, int all)
{
	struct search_state st;
	avp_t* avp;
	int n;
	
	n = 0;
	avp = search_first_avp(flags, name, 0, &st);
	while (avp) {
		destroy_avp(avp);
		n++;
		if (!all) break;
		avp = search_next_avp(&st, 0);
	}
	return n;
}


void delete_avp(avp_flags_t flags, avp_name_t name)
{
	struct search_state st;
	avp_t* avp;

	avp = search_first_avp(flags, name, 0, &st);
	while(avp) {
		destroy_avp(avp);
		avp = search_next_avp(&st, 0);
	}
}

/* AVP flags functions */

/* name2id conversion is intended to use during fixup (cfg parsing and modinit) only therefore no hash is used */
avp_flags_t register_avpflag(char* name) {
	avp_flags_t ret;
	ret = get_avpflag_no(name);
	if (ret == 0) {
		if (registered_avpflags_no >= MAX_AVPFLAG) {
			LM_ERR("cannot register new avp flag ('%s'), max.number of flags (%d) reached\n",
					name, MAX_AVPFLAG);
			return -1;
		}
		ret = 1<<(AVP_CUSTOM_FLAGS+registered_avpflags_no);
		registered_avpflags[registered_avpflags_no++] = name;
	}
	return ret;
}

avp_flags_t get_avpflag_no(char* name) {
	int i;
	for (i=0; i<registered_avpflags_no; i++) {
		if (strcasecmp(name, registered_avpflags[i])==0)
			return 1<<(AVP_CUSTOM_FLAGS+i);
	}
	return 0;
}
