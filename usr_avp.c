/*
 * $Id$
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
 *  2004-02-06  created (bogdan)
 *  2004-06-06  updated to the current DB api (andrei)
 */


#include "sr_module.h"
#include "dprint.h"
#include "str.h"
#include "ut.h"
#include "mem/mem.h"
#include "db/db.h"
#include "parser/parse_from.h"
#include "parser/parse_uri.h"
#include "usr_avp.h"



static db_con_t  *avp_db_con = 0;
static db_func_t avp_dbf; /* database call backs */
struct usr_avp   *users_avps = 0;
char             *avp_db_url = 0;


static char* usr_type[] = {"ruri","from","to",0};


int init_avp_child( int rank )
{
	if ( rank>PROC_MAIN ) {
		if (avp_db_url==0) {
			LOG(L_NOTICE,"NOTICE:init_avp_child: no avp_db_url specified "
				"-> feature disabled\n");
			return 0;
		}
		/* init db connection */
		if ( bind_dbmod(avp_db_url, &avp_dbf) < 0 ) {
			LOG(L_ERR,"ERROR:init_avp_child: unable to find any db module\n");
			return -1;
		}
		if ( (avp_db_con=avp_dbf.init( avp_db_url ))==0) {
			/* connection failed */
			LOG(L_ERR,"ERROR:init_avp_child: unable to connect to database\n");
			return -1;
		}
		if (avp_dbf.use_table( avp_db_con, AVP_DB_TABLE )<0) {
			/* table selection failed */
			LOG(L_ERR,"ERROR:init_avp_child: unable to select db table\n");
			return -1;
		}
	}

	return 0;
}


void print_avps(struct usr_avp *avp)
{
	if (!avp)
		return;
	if (avp->val_type==AVP_TYPE_STR)
		DBG("DEBUG:print_avp: %.*s=%.*s\n",
			avp->attr.len,avp->attr.s,
			avp->val.str_val.len,avp->val.str_val.s);
	else
		DBG("DEBUG:print_avp: %.*s=%u\n",
			avp->attr.len,avp->attr.s,
			avp->val.uint_val);
	print_avps(avp->next);
}


void destroy_avps( )
{
	struct usr_avp *avp;

	/*print_avps(users_avps);*/
	while (users_avps) {
		avp = users_avps;
		users_avps = users_avps->next;
		pkg_free( avp );
	}
}



int get_user_type( char *id )
{
	int i;

	for(i=0;usr_type[i];i++) {
		if (!strcasecmp( id, usr_type[i]) )
			return i;
	}

	LOG(L_ERR,"ERROR:avp:get_user_type: unknown user type <%s>\n",id);
	return -1;
}



inline static unsigned int compute_ID( str *attr )
{
	char *p;
	unsigned int id;

	id=0;
	for( p=attr->s+attr->len-1 ; p>=attr->s ; p-- )
		id ^= *p;
	return id;
}


inline static db_res_t *do_db_query(struct sip_uri *uri,char *attr,int use_dom)
{
	static db_key_t   keys_cmp[3] = {"username","domain","attribute"};
	static db_key_t   keys_ret[] = {"attribute","value","type"};
	static db_val_t   vals_cmp[3];
	unsigned int      nr_keys_cmp;
	db_res_t          *res;

	/* prepare DB query */
	nr_keys_cmp = 0;
	keys_cmp[ nr_keys_cmp ] = "username";
	vals_cmp[ nr_keys_cmp ].type = DB_STR;
	vals_cmp[ nr_keys_cmp ].nul  = 0;
	vals_cmp[ nr_keys_cmp ].val.str_val = uri->user;
	nr_keys_cmp++;
	if (use_dom) {
		keys_cmp[ nr_keys_cmp ] = "domain";
		vals_cmp[ nr_keys_cmp ].type = DB_STR;
		vals_cmp[ nr_keys_cmp ].nul  = 0;
		vals_cmp[ nr_keys_cmp ].val.str_val = uri->host;
		nr_keys_cmp++;
	}
	if (attr) {
		keys_cmp[ nr_keys_cmp ] = "attribute";
		vals_cmp[ nr_keys_cmp ].type = DB_STRING;
		vals_cmp[ nr_keys_cmp ].nul  = 0;
		vals_cmp[ nr_keys_cmp ].val.string_val = attr;
		nr_keys_cmp++;
	}

	/* do the DB query */
	if ( avp_dbf.query( avp_db_con, keys_cmp, 0/*op*/, vals_cmp, keys_ret,
	nr_keys_cmp, 3, 0/*order*/, &res) < 0)
		return 0;

	return res;
}



inline static int validate_db_row(struct db_row *row, unsigned int *val_type,
													unsigned int *uint_val)
{
	/* we don't accept null values */
	if (row->values[0].nul || row->values[1].nul || row->values[2].nul ) {
		LOG(L_ERR,"ERROR:avp:validat_db_row: DBreply contains NULL entryes\n");
		return -1;
	}
	/* check the value types */
	if ( (row->values[0].type!=DB_STRING && row->values[0].type!=DB_STR)
	||  (row->values[1].type!=DB_STRING && row->values[1].type!=DB_STR)
	|| row->values[2].type!=DB_INT ) {
		LOG(L_ERR,"ERROR:avp:validat_db_row: bad DB types in response\n");
		return -1;
	}
	/* check the content of TYPE filed */
	*val_type = (unsigned int)row->values[2].val.int_val;
	if (*val_type!=AVP_TYPE_INT && *val_type!=AVP_TYPE_STR) {
		LOG(L_ERR,"ERROR:avp:validat_db_row: bad val %d in type field\n",
			*val_type);
		return -1;
	}
	/* convert from DB_STRING to DB_STR if necesary */
	if (row->values[0].type==DB_STRING) {
		row->values[0].val.str_val.s =  (char*)row->values[0].val.string_val;
		row->values[0].val.str_val.len = strlen(row->values[0].val.str_val.s);
	}
	if (row->values[1].type==DB_STRING) {
		row->values[1].val.str_val.s =  (char*)row->values[1].val.string_val;
		row->values[1].val.str_val.len = strlen(row->values[1].val.str_val.s);
	}
	/* if type is INT decode the value */
	if ( *val_type==AVP_TYPE_INT &&
	str2int( &row->values[1].val.str_val, uint_val)==-1 ) {
		LOG(L_ERR,"ERROR:avp:validat_db_row: type is INT, but value not "
			"<%s>\n",row->values[1].val.str_val.s);
		return -1;
	}
	return 0;
}



