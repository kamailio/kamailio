/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * AVPOPS SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * AVPOPS SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2004-10-04  first version (ramona)
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "../../ut.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../action.h"
#include "../../ip_addr.h"
#include "../../config.h"
#include "../../dset.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../mem/mem.h"
#include "avpops_impl.h"
#include "avpops_db.h"


static db_key_t  store_keys[6];
static db_val_t  store_vals[6];
static str      empty={"",0};


void init_store_avps( char **db_columns)
{
	/* unique user id */
	store_keys[0] = db_columns[0]; /*uuid*/
	store_vals[0].type = DB_STR;
	store_vals[0].nul  = 0;
	/* attribute */
	store_keys[1] = db_columns[1]; /*attribute*/
	store_vals[1].type = DB_STR;
	store_vals[1].nul  = 0;
	/* value */
	store_keys[2] = db_columns[2]; /*value*/
	store_vals[2].type = DB_STR;
	store_vals[2].nul  = 0;
	/* type */
	store_keys[3] = db_columns[3]; /*type*/
	store_vals[3].type = DB_INT;
	store_vals[3].nul  = 0;
	/* user name */
	store_keys[4] = db_columns[4]; /*username*/
	store_vals[4].type = DB_STR;
	store_vals[4].nul  = 0;
	/* domain */
	store_keys[5] = db_columns[5]; /*domain*/
	store_vals[5].type = DB_STR;
	store_vals[5].nul  = 0;
}


static int dbrow2avp(struct db_row *row , int flags, int_str attr)
{
	unsigned int uint;
	int  db_flags;
	str  atmp;
	str  vtmp;
	int_str avp_attr;
	int_str avp_val;

	/* check for null fields into the row */
	if (row->values[0].nul || row->values[1].nul || row->values[2].nul )
	{
		LOG( L_ERR, "ERROR:avpops:dbrow2avp: dbrow contains NULL fields\n");
		return -1;
	}

	/* check the value types */
	if ( (row->values[0].type!=DB_STRING && row->values[0].type!=DB_STR)
		||  (row->values[1].type!=DB_STRING && row->values[1].type!=DB_STR)
		|| row->values[2].type!=DB_INT )
	{
		LOG(L_ERR,"ERROR:avpops:dbrow2avp: wrong field types in dbrow\n");
		return -1;
	}

	/* check the content of flag field */
	uint = (unsigned int)row->values[2].val.int_val;
	db_flags = ((uint&AVPOPS_DB_NAME_INT)?0:AVP_NAME_STR) |
		((uint&AVPOPS_DB_VAL_INT)?0:AVP_VAL_STR);

	DBG("db_flags=%d, flags=%d\n",db_flags,flags);
	/* does the avp type match ? */
	if(!((flags&(AVPOPS_VAL_INT|AVPOPS_VAL_STR))==0 ||
			((flags&AVPOPS_VAL_INT)&&((db_flags&AVP_NAME_STR)==0)) ||
			((flags&AVPOPS_VAL_STR)&&(db_flags&AVP_NAME_STR))))
		return -2;

	/* is the avp name already known? */
	if ( (flags&AVPOPS_VAL_NONE)==0 )
	{
		/* use the name  */
		avp_attr = attr;
	} else {
		/* take the name from db response */
		if (row->values[0].type==DB_STRING)
		{
			atmp.s = (char*)row->values[0].val.string_val;
			atmp.len = strlen(atmp.s);
		} else {
			atmp = row->values[0].val.str_val;
		}
		if (db_flags&AVP_NAME_STR)
		{
			/* name is string */
			avp_attr.s = &atmp;
		} else {
			/* name is ID */
			if (str2int( &atmp, &uint)==-1)
			{
				LOG(L_ERR,"ERROR:avpops:dbrow2avp: name is not ID as flags say"
					"<%s>\n", atmp.s);
				return -1;
			}
			avp_attr.n = (int)uint;
		}
	}

	/* now get the value as correct type */
	if (row->values[1].type==DB_STRING)
	{
		vtmp.s = (char*)row->values[1].val.string_val;
		vtmp.len = strlen(vtmp.s);
	} else {
		vtmp = row->values[1].val.str_val;
	}
	if (db_flags&AVP_VAL_STR)
	{
		/* value is string */
		avp_val.s = &vtmp;
	} else {
		/* name is ID */
		if (str2int(&vtmp, &uint)==-1)
		{
			LOG(L_ERR,"ERROR:avpops:dbrow2avp: value is not int as flags say"
				"<%s>\n", vtmp.s);
			return -1;
		}
		avp_val.n = (int)uint;
	}

	/* added the avp */
	db_flags |= AVP_IS_IN_DB;
	return add_avp( (unsigned short)db_flags, avp_attr, avp_val);
}


