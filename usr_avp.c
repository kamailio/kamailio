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
 *  2004-10-09  interface more flexibil - more function available (bogdan)
 */


#include <assert.h>

#include "sr_module.h"
#include "dprint.h"
#include "str.h"
#include "ut.h"
#include "mem/shm_mem.h"
#include "usr_avp.h"



struct str_int_data {
	str  name;
	int  val;
};

struct str_str_data {
	str  name;
	str  val;
};


static struct usr_avp *global_avps = 0;
static struct usr_avp **crt_avps  = &global_avps;



inline static unsigned short compute_ID( str *name )
{
	char *p;
	unsigned short id;

	id=0;
	for( p=name->s+name->len-1 ; p>=name->s ; p-- )
		id ^= *p;
	return id;
}


int add_avp(unsigned short flags, int_str name, int_str val)
{
	struct usr_avp *avp;
	str *s;
	struct str_int_data *sid;
	struct str_str_data *ssd;
	int len;

	assert( crt_avps!=0 );

	/* compute the required mem size */
	len = sizeof(struct usr_avp);
	if (flags&AVP_NAME_STR) {
		if (flags&AVP_VAL_STR)
			len += sizeof(struct str_str_data)-sizeof(void*) + name.s->len
				+ val.s->len;
		else
			len += sizeof(struct str_int_data)-sizeof(void*) + name.s->len;
	} else if (flags&AVP_VAL_STR)
			len += sizeof(str)-sizeof(void*) + val.s->len;

	avp = (struct usr_avp*)shm_malloc( len );
	if (avp==0) {
		LOG(L_ERR,"ERROR:avp:add_avp: no more shm mem\n");
		goto error;
	}

	avp->flags = flags;
	avp->id = (flags&AVP_NAME_STR)? compute_ID(name.s) : name.n ;

	avp->next = *crt_avps;
	*crt_avps = avp;

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
			sid->name.len =name.s->len;
			sid->name.s = (char*)sid + sizeof(struct str_int_data);
			memcpy( sid->name.s , name.s->s, name.s->len);
			break;
		case AVP_VAL_STR:
			/* avp type ID, str value */
			s = (str*)&(avp->data);
			s->len = val.s->len;
			s->s = (char*)s + sizeof(str);
			memcpy( s->s, val.s->s , s->len);
			break;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			ssd = (struct str_str_data*)&(avp->data);
			ssd->name.len = name.s->len;
			ssd->name.s = (char*)ssd + sizeof(struct str_str_data);
			memcpy( ssd->name.s , name.s->s, name.s->len);
			ssd->val.len = val.s->len;
			ssd->val.s = ssd->name.s + ssd->name.len;
			memcpy( ssd->val.s , val.s->s, val.s->len);
			break;
	}

	return 0;
error:
	return -1;
}


/* get value functions */

inline str* get_avp_name(struct usr_avp *avp)
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


inline void get_avp_val(struct usr_avp *avp, int_str *val)
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
			val->s = (str*)(&avp->data);
			break;
		case AVP_NAME_STR|AVP_VAL_STR:
			/* avp type str, str value */
			val->s = &(((struct str_str_data*)(&avp->data))->val);
			break;
	}
}


struct usr_avp** get_avp_list( )
{
	assert( crt_avps!=0 );
	return crt_avps;
}




/* search functions */

inline static struct usr_avp *internal_search_ID_avp( struct usr_avp *avp,
												unsigned short id)
{
	for( ; avp ; avp=avp->next ) {
		if ( id==avp->id && (avp->flags&AVP_NAME_STR)==0  ) {
			return avp;
		}
	}
	return 0;
}



inline static struct usr_avp *internal_search_name_avp( struct usr_avp *avp,
												unsigned short id, str *name)
{
	str * avp_name;

	for( ; avp ; avp=avp->next )
		if ( id==avp->id && avp->flags&AVP_NAME_STR &&
		(avp_name=get_avp_name(avp))!=0 && avp_name->len==name->len
		 && !strncasecmp( avp_name->s, name->s, name->len) ) {
			return avp;
		}
	return 0;
}



struct usr_avp *search_first_avp( unsigned short name_type,
										int_str name, int_str *val)
{
	struct usr_avp *avp;

	assert( crt_avps!=0 );
	
	if (*crt_avps==0)
		return 0;

	/* search for the AVP by ID (&name) */
	if (name_type&AVP_NAME_STR)
		avp = internal_search_name_avp(*crt_avps,compute_ID(name.s),name.s);
	else
		avp = internal_search_ID_avp( *crt_avps, name.n );

	/* get the value - if required */
	if (avp && val)
		get_avp_val(avp, val);

	return avp;
}



struct usr_avp *search_next_avp( struct usr_avp *avp,  int_str *val )
{
	if (avp==0 || (avp=avp->next)==0)
		return 0;

	if (avp->flags&AVP_NAME_STR)
		avp = internal_search_name_avp( avp, avp->id, get_avp_name(avp));
	else
		avp = internal_search_ID_avp( avp, avp->id );

	if (avp && val)
		get_avp_val(avp, val);

	return avp;
}




/********* free functions ********/

void destroy_avp( struct usr_avp *avp_del)
{
	struct usr_avp *avp;
	struct usr_avp *avp_prev;

	for( avp_prev=0,avp=*crt_avps ; avp ; avp_prev=avp,avp=avp->next ) {
		if (avp==avp_del) {
			if (avp_prev)
				avp_prev->next=avp->next;
			else
				*crt_avps = avp->next;
			shm_free(avp);
			return;
		}
	}
}


void destroy_avp_list_unsafe( struct usr_avp **list )
{
	struct usr_avp *avp, *foo;

	avp = *list;
	while( avp ) {
		foo = avp;
		avp = avp->next;
		shm_free_unsafe( foo );
	}
	*list = 0;
}


inline void destroy_avp_list( struct usr_avp **list )
{
	struct usr_avp *avp, *foo;

	DBG("DEBUG:destroy_avp_list: destroying list %p\n",*list);
	avp = *list;
	while( avp ) {
		foo = avp;
		avp = avp->next;
		shm_free( foo );
	}
	*list = 0;
}


void reset_avps( )
{
	assert( crt_avps!=0 );
	
	if ( crt_avps!=&global_avps) {
		crt_avps = &global_avps;
	}
	destroy_avp_list( crt_avps );
}



struct usr_avp** set_avp_list( struct usr_avp **list )
{
	struct usr_avp **foo;
	
	assert( crt_avps!=0 );

	foo = crt_avps;
	crt_avps = list;
	return foo;
}