#define copy_str(_p_,_sd_,_ss_) \
	do {\
		(_sd_).s = (_p_);\
		(_sd_).len = (_ss_).len;\
		memcpy( _p_, (_ss_).s, (_ss_).len);\
		(_p_) += (_ss_).len;\
	}while(0)
/*
 * Returns:   -1 : error
 *             0 : sucess and avp(s) loaded
 *             1 : sucess but no avp loaded
 */
int load_avp( struct sip_msg *msg, int uri_type, char *attr, int use_dom)
{
	db_res_t          *res;
	struct sip_uri    uri;
	struct usr_avp    *avp;
	str               *uri_s;
	int               n;
	unsigned int      val_type;
	unsigned int      uint_val;
	int               len;
	char              *p;

	/* featch the user name [and domain] */
	switch (uri_type) {
		case 0: /* RURI */
			uri_s = &(msg->first_line.u.request.uri);
			break;
		case 1: /* from */
			if (parse_from_header( msg )<0 ) {
				LOG(L_ERR,"ERROR:load_avp: failed to parse from\n");
				goto error;
			}
			uri_s = &(get_from(msg)->uri);
			break;
		case 2: /* to */
			if (parse_headers( msg, HDR_TO, 0)<0) {
				LOG(L_ERR,"ERROR:load_avp: failed to parse to\n");
				goto error;
			}
			uri_s = &(get_to(msg)->uri);
			break;
		default:
			LOG(L_CRIT,"BUG:load_avp: unknow username type <%d>\n",uri_type);
			goto error;
	}

	/* parse uri */
	if (parse_uri( uri_s->s, uri_s->len , &uri )<0) {
		LOG(L_ERR,"ERROR:load_avp: failed to parse uri\n");
		goto error;
	}

	/* check uri */
	if (!uri.user.s||!uri.user.len||(use_dom&&(!uri.host.len||!uri.host.s))) {
		LOG(L_ERR,"ERROR:load_avp: uri has no user/host part <%.*s>\n",
			uri_s->len,uri_s->s);
		goto error;
	}

	/* do DB query */
	if ( (res=do_db_query( &uri, attr,use_dom))==0 ) {
		LOG(L_ERR,"ERROR:load_avp: db_query failed\n");
		goto error;
	}

	/* process DB response */
	if (res->n==0) {
		DBG("DEBUG:load_avp: no avp found for %.*s@%.*s <%s>\n",
			uri.user.len,uri.user.s,(use_dom!=0)*uri.host.len,uri.host.s,
			attr?attr:"NULL");
		avp_dbf.free_query( avp_db_con, res);
		/*no avp found*/
		return 1;
	}

	for( n=0 ; n<res->n ; n++) {
		/* validate row */
		if (validate_db_row( &res->rows[n] ,&val_type, &uint_val) < 0 )
			continue;
		/* what do we have here?! */
		DBG("DEBUG:load_avp: found avp: <%s,%s,%d>\n",
			res->rows[n].values[0].val.string_val,
			res->rows[n].values[1].val.string_val,
		res->rows[n].values[2].val.int_val);
		/* build a new avp struct */
		len = sizeof(struct usr_avp);
		len += res->rows[n].values[0].val.str_val.len ;
		if (val_type==AVP_TYPE_STR)
			len += res->rows[n].values[1].val.str_val.len ;
		avp = (struct usr_avp*)pkg_malloc( len );
		if (avp==0) {
			LOG(L_ERR,"ERROR:load_avp: no more pkg mem\n");
			continue;
		}
		/* fill the structure in */
		p = ((char*)avp) + sizeof(struct usr_avp);
		avp->id = compute_ID( &res->rows[n].values[0].val.str_val );
		avp->val_type = val_type;
		/* attribute name */
		copy_str( p, avp->attr, res->rows[n].values[0].val.str_val);
		if (val_type==AVP_TYPE_INT) {
			/* INT */
			avp->val.uint_val = uint_val;
		} else {
			/* STRING */
			copy_str( p, avp->val.str_val,
				res->rows[n].values[1].val.str_val);
		}
		/* add avp to internal list */
		avp->next = users_avps;
		users_avps = avp;
	}

	avp_dbf.free_query( avp_db_con, res);
	return 0;
error:
	return -1;
}



inline static struct usr_avp *internal_search_avp( unsigned int id, str *attr)
{
	struct usr_avp *avp;

	for( avp=users_avps ; avp ; avp=avp->next )
		if ( id==avp->id 
		&& attr->len==avp->attr.len
		&& !strncasecmp( attr->s, avp->attr.s, attr->len)
		) {
			return avp;
		}
	return 0;
}



struct usr_avp *search_avp( str *attr)
{
	return internal_search_avp( compute_ID( attr ), attr);
}



struct usr_avp *search_next_avp( struct usr_avp *avp )
{
	return internal_search_avp( avp->id, &avp->attr);
}