inline static str* get_source_uri(struct sip_msg* msg,int source)
{
	/* which uri will be used? */
	if (source&AVPOPS_USE_FROM)
	{ /* from */
		if (parse_from_header( msg )<0 )
		{
			LOG(L_ERR,"ERROR:avpops:get_source_uri: failed "
				"to parse from\n");
			goto error;
		}
		return &(get_from(msg)->uri);
	} else if (source&AVPOPS_USE_TO)
	{  /* to */
		if (parse_headers( msg, HDR_TO, 0)<0)
		{
			LOG(L_ERR,"ERROR:avpops:get_source_uri: failed "
				"to parse to\n");
			goto error;
		}
		return &(get_to(msg)->uri);
	} else if (source&AVPOPS_USE_RURI) {  /* RURI */
		if(msg->new_uri.s!=NULL && msg->new_uri.len>0)
			return &(msg->new_uri);
		return &(msg->first_line.u.request.uri);
	} else {
		LOG(L_CRIT,"BUG:avpops:get_source_uri: unknow source <%d>\n",
			source);
		goto error;
	}
error:
	return 0;
}


static int parse_source_uri(struct sip_msg* msg,int source,struct sip_uri *uri)
{
	str *uri_s;

	/* get uri */
	if ( (uri_s=get_source_uri(msg,source))==0 )
	{
		LOG(L_ERR,"ERROR:avpops:parse_source_uri: cannot get uri\n");
		goto error;
	}

	/* parse uri */
	if (parse_uri(uri_s->s, uri_s->len , uri)<0)
	{
		LOG(L_ERR,"ERROR:avpops:parse_source_uri: failed to parse uri\n");
		goto error;
	}

	/* check uri */
	if (!uri->user.s || !uri->user.len || !uri->host.len || !uri->host.s )
	{
		LOG(L_ERR,"ERROR:avpops:parse_source_uri: incomplet uri <%.*s>\n",
			uri_s->len,uri_s->s);
		goto error;
	}

	return 0;
error:
	return -1;
}


static inline void int_str2db_val( int_str is_val, str *val, int is_s)
{
	if (is_s)
	{
		/* val is string */
		*val = *is_val.s;
	} else {
		/* val is integer */
		val->s =
			int2str((unsigned long)is_val.n, &val->len);
	}
}


static int get_avp_as_str(struct fis_param *ap ,str *val)
{
	struct usr_avp  *avp;
	unsigned short  name_type;
	int_str         avp_val;

	name_type = ((ap->flags&AVPOPS_VAL_INT)?0:AVP_NAME_STR);
	avp = search_first_avp( name_type, ap->val, &avp_val);
	if (avp==0)
	{
		DBG("DEBUG:avpops:get_val_as_str: no avp found\n");
		return -1;
	}
	int_str2db_val( avp_val, val, avp->flags&AVP_VAL_STR);
	return 0;
}


int ops_dbload_avps (struct sip_msg* msg, struct fis_param *sp,
									struct db_param *dbp, int use_domain)
{
	struct sip_uri  uri;
	db_res_t       *res;
	str            uuid;
	int  i, n;

