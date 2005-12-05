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
 *  2004-10-09  interface more flexible - more function available (bogdan)
 *  2004-11-07  AVP string values are kept 0 terminated (bogdan)
 *  2004-11-14  global aliases support added
 */


#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "sr_module.h"
#include "dprint.h"
#include "str.h"
#include "ut.h"
#include "mem/shm_mem.h"
#include "mem/mem.h"
#include "usr_avp.h"

enum idx {
	IDX_FROM_USER = 0,
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
		LOG(L_ERR, "ERROR: No memory to allocate default global AVP list\n");
		return -1;
	}
	*def_glist = 0;
	*crt_glist = def_glist;
	return 0;
}


/*
 * Select active AVP list based on the value of flags
 */
static avp_list_t* select_list(unsigned short flags)
{
	if (flags & AVP_CLASS_USER) {
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
	}

	return *crt_glist;
}

inline static unsigned short compute_ID( str *name )
{
	char *p;
	unsigned short id;

	id=0;
	for( p=name->s+name->len-1 ; p>=name->s ; p-- )
		id ^= *p;
	return id;
}


int add_avp_list(avp_list_t* list, unsigned short flags, int_str name, int_str val)
{
	avp_t *avp;
	str *s;
	struct str_int_data *sid;
	struct str_str_data *ssd;
	int len;

	assert(list != 0);

	if (name.s.s == 0 && name.s.len == 0) {
		LOG(L_ERR,"ERROR:avp:add_avp: 0 ID or NULL NAME AVP!");
		goto error;
	}

	/* compute the required mem size */
	len = sizeof(struct usr_avp);
	if (flags&AVP_NAME_STR) {
		if ( name.s.s==0 || name.s.len==0) {
			LOG(L_ERR,"ERROR:avp:add_avp: EMPTY NAME AVP!");
			goto error;
		}
		if (flags&AVP_VAL_STR) {
			len += sizeof(struct str_str_data)-sizeof(void*) 
				+ name.s.len + 1 /* Terminating zero for regex search */
				+ val.s.len + 1; /* Value is zero terminated */
		} else {
			len += sizeof(struct str_int_data)-sizeof(void*) 
				+ name.s.len + 1; /* Terminating zero for regex search */
		}
	} else if (flags&AVP_VAL_STR) {
		len += sizeof(str)-sizeof(void*) + val.s.len + 1;
	}

	avp = (struct usr_avp*)shm_malloc( len );
	if (avp==0) {
		LOG(L_ERR,"ERROR:avp:add_avp: no more shm mem\n");
		goto error;
	}

	avp->flags = flags;
	avp->id = (flags&AVP_NAME_STR)? compute_ID(&name.s) : name.n ;

	avp->next = *list;
	*list = avp;

	switch ( flags&(AVP_NAME_STR|AVP_VAL_STR) )
	{
		case 0:
			/* avp type ID, int value */
			avp->data = (void*)(long)val.n;
			break;
		case AVP_NAME_STR:
			/* avp type str, int value */
			sid = (struct str_int_data*)&(avp->data);
			sid->val = val.n;
			sid->name.len =name.s.len;
			sid->name.s = (char*)sid + sizeof(struct str_int_data);
			memcpy( sid->name.s , name.s.s, name.s.len);
			sid->name.s[name.s.len] = '\0'; /* Zero terminator */
			break;
		case AVP_VAL_STR:
			/* avp type ID, str value */
			s = (str*)&(avp->data);
			s->len = val.s.len;
			s->s = (char*)s + sizeof(str);
			memcpy( s->s, val.s.s , s->len);
			s->s[s->len] = 0;
			break;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			ssd = (struct str_str_data*)&(avp->data);
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

	return 0;
error:
	return -1;
}


int add_avp(unsigned short flags, int_str name, int_str val)
{
	unsigned short avp_class;
	avp_list_t* list;

	     /* Add avp to user class if no class has been
	      * specified by the caller
	      */
	if ((flags & AVP_CLASS_ALL) == 0) flags |= AVP_CLASS_USER;
	if ((flags & AVP_TRACK_ALL) == 0) flags |= AVP_TRACK_FROM;
	list = select_list(flags);

	if (flags & AVP_CLASS_USER) avp_class = AVP_CLASS_USER;
	else if (flags & AVP_CLASS_DOMAIN) avp_class = AVP_CLASS_DOMAIN;
	else avp_class = AVP_CLASS_GLOBAL;

	     /* Make that only the selected class is set
	      * if the caller set more classes in flags
	      */
	return add_avp_list(list, flags & (~(AVP_CLASS_ALL) | avp_class), name, val);
}


/* get value functions */
inline str* get_avp_name(avp_t *avp)
{
	switch ( avp->flags&(AVP_NAME_STR|AVP_VAL_STR) )
	{
		case 0:
			/* avp type ID, int value */
		case AVP_VAL_STR:
			/* avp type ID, str value */
			return 0;
		case AVP_NAME_STR:
			/* avp type str, int value */
			return &((struct str_int_data*)&avp->data)->name;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			return &((struct str_str_data*)&avp->data)->name;
	}

	LOG(L_ERR,"BUG:avp:get_avp_name: unknown avp type (name&val) %d\n",
	    avp->flags&(AVP_NAME_STR|AVP_VAL_STR));
	return 0;
}


inline void get_avp_val(avp_t *avp, int_str *val)
{
	if (avp==0 || val==0)
		return;

	switch ( avp->flags&(AVP_NAME_STR|AVP_VAL_STR) ) {
		case 0:
			/* avp type ID, int value */
			val->n = (long)(avp->data);
			break;
		case AVP_NAME_STR:
			/* avp type str, int value */
			val->n = ((struct str_int_data*)(&avp->data))->val;
			break;
		case AVP_VAL_STR:
			/* avp type ID, str value */
			val->s = *(str*)(avp->data);
			break;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			val->s = (((struct str_str_data*)(&avp->data))->val);
			break;
	}
}


/* Return the current list of user attributes */
avp_list_t get_avp_list(unsigned short flags)
{
	return *select_list(flags);
}


/*
 * Compare given id with id in avp, return true if they match
 */
static inline int match_by_id(avp_t* avp, unsigned short id)
{
	if (avp->id == id && (avp->flags&AVP_NAME_STR)==0) {
		return 1;
	}
	return 0;
}


/*
 * Compare given name with name in avp, return true if they are same
 */
static inline int match_by_name(avp_t* avp, unsigned short id, str* name)
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


avp_t *search_first_avp(unsigned short flags, int_str name, int_str *val, struct search_state* s)
{
	avp_t* ret;
	static struct search_state st;
	avp_list_t* list;

	if (name.s.s==0 && name.s.len == 0) {
		LOG(L_ERR,"ERROR:avp:search_first_avp: 0 ID or NULL NAME AVP!");
		return 0;
	}

	if (!s) s = &st;

	if ((flags & AVP_CLASS_ALL) == 0) {
		     /* The caller did not specify any class to search in, so enable
		      * all of them by default
		      */
		flags |= AVP_CLASS_ALL;
	}

	list = select_list(flags);

	s->flags = flags;
	s->avp = *list;
	s->name = name;

	if (flags & AVP_NAME_STR) {
		s->id = compute_ID(&name.s);
	}

        ret = search_next_avp(s, val);

	     /* Make sure that search next avp stays in the same class as the first
	      * avp found
	      */
	if (s && ret) s->flags = (flags & ~AVP_CLASS_ALL) | (ret->flags & AVP_CLASS_ALL);
	return ret;
}



avp_t *search_next_avp(struct search_state* s, int_str *val )
{
	int matched;
	avp_t* avp;

	if (s == 0) {
		LOG(L_ERR, "search_next:avp: Invalid parameter value\n");
		return 0;
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

		if (s->flags & AVP_CLASS_USER) {
			s->flags &= ~AVP_CLASS_USER;
			s->avp = *select_list(s->flags);
		} else if (s->flags & AVP_CLASS_DOMAIN) {
			s->flags &= ~AVP_CLASS_DOMAIN;
			s->avp = *select_list(s->flags);
		} else {
			s->flags &= ~AVP_CLASS_GLOBAL;
			return 0;
		}
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

	DBG("DEBUG:destroy_avp_list: destroying list %p\n", *list);
	avp = *list;
	while( avp ) {
		foo = avp;
		avp = avp->next;
		shm_free( foo );
	}
	*list = 0;
}


void reset_avps(void)
{
	int i;
	for(i = 0; i < IDX_MAX; i++) {
		crt_list[i] = &def_list[i];
		destroy_avp_list(crt_list[i]);
	}
}


avp_list_t* set_avp_list( unsigned short flags, avp_list_t* list )
{
	avp_list_t* prev;

	if (flags & AVP_CLASS_USER) {
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
		LOG(L_ERR, "ERROR:add_avp_galias: null params received\n");
		goto error;
	}

	if (check_avp_galias(alias,type,avp_name)!=0) {
		LOG(L_ERR, "ERROR:add_avp_galias: duplicate alias/avp entry\n");
		goto error;
	}

	ga = (struct avp_galias*)pkg_malloc( sizeof(struct avp_galias) );
	if (ga==0) {
		LOG(L_ERR, "ERROR:add_avp_galias: no more pkg memory\n");
		goto error;
	}

	ga->alias.s = (char*)pkg_malloc( alias->len+1 );
	if (ga->alias.s==0) {
		LOG(L_ERR, "ERROR:add_avp_galias: no more pkg memory\n");
		goto error1;
	}
	memcpy( ga->alias.s, alias->s, alias->len);
	ga->alias.len = alias->len;

	ga->avp.type = type&AVP_NAME_STR;

	if (type&AVP_NAME_STR) {
		ga->avp.name.s.s = (char*)pkg_malloc( avp_name.s.len+1 );
		if (ga->avp.name.s.s==0) {
			LOG(L_ERR, "ERROR:add_avp_galias: no more pkg memory\n");
			goto error2;
		}
		ga->avp.name.s.len = avp_name.s.len;
		memcpy( ga->avp.name.s.s, avp_name.s.s, avp_name.s.len);
		ga->avp.name.s.s[avp_name.s.len] = 0;
		DBG("DEBUG:add_avp_galias: registering <%s> for avp name <%s>\n",
			ga->alias.s, ga->avp.name.s.s);
	} else {
		ga->avp.name.n = avp_name.n;
		DBG("DEBUG:add_avp_galias: registering <%s> for avp id <%d>\n",
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

int parse_avp_name( str *name, int *type, int_str *avp_name)
{
	unsigned int id;
	char c;

	if (name==0 || name->s==0 || name->len==0)
		goto error;

	if (name->len>=2 && name->s[1]==':') {
		c = name->s[0];
		name->s += 2;
		name->len -= 2;
		if (name->len==0)
			goto error;
		switch (c) {
			case 's': case 'S':
				*type = AVP_NAME_STR;
				avp_name->s = *name;
				break;
			case 'i': case 'I':
				*type = 0;
				if (str2int( name, &id)!=0) {
					LOG(L_ERR, "ERROR:parse_avp_name: invalid ID "
						"<%.*s> - not a number\n", name->len, name->s);
					goto error;
				}
				avp_name->n = (int)id;
				break;
			default:
				LOG(L_ERR, "ERROR:parse_avp_name: unsupported type "
					"[%c]\n", c);
				goto error;
		}
	} else {
		/*default is string name*/
		*type = AVP_NAME_STR;
		avp_name->s = *name;
	}

	return 0;
error:
	return -1;
}


int parse_avp_spec( str *name, int *type, int_str *avp_name)
{
	str alias;

	if (name==0 || name->s==0 || name->len==0)
		return -1;

	if (name->s[0]==GALIAS_CHAR_MARKER) {
		/* it's an avp alias */
		if (name->len==1) {
			LOG(L_ERR,"ERROR:parse_avp_spec: empty alias\n");
			return -1;
		}
		alias.s = name->s+1;
		alias.len = name->len-1;
		return lookup_avp_galias( &alias, type, avp_name);
	} else {
		return parse_avp_name( name, type, avp_name);
	}
}


int add_avp_galias_str(char *alias_definition)
{
	int_str avp_name;
	char *s;
	str  name;
	str  alias;
	int  type;

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

		if (parse_avp_name( &name, &type, &avp_name)!=0) {
			LOG(L_ERR, "ERROR:add_avp_galias_str: <%.*s> not a valid AVP "
				"name\n", name.len, name.s);
			goto error;
		}

		if (add_avp_galias( &alias, type, avp_name)!=0) {
			LOG(L_ERR, "ERROR:add_avp_galias_str: add global alias failed\n");
			goto error;
		}
	} /*end while*/

	return 0;
parse_error:
	LOG(L_ERR, "ERROR:add_avp_galias_str: parse error in <%s> around "
		"pos %ld\n", alias_definition, (long)(s-alias_definition));
error:
	return -1;
}


void delete_avp(unsigned short flags, int_str name)
{
	struct search_state st;
	avp_t* avp;

	avp = search_first_avp(flags, name, 0, &st);
	while(avp) {
		destroy_avp(avp);
		avp = search_next_avp(&st, 0);
	}
}
