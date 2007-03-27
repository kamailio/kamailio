/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <time.h>

#include "../../ut.h"
#include "../../str.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h" 
#include "../../parser/parse_expires.h" 
#include "../../parser/parse_content.h" 
#include "../../lock_ops.h"
#include "../../hash_func.h"
#include "../../db/db.h"
#include "../../data_lump_rpl.h"
#include "presence.h"
#include "notify.h"
#include "utils_func.h"
#include "publish.h"
#include "presentity.h"

extern db_con_t* pa_db;
extern db_func_t pa_dbf;
extern gen_lock_set_t* set;
extern int counter ;
extern int pid;
extern char prefix;
extern int startup_time;

static str pu_400a_rpl = str_init("Bad request");
static str pu_400b_rpl = str_init("Invalid request");
static str pu_489_rpl  = str_init("Bad Event");
static str pu_415_rpl  = str_init("Unsupported media type");
static str pu_200_rpl  = str_init("OK");
static str pu_412_rpl  = str_init("Conditional request failed");


char* generate_ETag()
{
	char* etag;
	int size = 0;
	etag = (char*)pkg_malloc(40*sizeof(char));
	if(etag ==NULL)
	{
		LOG(L_ERR, "PRESENCE:generate_ETag:Error while allocating memory \n");
		return NULL ;
	}
	size = sprintf (etag, "%c.%d.%d.%d",prefix, startup_time, pid, counter );
	if( size <0 )
	{
		LOG(L_ERR, "PRESENCE: generate_ETag: ERROR unsuccessfull sprintf\n ");
		return NULL;
	}
	LOG(L_ERR, "PRESENCE: generate_ETag: etag= %.*s / %d\n ", size, etag, size);
	etag[size] = '\0';
	return etag;
}

void msg_presentity_clean(unsigned int ticks,void *param)
{
	db_key_t db_keys[5];
	db_val_t db_vals[5];
	db_op_t  db_ops[5] ;
	db_key_t result_cols[3];
	db_res_t *result = NULL;
	db_row_t *row ;	
	db_val_t *row_vals ;
	int i =0, size= 0;
	presentity_t** p= NULL;
	presentity_t* pres= NULL;
	int user_len, domain_len, etag_len;
	int n= 0;

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:msg_presentity_clean: Error in use_table\n");
		return ;
	}
	
	DBG("PRESENCE:msg_presentity_clean:cleaning expired presentity"
			" information\n");

	db_keys[0] ="expires";
	db_ops[0] = OP_LT;
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = (int)time(NULL);
		
	result_cols[0] = "username";
	result_cols[1] = "domain";
	result_cols[2] = "etag";

	if(pa_dbf.query(pa_db, db_keys, db_ops, db_vals, result_cols,
						1, 3, 0, &result )< 0)
	{
		LOG(L_ERR,
			"PRESENCE:msg_presentity_clean: ERROR while querying database"
			" for expired messages\n");
		if(result)
			pa_dbf.free_result(pa_db, result);
		return;
	}
	if(result== NULL)
		return;

	if(result && result->n<= 0)
	{
		pa_dbf.free_result(pa_db, result);	
		return;
	}
	DBG("PRESENCE:msg_presentity_clean: found n= %d expires messages\n ",
			result->n);

	n= result->n;
	
	p= (presentity_t**)pkg_malloc(result->n* sizeof(presentity_t*));
	if(p== NULL)
	{
		LOG(L_ERR, "PRESENCE:msg_presentity_clean:  ERROR while allocating memory\n");
		goto error;
	}

	for(i = 0; i< n; i++)
	{	
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);	
		
		user_len = strlen(row_vals[0].val.str_val.s);
		domain_len = strlen(row_vals[1].val.str_val.s);
		etag_len= strlen(row_vals[2].val.str_val.s);
		
		size= sizeof(presentity_t)+ user_len+ domain_len+ etag_len; 
		pres= (presentity_t*)pkg_malloc(size);
		if(pres== NULL)
		{
			LOG(L_ERR, "PRESENCE:msg_presentity_clean:  ERROR while allocating memory\n");
			goto error;
		}
		memset(pres, 0, size);
		size= sizeof(presentity_t);
		
		pres->user.s= (char*)pres+ size;	
		memcpy(pres->user.s, row_vals[0].val.str_val.s, user_len);
		pres->user.len= user_len;
		size+= user_len;

		pres->domain.s= (char*)pres+ size;
		memcpy(pres->domain.s, row_vals[1].val.str_val.s, domain_len);
		pres->domain.len= domain_len;
		size+= domain_len;

		pres->etag.s= (char*)pres+ size;
		memcpy(pres->etag.s, row_vals[2].val.str_val.s, etag_len);
		pres->etag.len= etag_len;
		size+= etag_len;
		
		p[i]= pres;

	}

	pa_dbf.free_result(pa_db, result);
	result= NULL;
	
	for(i= 0; i<n ; i++)
	{

		LOG(L_INFO, "PRESENCE:msg_presentity_clean:found expired publish"
				" for [user]=%.*s  [domanin]=%.*s\n",p[i]->user.len,p[i]->user.s,
				p[i]->domain.len, p[i]->domain.s);
		query_db_notify( &p[i]->user, &p[i]->domain, "presence", NULL, &p[i]->etag);

	}


	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE:msg_presentity_clean: Error in use_table\n");
		goto error;
	}
	
	if (pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, 1) < 0) 
		LOG(L_ERR,"PRESENCE:msg_presentity_clean: ERROR cleaning expired"
				" messages\n");
	
	for(i= 0; i< n; i++)
	{
		if(p[i])
			pkg_free(p[i]);
	}
	pkg_free(p);

	return;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(p)
	{
		for(i= 0; i< n; i++)
		{
			if(p[i])
				pkg_free(p[i]);
		}
		pkg_free(p);
	}
	return;	
}