	if (sp->flags&AVPOPS_VAL_NONE)
	{
		/* get and parse uri */
		if (parse_source_uri( msg, sp->flags, &uri)<0 )
		{
			LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uri\n");
			goto error;
		}
		/* do DB query */
		res = db_load_avp( 0, (sp->flags&AVPOPS_FLAG_DOMAIN)?&empty:&uri.user,
				(use_domain||(sp->flags&AVPOPS_FLAG_DOMAIN))?&uri.host:0,
				dbp->sa.s, dbp->table.s);
	} else if (sp->flags&AVPOPS_VAL_AVP) {
		/* get uuid from avp */
		if (get_avp_as_str( sp, &uuid)<0)
		{
			LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uuid\n");
			goto error;
		}
		/* do DB query */
		res = db_load_avp( &uuid, 0, 0, dbp->sa.s, dbp->table.s);
	} else  if (sp->flags&AVPOPS_VAL_STR) {
		/* use the STR val as uuid */
		/* do DB query */
		res = db_load_avp( sp->val.s, 0, 0, dbp->sa.s, dbp->table.s);
	} else {
		LOG(L_CRIT,"BUG:avpops:load_avps: invalid flag combination (%d)\n",
			sp->flags);
		goto error;
	}

	/* res query ?  */
	if (res==0)
	{
		LOG(L_ERR,"ERROR:avpops:load_avps: db_load failed\n");
		goto error;
	}

	/* process the results */
	for( n=0,i=0 ; i<res->n ; i++)
	{
		/* validate row */
		if ( dbrow2avp( &res->rows[i], dbp->a.flags, dbp->a.val) < 0 )
			continue;
		n++;
	}

	db_close_query( res );

	DBG("DEBUG:avpops:load_avps: loaded avps = %d\n",n);

	return n?1:-1;
error:
	return -1;
}


int ops_dbstore_avps (struct sip_msg* msg, struct fis_param *sp,
									struct db_param *dbp, int use_domain)
{
	struct sip_uri   uri;
	struct usr_avp   **avp_list;
	struct usr_avp   *avp;
	unsigned short   name_type;
	int_str          i_s;
	str              uuid;
	int              keys_off;
	int              keys_nr;
	int              n;
	

	if (sp->flags&AVPOPS_VAL_NONE)
	{
		/* get and parse uri */
		if (parse_source_uri( msg, sp->flags, &uri)<0 )
		{
			LOG(L_ERR,"ERROR:avpops:store_avps: failed to get uri\n");
			goto error;
		}
		/* set values for keys  */
		keys_off = 1;
		store_vals[4].val.str_val =
			(sp->flags&AVPOPS_FLAG_DOMAIN)?empty:uri.user;
		if (use_domain || sp->flags&AVPOPS_FLAG_DOMAIN)
		{
			store_vals[5].val.str_val = uri.host;
			keys_nr = 5;
		} else {
			keys_nr = 4;
		}
	} else if (sp->flags&AVPOPS_VAL_AVP) {
		/* get uuid from avp */
		if (get_avp_as_str(sp, &uuid)<0)
		{
			LOG(L_ERR,"ERROR:avpops:store_avps: failed to get uuid\n");
			goto error;
		}
		/* set values for keys  */
		keys_off = 0;
		keys_nr = 4;
		store_vals[0].val.str_val = uuid;
	} else if (sp->flags&AVPOPS_VAL_STR) {
		/* use the STR value as uuid */
		/* set values for keys  */
		keys_off = 0;
		keys_nr = 4;
		store_vals[0].val.str_val = *sp->val.s;
	} else {
		LOG(L_CRIT,"BUG:avpops:store_avps: invalid flag combination (%d)\n",
			sp->flags);
		goto error;
	}

	/* set uuid/(username and domain) fields */
	n =0 ;