/**
 * PUBLISH request handling
 *
 */
int handle_publish(struct sip_msg* msg, char* str1, char* str2)
{
	struct sip_uri uri;
	str body;
	unsigned int idx;
	struct to_body *pto, TO;
	int lexpire;
	presentity_t* presentity = 0;
	struct hdr_field* hdr;
	int found= 0, etag_gen = 0, update_p = 0;
	str etag;
	str hdr_append, hdr_append2 ;
	int error_ret = -1; /* error return code */
	xmlDocPtr doc= NULL;

	counter ++;
	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LOG(L_ERR, "PRESENCE:handle_publish: error parsing headers\n");
		slb.reply(msg, 400, &pu_400a_rpl);
		return 0;
	}
	memset(&body, 0, sizeof(str));
	/* inspecting the Event header field */
	if( (!msg->event ) ||(msg->event->body.len<=0) ||
	( strncmp(msg->event->body.s, "presence",8 )!=0) )
	{
		LOG(L_ERR, "PRESENCE: handle_publish:Missing or unsupported event"
				" header field value [%.*s]\n", msg->event->body.len,
				msg->event->body.s);

		if (slb.reply(msg, 489, &pu_489_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE: handle_publish: Error while sending reply\n");
		}
		return 0;
	}

	/* examine the SIP-If-Match header field */
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(strncmp(hdr->name.s, "SIP-If-Match",12)==0|| 
				strncmp(hdr->name.s,"Sip-If-Match",12)==0 )
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if( found==0 )
	{
		DBG("PRESENCE:handle_publish: SIP-If-Match not found\n");
		etag.s = generate_ETag();
		if(etag.s == NULL)
		{
			LOG(L_ERR,
				"PRESENCE:handle_publish:ERROR while generating etag\n");
			return -1;
		}
		etag.len=(strlen(etag.s));
		etag_gen=1;
		DBG("PRESENCE:handle_publish: new etag  = %.*s \n", etag.len,
				etag.s);
	}
	else
	{
		DBG("PRESENCE:handle_publish: SIP-If-Match found\n");

		etag.s = hdr->body.s;
		etag.len = hdr->body.len;
		etag.s[ etag.len] = '\0';
		DBG("PRESENCE:handle_publish: existing etag  = %.*s \n", etag.len,
				etag.s);
	}

	/* examine the expire header field */
	if(msg->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LOG(L_ERR,
				"PRESENCE: handle_publish: ERROR cannot parse Expires header\n");
			goto error;
		}
		DBG("PRESENCE: handle_publish: 'expires' found\n");
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		DBG("PRESENCE: handle_publish: lexpire= %d\n", lexpire);

	} else 
	{
		DBG("PRESENCE: handle_publish: 'expires' not found; default=%d\n",
				default_expires);
		lexpire = default_expires;
	}
	if(lexpire > max_expires)
		lexpire = max_expires;

	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_publish: ERROR cannot parse TO header\n");
		goto error;
	}

	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		DBG("PRESENCE: handle_publish: 'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			DBG("PRESENCE: handle_publish: 'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}
	
	if(parse_uri(pto->uri.s, pto->uri.len, &uri)!=0)
	{
		LOG(L_ERR, "PRESENCE: handle_publish: bad R-URI!\n");
		goto error;
	}

	if(uri.user.len<=0 || uri.user.s==NULL || uri.host.len<=0 ||
			uri.host.s==NULL)
	{
		LOG(L_ERR, "PRESENCE: handle_publish: bad URI in To header!\n");
		goto error;
	}

	if (!msg->content_length) 
	{
		LOG(L_ERR,"PRESENCE: handle_publish: ERROR no Content-Length"
				" header found!\n");
		goto error;
	}	

	/* process the body */
	if ( get_content_length(msg) == 0 )
	{
		body.s = NULL;
		if (etag_gen)
		{
			LOG(L_ERR, "PRESENCE: handle_publish: No E-Tag and no body"
					" present\n");

			if (slb.reply(msg, 400, &pu_400b_rpl) == -1)
			{
				LOG(L_ERR, "PRESENCE: handle_publish: Error while sending"
						" reply\n");
			}
			error_ret = 0;
			goto error;
		}
	}
	else
	{
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LOG(L_ERR,"PRESENCE: handle_publish: ERROR cannot extract body"
					" from msg\n");
			goto error;
		}
		/* content-length (if present) must be already parsed */

		body.len = get_content_length( msg );
		doc= xmlParseMemory( body.s , body.len );
		if(doc== NULL)
		{
			LOG(L_ERR, "PRESENCE: handle_publish: Bad body format\n");
			if( slb.reply( msg, 415, &pu_415_rpl)== -1)
			{
				LOG(L_ERR,"PRESENCE: handle_publish: ERORR while sending"
						" reply\n");
			}
			error_ret = 0;
			goto error;
		}
		xmlFreeDoc(doc);
	}	
	
	/* now we have all the necessary values */
	/* fill in the filds of the structure */
	if(new_presentity(&uri.host, &uri.user, lexpire ,(int)time(NULL), &etag,&presentity)
			!=0)
	{
		LOG(L_ERR,"PRESENCE: handle_publish: ERORR creating presentity\n");
		goto error;
	}

	idx = core_hash( &pto->uri, NULL, lock_set_size ) ;

	lock_set_get( set, idx );
	/* querry the database and update or insert */
	update_p= update_presentity(presentity, &body, etag_gen);
	lock_set_release( set, idx );

	if(update_p <0)
	{
		LOG(L_ERR, "PRESENCE:handle_publish: ERROR occured while updating"
				" presentity\n");
		goto error;
	}

	if(update_p >= 0 && update_p != 412)
	{
		/*send a 200(OK) reply with the 2 headers*/	
	
		/* ??? should we use static allocated buffer */
		hdr_append.s = (char *)pkg_malloc( sizeof(char)*50);
		if(hdr_append.s == NULL)
		{
			LOG(L_ERR,"ERROR:handle_publish : unable to add lump_rl\n");
			goto error;
		}
		hdr_append.s[0]='\0';
		hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n",lexpire -
				expires_offset);
		if(hdr_append.len < 0)
		{
			LOG(L_ERR, "PRESENCE:handle_publish: ERROR unsuccessful sprintf\n");
			pkg_free(hdr_append.s);
			goto error;
		}
		hdr_append.s[hdr_append.len]= '\0';
		
		if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
		{
			LOG(L_ERR,"PRESENCE: handle_publish:ERROR unable to add lump_rl\n");
			pkg_free(hdr_append.s);
			goto error;
		}
		pkg_free(hdr_append.s);

		hdr_append2.s = (char *)pkg_malloc( sizeof(char)*16+etag.len );
		if(hdr_append2.s == NULL)
		{
			LOG(L_ERR,"PRESENCE:handle_publish:ERROR unable to add lump_rl\n");
			goto error;
		}
		hdr_append2.s[0]='\0';
		hdr_append2.len = sprintf(hdr_append2.s, "SIP-ETag: %s\r\n", etag.s);
		if(hdr_append2.len < 0)
		{
			LOG(L_ERR, "PRESENCE:handle_publish:ERROR unsuccessful sprintf\n ");
			pkg_free(hdr_append2.s);
			goto error;
		}
		hdr_append2.s[hdr_append2.len]= '\0';
		LOG(L_ERR, "PRESENCE: handle_publish: sip-etag: [%.*s/%d]\n",
				hdr_append2.len,hdr_append2.s,hdr_append2.len);
		if (add_lump_rpl(msg, hdr_append2.s, hdr_append2.len, LUMP_RPL_HDR)==0 )
		{
			LOG(L_ERR,"PRESENCE:handle_publish: unable to add lump_rl\n");
			pkg_free(hdr_append2.s);
			goto error;
		}
		pkg_free(hdr_append2.s);

		if( slb.reply( msg, 200, &pu_200_rpl)== -1)
		{
			LOG(L_ERR,"PRESENCE: handle_publish: ERORR while sending reply\n");
			goto error;
		}
	}

	if(update_p == 412)
	{
		if (slb.reply(msg, 412, &pu_412_rpl) == -1)
		{
			LOG(L_ERR, "PRESENCE:PRESENCE:handle_publish: ERROR while sending"
					"reply\n");
		}
	}
	

	if(presentity)
		free_presentity(presentity);
	if(etag_gen)
		pkg_free(etag.s);
	xmlCleanupParser();
	xmlMemoryDump();

	return 1;

error:
	LOG(L_ERR, "PRESENCE: handle_publish: ERROR occured\n");
	
	if(presentity)
		free_presentity(presentity);
	if(etag_gen )
		pkg_free(etag.s);
	xmlCleanupParser();
	xmlMemoryDump();
	
	return error_ret;

}