	if ((dbp->a.flags&AVPOPS_VAL_NONE)==0)
	{
		/* avp name is known ->set it and its type */
		name_type = (((dbp->a.flags&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		store_vals[1].val.str_val = dbp->sa; /*attr name*/
		avp = search_first_avp( name_type, dbp->a.val, &i_s);
		for( ; avp; avp=search_next_avp(avp,&i_s))
		{
			/* don't insert avps which were loaded */
			if (avp->flags&AVP_IS_IN_DB)
				continue;
			/* set type */
			store_vals[3].val.int_val =
				(avp->flags&AVP_NAME_STR?0:AVPOPS_DB_NAME_INT)|
				(avp->flags&AVP_VAL_STR?0:AVPOPS_DB_VAL_INT);
			/* set value */
			int_str2db_val( i_s, &store_vals[2].val.str_val,
				avp->flags&AVP_VAL_STR);
			/* save avp */
			if (db_store_avp( store_keys+keys_off, store_vals+keys_off,
					keys_nr, dbp->table.s)==0 )
			{
				avp->flags |= AVP_IS_IN_DB;
				n++;
			}
		}
	} else {
		/* avp name is unknown -> go through all list */
		avp_list = get_avp_list();
		avp = *avp_list;

		for ( ; avp ; avp=avp->next )
		{
			/* don't insert avps which were loaded */
			if (avp->flags&AVP_IS_IN_DB)
				continue;
			/* check if type match */
			if ( !( (dbp->a.flags&(AVPOPS_VAL_INT|AVPOPS_VAL_STR))==0 ||
				((dbp->a.flags&AVPOPS_VAL_INT)&&((avp->flags&AVP_NAME_STR))==0)
				||((dbp->a.flags&AVPOPS_VAL_STR)&&(avp->flags&AVP_NAME_STR))))
				continue;

			/* set attribute name and type */
			if ( (i_s.s=get_avp_name(avp))==0 )
				i_s.n = avp->id;
			int_str2db_val( i_s, &store_vals[1].val.str_val,
				avp->flags&AVP_NAME_STR);
			store_vals[3].val.int_val =
				(avp->flags&AVP_NAME_STR?0:AVPOPS_DB_NAME_INT)|
				(avp->flags&AVP_VAL_STR?0:AVPOPS_DB_VAL_INT);
			/* set avp value */
			get_avp_val( avp, &i_s);
			int_str2db_val( i_s, &store_vals[2].val.str_val,
				avp->flags&AVP_VAL_STR);
			/* save avp */
			if (db_store_avp( store_keys+keys_off, store_vals+keys_off,
			keys_nr, dbp->table.s)==0)
			{
				avp->flags |= AVP_IS_IN_DB;
				n++;
			}
		}
	}

	DBG("DEBUG:avpops:store_avps: %d avps were stored\n",n);

	return n==0?-1:1;
error:
	return -1;
}



int ops_dbdelete_avps (struct sip_msg* msg, struct fis_param *sp,
									struct db_param *dbp, int use_domain)
{
	struct sip_uri  uri;
	int             res;
	str             uuid;

	if (sp->flags&AVPOPS_VAL_NONE)
	{
		/* get and parse uri */
		if (parse_source_uri( msg, sp->flags, &uri)<0 )
		{
			LOG(L_ERR,"ERROR:avpops:dbdelete_avps: failed to get uri\n");
			goto error;
		}
		/* do DB delete */
		res = db_delete_avp(0, (sp->flags&AVPOPS_FLAG_DOMAIN)?&empty:&uri.user,
				(use_domain||(sp->flags&AVPOPS_FLAG_DOMAIN))?&uri.host:0,
				dbp->sa.s, dbp->table.s);
	} else if (sp->flags&AVPOPS_VAL_AVP) {
		/* get uuid from avp */
		if (get_avp_as_str( sp, &uuid)<0)
		{
			LOG(L_ERR,"ERROR:avpops:dbdelete_avps: failed to get uuid\n");
			goto error;
		}
		/* do DB delete */
		res = db_delete_avp( &uuid, 0, 0, dbp->sa.s, dbp->table.s);
	} else if (sp->flags&AVPOPS_VAL_STR) {
		/* use the STR value as uuid */
		/* do DB delete */
		res = db_delete_avp( sp->val.s, 0, 0, dbp->sa.s, dbp->table.s);
	} else {
		LOG(L_CRIT,"BUG:avpops:dbdelete_avps: invalid flag combination (%d)\n",
			sp->flags);
		goto error;
	}

	/* res ?  */
	if (res<0)
	{
		LOG(L_ERR,"ERROR:avpops:dbdelete_avps: db_delete failed\n");
		goto error;
	}

	return 1;
error:
	return -1;
}



int ops_write_avp(struct sip_msg* msg, struct fis_param *src,
													struct fis_param *ap)
{
	struct sip_uri uri;
	int_str avp_val;
	unsigned short flags;
	str s_ip;

	if (src->flags&AVPOPS_VAL_NONE)
	{
		if (src->flags&AVPOPS_USE_SRC_IP)
		{
			/* get data from src_ip */
			if ( (s_ip.s=ip_addr2a( &msg->rcv.src_ip ))==0)
			{
				LOG(L_ERR,"ERROR:avpops:write_avp: cannot get src_ip\n");
				goto error;
			}
			s_ip.len = strlen(s_ip.s);
			avp_val.s = &s_ip;
		} else {
			/* get data from uri (from,to,ruri) */
			if (src->flags&(AVPOPS_FLAG_USER|AVPOPS_FLAG_DOMAIN))
			{
				if (parse_source_uri( msg, src->flags, &uri)!=0 )
				{
					LOG(L_ERR,"ERROR:avpops:write_avp: cannot parse uri\n");
					goto error;
				}
				if (src->flags&AVPOPS_FLAG_DOMAIN)
					avp_val.s = &uri.host;
				else
					avp_val.s = &uri.user;
			} else {
				/* get whole uri */
				if ( (avp_val.s=get_source_uri(msg,src->flags))==0 )
				{
					LOG(L_ERR,"ERROR:avpops:write_avp: cannot get uri\n");
					goto error;
				}
			}
		}
		flags = AVP_VAL_STR;
	} else {
		avp_val = src->val;
		flags = (src->flags&AVPOPS_VAL_INT)?0:AVP_VAL_STR;
	}

	/* set the proper flag */
	flags |=  (ap->flags&AVPOPS_VAL_INT)?0:AVP_NAME_STR;

	/* added the avp */
	if (add_avp( flags, ap->val, avp_val)<0)
		goto error;

	return 1;
error:
	return -1;
}



int ops_delete_avp(struct sip_msg* msg, struct fis_param *ap)
{
	struct usr_avp **avp_list;
	struct usr_avp *avp;
	struct usr_avp *avp_next;
	unsigned short name_type;
	int n;

	n = 0;

	if ( (ap->flags&AVPOPS_VAL_NONE)==0)
	{
		/* avp name is known ->search by name */
		name_type = (((ap->flags&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		while ( (avp=search_first_avp( name_type, ap->val, 0))!=0 )
		{
			destroy_avp( avp );
			n++;
			if ( !(ap->flags&AVPOPS_FLAG_ALL) )
				break;
		}
	} else {
		/* avp name is not given - we have just flags */
		/* -> go through all list */
		avp_list = get_avp_list();
		avp = *avp_list;

		for ( ; avp ; avp=avp_next )
		{
			avp_next = avp->next;
			/* check if type match */
			if ( !( (ap->flags&(AVPOPS_VAL_INT|AVPOPS_VAL_STR))==0 ||
			((ap->flags&AVPOPS_VAL_INT)&&((avp->flags&AVP_NAME_STR))==0) ||
			((ap->flags&AVPOPS_VAL_STR)&&(avp->flags&AVP_NAME_STR)) )  )
				continue;
			/* remove avp */
			destroy_avp( avp );
			n++;
			if ( !(ap->flags&AVPOPS_FLAG_ALL) )
				break;
		}
	}

	DBG("DEBUG:avpops:remove_avps: %d avps were removed\n",n);

	return n?1:-1;
}



#define STR_BUF_SIZE  1024
static char str_buf[STR_BUF_SIZE];

inline static int compose_hdr(str *name, str *val, str *hdr, int new)
{
	char *p;
	char *s;
	int len;

	len = name->len+2+val->len+CRLF_LEN;
	if (new)
	{
		if ( (s=(char*)pkg_malloc(len))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:compose_hdr: no more pkg mem\n");
			return -1;
		}
	} else {
		if ( len>STR_BUF_SIZE )
			return -1;
		s = str_buf;
	}
	p = s;
	memcpy(p, name->s, name->len);
	p += name->len;
	*(p++) = ':';
	*(p++) = ' ';
	memcpy(p, val->s, val->len);
	p += val->len;
	memcpy(p, CRLF, CRLF_LEN);
	p += CRLF_LEN;
	if (len!=p-s)
	{
		LOG(L_CRIT,"BUG:avpops:compose_hdr: buffer overflow\n");
		return -1;
	}
	hdr->len = len;
	hdr->s = s;
	return 0;
}



inline static int append_0(str *in, str *out)
{
	if (in->len+1>STR_BUF_SIZE)
		return -1;
	memcpy( str_buf, in->s, in->len);
	str_buf[in->len] = 0;
	out->len = in->len;
	out->s = str_buf;
	return 0;
}



int ops_pushto_avp ( struct sip_msg* msg, struct fis_param* dst,
													struct fis_param* ap)
{
	struct lump    *anchor;
	struct action  act;
	struct usr_avp *avp;
	unsigned short name_type;
	int_str        avp_val;
	str            val;
	int            act_type;
	int            n;

	/* search for the avp */
	name_type = (((ap->flags&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	avp = search_first_avp( name_type, ap->val, &avp_val);
	if (avp==0)
	{
		DBG("DEBUG:avpops:pushto_avp: no avp found\n");
		return -1;
	}

	n = 0;
	while (avp)
	{
		/* the avp val will be used all the time as str */
		if (avp->flags&AVP_VAL_STR) {
			val = *(avp_val.s);
		} else {
			val.s = int2str((unsigned long)avp_val.n, &val.len);
		}

		act_type = 0;
		/* push the value into right position */
		if (dst->flags&AVPOPS_USE_RURI)
		{
			if (dst->flags&AVPOPS_FLAG_USER)
				act_type = SET_USER_T;
			else if (dst->flags&AVPOPS_FLAG_DOMAIN)
				act_type = SET_HOST_T;
			else
				act_type = SET_URI_T;
			if ( avp->flags&AVP_VAL_STR && append_0( &val, &val)!=0 ) {
				LOG(L_ERR,"ERROR:avpops:pushto_avp: failed to make 0 term.\n");
				goto error;
			}
		} else if (dst->flags&(AVPOPS_USE_HDRREQ|AVPOPS_USE_HDRRPL)) {
			if ( compose_hdr( dst->val.s, &val, &val,
				dst->flags&AVPOPS_USE_HDRREQ)<0 )
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: failed to build hdr\n");
				goto error;
			}
		} else {
			LOG(L_CRIT,"BUG:avpops:pushto_avp: destination unknown (%d)\n",
				dst->flags);
			goto error;
		}
	
		if ( act_type )
		{
			/* rewrite part of ruri */
			if (n)
			{
				/* if is not the first modification, push the current uri as
				 * branch */
				if (append_branch( msg, 0, 0, 0, 0, 0)!=1 )
				{
					LOG(L_ERR,"ERROR:avpops:pushto_avp: append_branch action"
						" failed\n");
					goto error;
				}
			}
			memset(&act, 0, sizeof(act));
			act.p1_type = STRING_ST;
			act.p1.string = val.s;
			act.type = act_type;
			if (do_action(&act, msg)<0)
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: SET_XXXX_T action"
					" failed\n");
				goto error;
			}
		} else if (dst->flags==AVPOPS_USE_HDRRPL) {
			/* set a header for reply */
			if (add_lump_rpl( msg , val.s, val.len, LUMP_RPL_HDR )==0)
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: add_lump_rpl failed\n");
				goto error;
			}
		} else {
			/* set a header for request */
			if (parse_headers(msg, HDR_EOH, 0)==-1)
			{
				LOG(L_ERR, "ERROR:avpops:pushto_avp: message parse failed\n");
				goto error;
			}
			anchor = anchor_lump( msg, msg->unparsed-msg->buf, 0, 0);
			if (anchor==0)
			{
				LOG(L_ERR, "ERROR:avpops:pushto_avp: can't get anchor\n");
				goto error;
			}
			if (insert_new_lump_before(anchor, val.s, val.len, 0)==0)
			{
				LOG(L_ERR, "ERROR:avpops:pushto_avp: can't insert lump\n");
				goto error;
			}
		}

		n++;
		if ( !(ap->flags&AVPOPS_FLAG_ALL) )
			break;
		avp = search_next_avp( avp, &avp_val);
	} /* end while */

	DBG("DEBUG:avpops:pushto_avps: %d avps were processed\n",n);
	return 1;
error:
	return -1;
}


int ops_check_avp( struct sip_msg* msg, struct fis_param* ap,
													struct fis_param* val)
{
	unsigned short    name_type;
	struct usr_avp    *avp1;
	struct usr_avp    *avp2;
	regmatch_t        pmatch;
	int_str           avp_val;
	int_str           ck_val;
	str               s_ip;
	int               ck_flg;
	int               n;

	/* look if the required avp(s) is/are present */
	name_type = (((ap->flags&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	avp1 = search_first_avp( name_type, ap->val, &avp_val);
	if (avp1==0)
	{
		DBG("DEBUG:avpops:check_avp: no avp found to check\n");
		goto error;
	}

cycle1:
	if (val->flags&AVPOPS_VAL_AVP)
	{
		/* the 2nd operator is an avp name -> get avp val */
		name_type = (((val->flags&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		avp2 = search_first_avp( name_type, val->val, &ck_val);
		ck_flg = avp2->flags&AVP_VAL_STR?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
	} else {
		ck_val = val->val;
		ck_flg = val->flags;
		avp2 = 0;
	}

cycle2:
	/* are both values of the same type? */
	if ( !( (avp1->flags&AVP_VAL_STR && ck_flg&AVPOPS_VAL_STR) ||
		((avp1->flags&AVP_VAL_STR)==0 && ck_flg&AVPOPS_VAL_INT) ||
		(avp1->flags&AVP_VAL_STR && ck_flg&AVPOPS_VAL_NONE)))
	{
		LOG(L_ERR,"ERROR:avpops:check_avp: value types don't match\n");
		goto next;
	}

	if (avp1->flags&AVP_VAL_STR)
	{
		/* string values to check */
		if (val->flags&AVPOPS_VAL_NONE)
		{
			/* value is variable */
			if (val->flags&AVPOPS_USE_SRC_IP)
			{
				/* get value from src_ip */
				if ( (s_ip.s=ip_addr2a( &msg->rcv.src_ip ))==0)
				{
					LOG(L_ERR,"ERROR:avpops:check_avp: cannot get src_ip\n");
					goto error;
				}
				s_ip.len = strlen(s_ip.s);
				ck_val.s = &s_ip;
			} else {
				/* get value from uri */
				if ( (ck_val.s=get_source_uri(msg,val->flags))==0 )
				{
					LOG(L_ERR,"ERROR:avpops:check_avp: cannot get uri\n");
					goto next;
				}
			}
		}
		DBG("DEBUG:avpops:check_avp: check <%.*s> against <%.*s> as str\n",
			avp_val.s->len,avp_val.s->s,
			(val->flags&AVPOPS_OP_RE)?6:ck_val.s->len,
			(val->flags&AVPOPS_OP_RE)?"REGEXP":ck_val.s->s);
		/* do check */
		if (val->flags&AVPOPS_OP_EQ)
		{
			if (avp_val.s->len==ck_val.s->len)
			{
				if (val->flags&AVPOPS_FLAG_CI)
				{
					if (strncasecmp(avp_val.s->s,ck_val.s->s,ck_val.s->len)==0)
						return 1;
				} else {
					if (strncmp(avp_val.s->s,ck_val.s->s,ck_val.s->len)==0 )
						return 1;
				}
			}
		} else if (val->flags&AVPOPS_OP_LT) {
			n = (avp_val.s->len>=ck_val.s->len)?avp_val.s->len:ck_val.s->len;
			if (strncasecmp(avp_val.s->s,ck_val.s->s,n)==-1)
				return 1;
		} else if (val->flags&AVPOPS_OP_GT) {
			n = (avp_val.s->len>=ck_val.s->len)?avp_val.s->len:ck_val.s->len;
			if (strncasecmp(avp_val.s->s,ck_val.s->s,n)==1)
				return 1;
		} else if (val->flags&AVPOPS_OP_RE) {
			if (regexec((regex_t*)ck_val.s, avp_val.s->s, 1, &pmatch, 0)==0)
				return 1;
		} else {
			LOG(L_CRIT,"BUG:avpops:check_avp: unknown operation "
				"(flg=%d)\n",val->flags);
		}
	} else {
		/* int values to check -> do check */
		DBG("DEBUG:avpops:check_avp: check <%d> against <%d> as int\n",
				avp_val.n, ck_val.n);
		if (val->flags&AVPOPS_OP_EQ)
		{
			if ( avp_val.n==ck_val.n)
				return 1;
		} else if (val->flags&AVPOPS_OP_LT) {
			if ( avp_val.n<ck_val.n)
				return 1;
		} else if (val->flags&AVPOPS_OP_GT) {
			if ( avp_val.n>ck_val.n)
				return 1;
		} else {
			LOG(L_CRIT,"BUG:avpops:check_avp: unknown operation "
				"(flg=%d)\n",val->flags);
		}
	}

next:
	/* cycle for the second value (only if avp can have multiple vals) */
	if ((val->flags&AVPOPS_VAL_AVP)&&(avp2=search_next_avp(avp2,&ck_val))!=0)
	{
		ck_flg = avp2->flags&AVP_VAL_STR?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
		goto cycle2;
	/* cycle for the first value -> next avp */
	} else {
		avp1=(val->flags&AVPOPS_FLAG_ALL)?search_next_avp(avp1,&avp_val):0;
		if (avp1)
			goto cycle1;
	}

	DBG("DEBUG:avpops:check_avp: checked failed\n");
	return -1; /* check failed */
error:
	return -1;
}



int ops_print_avp()
{
	struct usr_avp **avp_list;
	struct usr_avp *avp;
	int_str         val;
	str            *name;

	/* go through all list */
	avp_list = get_avp_list();
	avp = *avp_list;

	for ( ; avp ; avp=avp->next)
	{
		DBG("DEBUG:avpops:print_avp: p=%p, flags=%X\n",avp, avp->flags);
		if (avp->flags&AVP_NAME_STR)
		{
			name = get_avp_name(avp);
			DBG("DEBUG:\t\t\tname=<%.*s>\n",name->len,name->s);
		} else {
			DBG("DEBUG:\t\t\tid=<%d>\n",avp->id);
		}
		get_avp_val( avp, &val);
		if (avp->flags&AVP_VAL_STR)
		{
			DBG("DEBUG:\t\t\tval_str=<%.*s>\n",val.s->len,val.s->s);
		} else {
			DBG("DEBUG:\t\t\tval_int=<%d>\n",val.n);
		}
	}

	
	return 1;
}

