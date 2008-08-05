/*
 * $Id$
 *
 * dispatcher module
 *
 * Copyright (C) 2004-2006 FhG Fokus
 * Copyright (C) 2005 Voice-System.ro
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
 *
 * History
 * -------
 * 2004-07-31  first version, by daniel
 * 2005-04-22  added ruri  & to_uri hashing (andrei)
 * 2005-12-10  added failover support via avp (daniel)
 * 2006-08-15  added support for authorization username hashing (carsten)
 * 2007-01-11  Added a function to check if a specific gateway is in a
 * group (carsten)
 * 2007-01-12  Added a threshhold for automatic deactivation (carsten)
 * 2007-02-09  Added active probing of failed destinations and automatic
 * re-enabling of destinations (carsten)
 * 2007-05-08  Ported the changes to SVN-Trunk, renamed ds_is_domain to
 * ds_is_from_list and modified the function to work with IPv6 adresses.
 * 2007-07-18  removed index stuff 
 * 			   added DB support to load/reload data(ancuta)
 * 2007-09-17  added list-file support for reload data (carstenbock)
 */

/*! \file
 * \ingroup dispatcher
 * \brief Dispatcher :: Dispatch
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../ut.h"
#include "../../trim.h"
#include "../../dprint.h"
#include "../../action.h"
#include "../../route.h"
#include "../../dset.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../usr_avp.h"
#include "../../mi/mi.h"
#include "../../parser/digest/digest.h"
#include "../../resolve.h"
#include "../tm/tm_load.h"
#include "../../db/db.h"
#include "../../db/db_res.h"
#include "../../str.h"

#include "dispatch.h"

#define DS_TABLE_VERSION	1
#define DS_TABLE_VERSION2	2

static int _ds_table_version = DS_TABLE_VERSION;

typedef struct _ds_dest
{
	str uri;
	int flags;
	struct ip_addr ip_address; 	/*!< IP-Address of the entry */
	unsigned short int port; 	/*!< Port of the request URI */
	int failure_count;
	struct _ds_dest *next;
} ds_dest_t, *ds_dest_p;

typedef struct _ds_set
{
	int id;				/*!< id of dst set */
	int nr;				/*!< number of items in dst set */
	int last;			/*!< last used item in dst set */
	ds_dest_p dlist;
	struct _ds_set *next;
} ds_set_t, *ds_set_p;

extern int ds_force_dst;

static db_func_t ds_dbf;
static db_con_t* ds_db_handle=0;
ds_set_p *ds_lists=NULL;
int *ds_list_nr = NULL;
int *crt_idx    = NULL;
int *next_idx   = NULL;

#define _ds_list 	(ds_lists[*crt_idx])
#define _ds_list_nr (*ds_list_nr)

void destroy_list(int);

int init_data(void)
{
	int * p;

	ds_lists = (ds_set_p*)shm_malloc(2*sizeof(ds_set_p));
	if(!ds_lists)
	{
		LM_ERR("Out of memory\n");
		return -1;
	}
	ds_lists[0] = ds_lists[1] = 0;

	
	p = (int*)shm_malloc(3*sizeof(int));
	if(!p)
	{
		LM_ERR("Out of memory\n");
		return -1;
	}

	crt_idx = p;
	next_idx = p+1;
	ds_list_nr = p+2;
	*crt_idx= *next_idx = 0;

	return 0;
}

int add_dest2list(int id, str uri, int flags, int list_idx, int * setn)
{
	ds_dest_p dp = NULL;
	ds_set_p  sp = NULL;
	
	/* For DNS-Lookups */
	static char hn[256];
	struct hostent* he;
	struct sip_uri puri;

	/* check uri */
	if(parse_uri(uri.s, uri.len, &puri)!=0 || puri.host.len>254)
	{
		LM_ERR("bad uri [%.*s]\n", uri.len, uri.s);
		goto err;
	}
	
	/* get dest set */
	sp = ds_lists[list_idx];
	while(sp)
	{
		if(sp->id == id)
			break;
		sp = sp->next;
	}

	if(sp==NULL)
	{
		sp = (ds_set_p)shm_malloc(sizeof(ds_set_t));
		if(sp==NULL)
		{
			LM_ERR("no more memory.\n");
			goto err;
		}
		
		memset(sp, 0, sizeof(ds_set_t));
		sp->next = ds_lists[list_idx];
		ds_lists[list_idx] = sp;
		*setn = *setn+1;
	}
	sp->id = id;
	sp->nr++;

	/* store uri */
	dp = (ds_dest_p)shm_malloc(sizeof(ds_dest_t));
	if(dp==NULL)
	{
		LM_ERR("no more memory!\n");
		goto err;
	}
	memset(dp, 0, sizeof(ds_dest_t));

	dp->uri.s = (char*)shm_malloc((uri.len+1)*sizeof(char));
	if(dp->uri.s==NULL)
	{
		LM_ERR("no more memory!\n");
		goto err;
	}
	strncpy(dp->uri.s, uri.s, uri.len);
	dp->uri.s[uri.len]='\0';
	dp->uri.len = uri.len;
	dp->flags = flags;

	/* The Hostname needs to be \0 terminated for resolvehost, so we
	 * make a copy here. */
	strncpy(hn, puri.host.s, puri.host.len);
	hn[puri.host.len]='\0';
		
	/* Do a DNS-Lookup for the Host-Name: */
	he=resolvehost(hn, 1);
	if (he==0)
	{
		LM_ERR("could not resolve %.*s\n", puri.host.len, puri.host.s);
		pkg_free(hn);
		goto err;
	}
	/* Free the hostname */
	hostent2ip_addr(&dp->ip_address, he, 0);
		
	/* Copy the Port out of the URI: */
	dp->port = puri.port_no;		

	dp->next = sp->dlist;
	sp->dlist = dp;

	LM_DBG("dest [%d/%d] <%.*s>\n", sp->id, sp->nr, dp->uri.len, dp->uri.s);
	
	return 0;
err:
	/* free allocated memory */
	if(dp!=NULL)
	{
		if(dp->uri.s!=NULL)
			shm_free(dp->uri.s);
		shm_free(dp);
	}
	return -1;
}

/*! \brief  compact destinations from sets for fast access */
int reindex_dests(int list_idx, int setn)
{
	int j;
	ds_set_p  sp = NULL;
	ds_dest_p dp = NULL, dp0= NULL;

	for(sp = ds_lists[list_idx]; sp!= NULL;	sp->dlist = dp0, sp = sp->next)
	{
		dp0 = (ds_dest_p)shm_malloc(sp->nr*sizeof(ds_dest_t));
		if(dp0==NULL)
		{
			LM_ERR("no more memory!\n");
			goto err1;
		}
		memset(dp0, 0, sp->nr*sizeof(ds_dest_t));

		/*copy from the old pointer to destination, and then free it*/
		for(j=sp->nr-1; j>=0 && sp->dlist!= NULL; j--)
		{
			memcpy(&dp0[j], sp->dlist, sizeof(ds_dest_t));
			if(j==sp->nr-1)
				dp0[j].next = NULL;
			else
				dp0[j].next = &dp0[j+1];
	
			dp = sp->dlist;
			sp->dlist = dp->next;
			
			shm_free(dp);
			dp=NULL;
		}
	}

	LM_DBG("found [%d] dest sets\n", setn);
	return 0;

err1:
	return -1;
}

/*! \brief load groups of destinations from file */
int ds_load_list(char *lfile)
{
	char line[256], *p;
	FILE *f = NULL;
	int id, setn, flags;
	str uri;
	
	if( (*crt_idx) != (*next_idx)) {
		LM_WARN("load command already generated, aborting reload...\n");
		return 0;
	}

	if(lfile==NULL || strlen(lfile)<=0)
	{
		LM_ERR("bad list file\n");
		return -1;
	}

	f = fopen(lfile, "r");
	if(f==NULL)
	{
		LM_ERR("can't open list file [%s]\n", lfile);
		return -1;
		
	}

	id = setn = flags = 0;

	*next_idx = (*crt_idx + 1)%2;
	destroy_list(*next_idx);
	
	p = fgets(line, 256, f);
	while(p)
	{
		/* eat all white spaces */
		while(*p && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n'))
			p++;
		if(*p=='\0' || *p=='#')
			goto next_line;
		
		/* get set id */
		id = 0;
		while(*p>='0' && *p<='9')
		{
			id = id*10+ (*p-'0');
			p++;
		}
		
		/* eat all white spaces */
		while(*p && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n'))
			p++;
		if(*p=='\0' || *p=='#')
		{
			LM_ERR("bad line [%s]\n", line);
			goto error;
		}

		/* get uri */
		uri.s = p;
		while(*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n' && *p!='#')
			p++;
		uri.len = p-uri.s;

		/* eat all white spaces */
		while(*p && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n'))
			p++;
		
		/* get flags */
		flags = 0;
		if(*p=='\0' || *p=='#')
		{
			/* no flags given */
			goto add_destination;
		}

		while(*p>='0' && *p<='9')
		{
			flags = flags*10+ (*p-'0');
			p++;
		}
		
add_destination:
		if(add_dest2list(id, uri, flags, *next_idx, &setn) != 0)
			goto error;
					
		
next_line:
		p = fgets(line, 256, f);
	}
		
	if(reindex_dests(*next_idx, setn)!=0){
		LM_ERR("error on reindex\n");
		goto error;
	}

	fclose(f);
	f = NULL;
	/* Update list */
	_ds_list_nr = setn;
	*crt_idx = *next_idx;
	return 0;

error:
	if(f!=NULL)
		fclose(f);
	destroy_list(*next_idx);
	*next_idx = *crt_idx; 
	return -1;
}

int ds_connect_db(void)
{
	if(!ds_db_url.s)
		return -1;

	if (ds_db_handle)
	{
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}

	if ((ds_db_handle = ds_dbf.init(&ds_db_url)) == 0){
		
			return -1;
	}
	return 0;
}

void ds_disconnect_db(void)
{
	if(ds_db_handle)
	{
		ds_dbf.close(ds_db_handle);
		ds_db_handle = 0;
	}
}

/*! \brief Initialize and verify DB stuff*/
int init_ds_db(void)
{
	int ret;

	if(ds_table_name.s == 0)
	{
		LM_ERR("invalid database name\n");
		return -1;
	}
	
	/* Find a database module */
	if (db_bind_mod(&ds_db_url, &ds_dbf) < 0)
	{
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}
	
	if(ds_connect_db()!=0){
		
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	
	_ds_table_version = db_table_version(&ds_dbf, ds_db_handle, &ds_table_name);
	if (_ds_table_version < 0) 
	{
		LM_ERR("failed to query table version\n");
		return -1;
	} else if (_ds_table_version != DS_TABLE_VERSION
			&& _ds_table_version != DS_TABLE_VERSION2) {
		LM_ERR("invalid table version (found %d , required %d or %d)\n"
			"(use kamdbctl reinit)\n",
			_ds_table_version, DS_TABLE_VERSION, DS_TABLE_VERSION2 );
		return -1;
	}

	ret = ds_load_db();

	ds_disconnect_db();

	return ret;
}

/*! \brief load groups of destinations from DB*/
int ds_load_db(void)
{
	int i, id, nr_rows, setn;
	int flags;
	int nrcols;
	str uri;
	db_res_t * res;
	db_val_t * values;
	db_row_t * rows;
	
	db_key_t query_cols[3] = {&ds_set_id_col, &ds_dest_uri_col,
								&ds_dest_flags_col};
	
	nrcols = 2;
	if(_ds_table_version == DS_TABLE_VERSION2)
		nrcols = 3;

	if( (*crt_idx) != (*next_idx))
	{
		LM_WARN("load command already generated, aborting reload...\n");
		return 0;
	}

	if(ds_db_handle == NULL){
			LM_ERR("invalid DB handler\n");
			return -1;
	}

	if (ds_dbf.use_table(ds_db_handle, &ds_table_name) < 0)
	{
		LM_ERR("error in use_table\n");
		return -1;
	}

	/*select the whole table and all the columns*/
	if(ds_dbf.query(ds_db_handle,0,0,0,query_cols,0,nrcols,0,&res) < 0)
	{
		LM_ERR("error while querying database\n");
		return -1;
	}

	nr_rows = RES_ROW_N(res);
	rows 	= RES_ROWS(res);
	if(nr_rows == 0)
	{
		LM_WARN("no dispatching data in the db -- empty destination set\n");
		ds_dbf.free_result(ds_db_handle, res);
		return 0;
	}

	setn = 0;
	*next_idx = (*crt_idx + 1)%2;
	destroy_list(*next_idx);
	
	for(i=0; i<nr_rows; i++)
	{
		values = ROW_VALUES(rows+i);

		id = VAL_INT(values);
		uri.s = VAL_STR(values+1).s;
		uri.len = strlen(uri.s);
		flags = 0;
		if(nrcols==3)
			flags = VAL_INT(values+2);

		if(add_dest2list(id, uri, flags, *next_idx, &setn) != 0)
			goto err2;

	}

	if(reindex_dests(*next_idx, setn)!=0)
	{
		LM_ERR("error on reindex\n");
		goto err2;
	}

	/*update data*/
	_ds_list_nr = setn;
	*crt_idx = *next_idx;
	ds_dbf.free_result(ds_db_handle, res);

	return 0;

err2:
	destroy_list(*next_idx);
	ds_dbf.free_result(ds_db_handle, res);
	*next_idx = *crt_idx; 

	return -1;
}

/*! \brief called from dispatcher.c: free all*/
int ds_destroy_list(void)
{
	if (ds_lists) {
		destroy_list(0);
		destroy_list(1);
		shm_free(ds_lists);
	}

	if (crt_idx)
		shm_free(crt_idx);

	return 0;
}

void destroy_list(int list_id)
{
	ds_set_p  sp = NULL;
	ds_dest_p dest = NULL;

	sp = ds_lists[list_id];

	while(sp)
	{
		for(dest = sp->dlist; dest!= NULL; dest=dest->next)
		{
			if(dest->uri.s!=NULL)
   			{
   				shm_free(dest->uri.s);
   				dest->uri.s = NULL;
	   		}
		}
		shm_free(sp->dlist);
		sp = sp->next;
	}
	
	ds_lists[list_id]  = NULL;
}

/**
 *
 */
unsigned int ds_get_hash(str *x, str *y)
{
	char* p;
	register unsigned v;
	register unsigned h;

	if(!x && !y)
		return 0;
	h=0;
	if(x)
	{
		p=x->s;
		if (x->len>=4)
		{
			for (; p<=(x->s+x->len-4); p+=4)
			{
				v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
				h+=v^(v>>3);
			}
		}
		v=0;
		for (;p<(x->s+x->len); p++)
		{ 
			v<<=8; 
			v+=*p;
		}
		h+=v^(v>>3);
	}
	if(y)
	{
		p=y->s;
		if (y->len>=4) 
		{
			for (; p<=(y->s+y->len-4); p+=4)
			{
				v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
				h+=v^(v>>3);
			}
		}
	
		v=0;
		for (;p<(y->s+y->len); p++)
		{ 
			v<<=8; 
			v+=*p;
		}
		h+=v^(v>>3);
	}
	h=((h)+(h>>11))+((h>>13)+(h>>23));

	return (h)?h:1;
}


/*! \brief
 * gets the part of the uri we will use as a key for hashing
 * \param  key1       - will be filled with first part of the key
 *                       (uri user or "" if no user)
 * \param  key2       - will be filled with the second part of the key
 *                       (uri host:port)
 * \param  uri        - str with the whole uri
 * \param  parsed_uri - struct sip_uri pointer with the parsed uri
 *                       (it must point inside uri). It can be null
 *                       (in this case the uri will be parsed internally).
 * \param  flags  -    if & DS_HASH_USER_ONLY, only the user part of the uri
 *                      will be used
 * \return: -1 on error, 0 on success
 */
static inline int get_uri_hash_keys(str* key1, str* key2,
							str* uri, struct sip_uri* parsed_uri, int flags)
{
	struct sip_uri tmp_p_uri; /* used only if parsed_uri==0 */
	
	if (parsed_uri==0)
	{
		if (parse_uri(uri->s, uri->len, &tmp_p_uri)<0)
		{
			LM_ERR("invalid uri %.*s\n", uri->len, uri->len?uri->s:"");
			goto error;
		}
		parsed_uri=&tmp_p_uri;
	}
	/* uri sanity checks */
	if (parsed_uri->host.s==0)
	{
			LM_ERR("invalid uri, no host present: %.*s\n",
					uri->len, uri->len?uri->s:"");
			goto error;
	}
	
	/* we want: user@host:port if port !=5060
	 *          user@host if port==5060
	 *          user if the user flag is set*/
	*key1=parsed_uri->user;
	key2->s=0;
	key2->len=0;
	if (!(flags & DS_HASH_USER_ONLY))
	{	/* key2=host */
		*key2=parsed_uri->host;
		/* add port if needed */
		if (parsed_uri->port.s!=0)
		{ /* uri has a port */
			/* skip port if == 5060 or sips and == 5061 */
			if (parsed_uri->port_no !=
					((parsed_uri->type==SIPS_URI_T)?SIPS_PORT:SIP_PORT))
				key2->len+=parsed_uri->port.len+1 /* ':' */;
		}
	}
	if (key1->s==0)
	{
		LM_WARN("empty username in: %.*s\n", uri->len, uri->len?uri->s:"");
	}
	return 0;
error:
	return -1;
}



/**
 *
 */
int ds_hash_fromuri(struct sip_msg *msg, unsigned int *hash)
{
	str from;
	str key1;
	str key2;
	
	if(msg==NULL || hash == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse From hdr\n");
		return -1;
	}
	
	if(msg->from==NULL || get_from(msg)==NULL)
	{
		LM_ERR("cannot get From uri\n");
		return -1;
	}
	
	from   = get_from(msg)->uri;
	trim(&from);
	if (get_uri_hash_keys(&key1, &key2, &from, 0, ds_flags)<0)
		return -1;
	*hash = ds_get_hash(&key1, &key2);
	
	return 0;
}



/**
 *
 */
int ds_hash_touri(struct sip_msg *msg, unsigned int *hash)
{
	str to;
	str key1;
	str key2;
	
	if(msg==NULL || hash == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if ((msg->to==0) && ((parse_headers(msg, HDR_TO_F, 0)==-1) ||
				(msg->to==0)))
	{
		LM_ERR("cannot parse To hdr\n");
		return -1;
	}
	
	
	to   = get_to(msg)->uri;
	trim(&to);
	
	if (get_uri_hash_keys(&key1, &key2, &to, 0, ds_flags)<0)
		return -1;
	*hash = ds_get_hash(&key1, &key2);
	
	return 0;
}



/**
 *
 */
int ds_hash_callid(struct sip_msg *msg, unsigned int *hash)
{
	str cid;
	if(msg==NULL || hash == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1) ||
				(msg->callid==NULL)) )
	{
		LM_ERR("cannot parse Call-Id\n");
		return -1;
	}
	
	cid.s   = msg->callid->body.s;
	cid.len = msg->callid->body.len;
	trim(&cid);
	
	*hash = ds_get_hash(&cid, NULL);
	
	return 0;
}



int ds_hash_ruri(struct sip_msg *msg, unsigned int *hash)
{
	str* uri;
	str key1;
	str key2;
	
	
	if(msg==NULL || hash == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if (parse_sip_msg_uri(msg)<0){
		LM_ERR("bad request uri\n");
		return -1;
	}
	
	uri=GET_RURI(msg);
	if (get_uri_hash_keys(&key1, &key2, uri, &msg->parsed_uri, ds_flags)<0)
		return -1;
	
	*hash = ds_get_hash(&key1, &key2);
	return 0;
}

int ds_hash_authusername(struct sip_msg *msg, unsigned int *hash)
{
	/* Header, which contains the authorization */
	struct hdr_field* h = 0;
	/* The Username */
	str username = {0, 0};
	/* The Credentials from this request */
	auth_body_t* cred;
	
	if(msg==NULL || hash == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if (parse_headers(msg, HDR_PROXYAUTH_F, 0) == -1)
	{
		LM_ERR("error parsing headers!\n");
		return -1;
	}
	if (msg->proxy_auth && !msg->proxy_auth->parsed)
		parse_credentials(msg->proxy_auth);
	if (msg->proxy_auth && msg->proxy_auth->parsed) {
		h = msg->proxy_auth;
	}
	if (!h)
	{
		if (parse_headers(msg, HDR_AUTHORIZATION_F, 0) == -1)
		{
			LM_ERR("error parsing headers!\n");
			return -1;
		}
		if (msg->authorization && !msg->authorization->parsed)
			parse_credentials(msg->authorization);
		if (msg->authorization && msg->authorization->parsed) {
			h = msg->authorization;
		}
	}
	if (!h)
	{
		LM_DBG("No Authorization-Header!\n");
		return 1;
	}

	cred=(auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.username.user.len)
	{
		LM_ERR("No Authorization-Username or Credentials!\n");
		return 1;
	}
	
	username.s = cred->digest.username.user.s;
	username.len = cred->digest.username.user.len;

	trim(&username);
	
	*hash = ds_get_hash(&username, NULL);
	
	return 0;
}


int ds_hash_pvar(struct sip_msg *msg, unsigned int *hash)
{
	/* The String to create the hash */
	str hash_str = {0, 0};
	
	if(msg==NULL || hash == NULL || hash_param_model == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	if (pv_printf_s(msg, hash_param_model, &hash_str)<0) {
		LM_ERR("error - cannot print the format\n");
		return -1;
	}

	/* Remove empty spaces */
	trim(&hash_str);
	if (hash_str.len <= 0) {
		LM_ERR("String is empty!\n");
		return -1;
	}
	LM_DBG("Hashing %.*s!\n", hash_str.len, hash_str.s);

	*hash = ds_get_hash(&hash_str, NULL);
	
	return 0;
}

static inline int ds_get_index(int group, ds_set_p *index)
{
	ds_set_p si = NULL;
	
	if(index==NULL || group<0 || _ds_list==NULL)
		return -1;
	
	/* get the index of the set */
	si = _ds_list;
	while(si)
	{
		if(si->id == group)
		{
			*index = si;
			break;
		}
		si = si->next;
	}

	if(si==NULL)
	{
		LM_ERR("destination set [%d] not found\n", group);
		return -1;
	}

	return 0;
}

static inline int ds_update_dst(struct sip_msg *msg, str *uri, int mode)
{
	struct action act;
	switch(mode)
	{
		case 1:
			act.type = SET_HOSTPORT_T;
			act.elem[0].type = STRING_ST;
			if(uri->len>4 
					&& strncasecmp(uri->s,"sip:",4)==0)
				act.elem[0].u.string = uri->s+4;
			else
				act.elem[0].u.string = uri->s;
			act.next = 0;
	
			if (do_action(&act, msg) < 0) {
				LM_ERR("error while setting host\n");
				return -1;
			}
			if(route_type==FAILURE_ROUTE)
			{
				if (append_branch(msg, 0, 0, 0, Q_UNSPECIFIED, 0, 0)!=1 )
				{
					LM_ERR("append_branch action failed\n");
					return -1;
				}
			}
		break;
		default:
			if(route_type==FAILURE_ROUTE)
			{
				if (append_branch(msg, 0, uri, 0, Q_UNSPECIFIED, 0, 0)!=1 )
				{
					LM_ERR("append_branch action failed\n");
					return -1;
				}
			} else {
				if (set_dst_uri(msg, uri) < 0) {
					LM_ERR("error while setting dst uri\n");
					return -1;
				}	
			}
		break;
	}
	return 0;
}

/**
 *
 */
int ds_select_dst(struct sip_msg *msg, int set, int alg, int mode)
{
	int i, cnt;
	unsigned int hash;
	int_str avp_val;
	ds_set_p idx = NULL;

	if(msg==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LM_ERR("no destination sets\n");
		return -1;
	}

	if((mode==0) && (ds_force_dst==0)
			&& (msg->dst_uri.s!=NULL || msg->dst_uri.len>0))
	{
		LM_ERR("destination already set [%.*s]\n", msg->dst_uri.len,
				msg->dst_uri.s);
		return -1;
	}
	

	/* get the index of the set */
	if(ds_get_index(set, &idx)!=0)
	{
		LM_ERR("destination set [%d] not found\n", set);
		return -1;
	}
	
	LM_DBG("set [%d]\n", set);

	hash = 0;
	switch(alg)
	{
		case 0:
			if(ds_hash_callid(msg, &hash)!=0)
			{
				LM_ERR("can't get callid hash\n");
				return -1;
			}
		break;
		case 1:
			if(ds_hash_fromuri(msg, &hash)!=0)
			{
				LM_ERR("can't get From uri hash\n");
				return -1;
			}
		break;
		case 2:
			if(ds_hash_touri(msg, &hash)!=0)
			{
				LM_ERR("can't get To uri hash\n");
				return -1;
			}
		break;
		case 3:
			if (ds_hash_ruri(msg, &hash)!=0)
			{
				LM_ERR("can't get ruri hash\n");
				return -1;
			}
		break;
		case 4:
			hash = idx->last;
			idx->last = (idx->last+1) % idx->nr;
		break;
		case 5:
			i = ds_hash_authusername(msg, &hash);
			switch (i)
			{
				case 0:
					/* Authorization-Header found: Nothing to be done here */
				break;
				case 1:
					/* No Authorization found: Use round robin */
					hash = idx->last;
					idx->last = (idx->last+1) % idx->nr;
				break;
				default:
					LM_ERR("can't get authorization hash\n");
					return -1;
				break;
			}
		break;
		case 6:
			hash = rand() % idx->nr;
		break;
		case 7:
			if (ds_hash_pvar(msg, &hash)!=0)
			{
				LM_ERR("can't get PV hash\n");
				return -1;
			}
		break;		
		default:
			LM_WARN("algo %d not implemented - using first entry...\n", alg);
			hash = 0;
	}

	LM_DBG("alg hash [%u]\n", hash);
	cnt = 0;

	if(ds_use_default!=0 && idx->nr!=1)
		hash = hash%(idx->nr-1);
	else
		hash = hash%idx->nr;
	i=hash;
	while ((idx->dlist[i].flags & DS_INACTIVE_DST)
			|| (idx->dlist[i].flags & DS_PROBING_DST))
	{
		if(ds_use_default!=0)
			i = (i+1)%(idx->nr-1);
		else
			i = (i+1)%idx->nr;
		if(i==hash)
		{
			if(ds_use_default!=0)
			{
				i = idx->nr-1;
			} else {
				return -1;
			}
		}
	}

	hash = i;

	if(ds_update_dst(msg, &idx->dlist[hash].uri, mode)!=0)
	{
		LM_ERR("cannot set dst addr\n");
		return -1;
	}
	/* if alg is round-robin then update the shortcut to next to be used */
	if(alg==4)
		idx->last = (hash+1) % idx->nr;
	
	LM_DBG("selected [%d-%d/%d] <%.*s>\n", alg, set, hash,
			idx->dlist[hash].uri.len, idx->dlist[hash].uri.s);

	if(!(ds_flags&DS_FAILOVER_ON))
		return 1;

	if(dst_avp_name.n!=0)
	{
		if(ds_use_default!=0 && hash!=idx->nr-1)
		{
			avp_val.s = idx->dlist[idx->nr-1].uri;
			if(add_avp(AVP_VAL_STR|dst_avp_type, dst_avp_name, avp_val)!=0)
				return -1;
			cnt++;
		}
	
		/* add to avp */

		for(i=hash-1; i>=0; i--)
		{	
			if((idx->dlist[i].flags & DS_INACTIVE_DST)
					|| (ds_use_default!=0 && i==(idx->nr-1)))
				continue;
			LM_DBG("using entry [%d/%d]\n", set, i);
			avp_val.s = idx->dlist[i].uri;
			if(add_avp(AVP_VAL_STR|dst_avp_type, dst_avp_name, avp_val)!=0)
				return -1;
			cnt++;
		}

		for(i=idx->nr-1; i>hash; i--)
		{	
			if((idx->dlist[i].flags & DS_INACTIVE_DST)
					|| (ds_use_default!=0 && i==(idx->nr-1)))
				continue;
			LM_DBG("using entry [%d/%d]\n", set, i);
			avp_val.s = idx->dlist[i].uri;
			if(add_avp(AVP_VAL_STR|dst_avp_type, dst_avp_name, avp_val)!=0)
				return -1;
			cnt++;
		}
	
		/* add to avp the first used dst */
		avp_val.s = idx->dlist[hash].uri;
		if(add_avp(AVP_VAL_STR|dst_avp_type, dst_avp_name, avp_val)!=0)
			return -1;
		cnt++;
	}

	if(grp_avp_name.n!=0)
	{
		/* add to avp the group id */
		avp_val.n = set;
		if(add_avp(grp_avp_type, grp_avp_name, avp_val)!=0)
			return -1;
	}

	if(cnt_avp_name.n!=0)
	{
		/* add to avp the number of dst */
		avp_val.n = cnt;
		if(add_avp(cnt_avp_type, cnt_avp_name, avp_val)!=0)
			return -1;
	}
	
	return 1;
}

int ds_next_dst(struct sip_msg *msg, int mode)
{
	struct usr_avp *avp;
	struct usr_avp *prev_avp;
	int_str avp_value;
	
	if(!(ds_flags&DS_FAILOVER_ON) || dst_avp_name.n==0)
	{
		LM_WARN("failover support disabled\n");
		return -1;
	}


	prev_avp = search_first_avp(dst_avp_type, dst_avp_name, &avp_value, 0);
	if(prev_avp==NULL)
		return -1; /* used avp deleted -- strange */

	avp = search_next_avp(prev_avp, &avp_value);
	destroy_avp(prev_avp);
	if(avp==NULL || !(avp->flags&AVP_VAL_STR))
		return -1; /* no more avps or value is int */
	
	if(ds_update_dst(msg, &avp_value.s, mode)!=0)
	{
		LM_ERR("cannot set dst addr\n");
		return -1;
	}
	LM_DBG("using [%.*s]\n", avp_value.s.len, avp_value.s.s);
	
	return 1;
}

int ds_mark_dst(struct sip_msg *msg, int mode)
{
	int group, ret;
	struct usr_avp *prev_avp;
	int_str avp_value;
	
	if(!(ds_flags&DS_FAILOVER_ON))
	{
		LM_WARN("failover support disabled\n");
		return -1;
	}

	prev_avp = search_first_avp(grp_avp_type, grp_avp_name, &avp_value, 0);
	
	if(prev_avp==NULL || prev_avp->flags&AVP_VAL_STR)
		return -1; /* grp avp deleted -- strange */
	group = avp_value.n;
	
	prev_avp = search_first_avp(dst_avp_type, dst_avp_name, &avp_value, 0);
	
	if(prev_avp==NULL || !(prev_avp->flags&AVP_VAL_STR))
		return -1; /* dst avp deleted -- strange */
	
	if(mode==1) {
		ret = ds_set_state(group, &avp_value.s, 
				DS_INACTIVE_DST|DS_PROBING_DST, 0);
	} else if(mode==2) {
		ret = ds_set_state(group, &avp_value.s, DS_PROBING_DST, 1);
		if (ret == 0) ret = ds_set_state(group, &avp_value.s,
				DS_INACTIVE_DST, 0);
	} else {
		ret = ds_set_state(group, &avp_value.s, DS_INACTIVE_DST, 1);
		if (ret == 0) ret = ds_set_state(group, &avp_value.s,
				DS_PROBING_DST, 0);
	}
	
	LM_DBG("mode [%d] grp [%d] dst [%.*s]\n", mode, group, avp_value.s.len,
			avp_value.s.s);
	
	return (ret==0)?1:-1;
}

int ds_set_state(int group, str *address, int state, int type)
{
	int i=0;
	ds_set_p idx = NULL;

	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LM_ERR("the list is null\n");
		return -1;
	}
	
	/* get the index of the set */
	if(ds_get_index(group, &idx)!=0)
	{
		LM_ERR("destination set [%d] not found\n", group);
		return -1;
	}

	while(i<idx->nr)
	{
		if(idx->dlist[i].uri.len==address->len 
				&& strncasecmp(idx->dlist[i].uri.s, address->s,
					address->len)==0)
		{
			
			/* remove the Probing/Inactive-State? Set the fail-count to 0. */
			if (state == DS_PROBING_DST) {
				if (type) {
					if (idx->dlist[i].flags & DS_INACTIVE_DST) {
						LM_INFO("Ignoring the request to set this destination"
								" to probing: It is already inactive!\n");
						return 0;
					}
					
					idx->dlist[i].failure_count++;
					/* Fire only, if the Threshold is reached. */
					if (idx->dlist[i].failure_count 
							< probing_threshhold) return 0;
					if (idx->dlist[i].failure_count
							> probing_threshhold) 
						idx->dlist[i].failure_count
							= probing_threshhold;				
				}
			}
			/* Reset the Failure-Counter */
			if ((state & DS_RESET_FAIL_DST) > 0) {
				idx->dlist[i].failure_count = 0;
				state &= ~DS_RESET_FAIL_DST;
			}
			
			if(type)
				idx->dlist[i].flags |= state;
			else
				idx->dlist[i].flags &= ~state;
				
			return 0;
		}
		i++;
	}

	return -1;
}

int ds_print_list(FILE *fout)
{
	int j;
	ds_set_p list;
		
	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LM_ERR("no destination sets\n");
		return -1;
	}
	
	fprintf(fout, "\nnumber of destination sets: %d\n", _ds_list_nr);
	
	for(list = _ds_list; list!= NULL; list= list->next)
	{
		for(j=0; j<list->nr; j++)
		{
			fprintf(fout, "\n set #%d\n", list->id);
		
			if (list->dlist[j].flags&DS_INACTIVE_DST)
  				fprintf(fout, "    Disabled         ");
  			else if (list->dlist[j].flags&DS_PROBING_DST)
  				fprintf(fout, "    Probing          ");
  			else {
  				fprintf(fout, "    Active");
  				/* Optional: Print the tries for this host. */
  				if (list->dlist[j].failure_count > 0) {
  					fprintf(fout, " (Fail %d/%d)",
  							list->dlist[j].failure_count,
 							probing_threshhold);
  				} else {
  					fprintf(fout, "           ");
  				}
  			}
  
  			fprintf(fout, "   %.*s\n",
  				list->dlist[j].uri.len, list->dlist[j].uri.s);		
		}
	}
	return 0;
}


/* Checks, if the request (sip_msg *_m) comes from a host in a group
 * (group-id or -1 for all groups)
 */
int ds_is_from_list(struct sip_msg *_m, int group)
{
	pv_value_t val;
	ds_set_p list;
	int j;

	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_INT|PV_TYPE_INT;

	for(list = _ds_list; list!= NULL; list= list->next)
	{
		// LM_ERR("list id: %d (n: %d)\n", list->id, list->nr);
		if ((group == -1) || (group == list->id))
		{
			for(j=0; j<list->nr; j++)
			{
				// LM_ERR("port no: %d (%d)\n", list->dlist[j].port, j);
				if (ip_addr_cmp(&_m->rcv.src_ip, &list->dlist[j].ip_address)
						&& (list->dlist[j].port==0
						|| _m->rcv.src_port == list->dlist[j].port))
				{
					if(group==-1 && ds_setid_pvname.s!=0)
					{
						val.ri = list->id;
						if(ds_setid_pv.setf(_m, &ds_setid_pv.pvp,
								(int)EQ_T, &val)<0)
						{
							LM_ERR("setting PV failed\n");
							return -2;
						}
					}
					return 1;
				}
			}
		}
	}
	return -1;
}


int ds_print_mi_list(struct mi_node* rpl)
{
	int len, j;
	char* p;
	char c;
	ds_set_p list;
	struct mi_node* node = NULL;
	struct mi_node* set_node = NULL;
	struct mi_attr* attr = NULL;
	
	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LM_ERR("no destination sets\n");
		return  0;
	}

	p= int2str(_ds_list_nr, &len); 
	node = add_mi_node_child(rpl, MI_DUP_VALUE, "SET_NO",6, p, len);
	if(node== NULL)
		return -1;

	for(list = _ds_list; list!= NULL; list= list->next)
	{
		p = int2str(list->id, &len);
		set_node= add_mi_node_child(rpl, MI_DUP_VALUE,"SET", 3, p, len);
		if(set_node == NULL)
			return -1;

		for(j=0; j<list->nr; j++)
  		{
  			node= add_mi_node_child(set_node, 0, "URI", 3,
  					list->dlist[j].uri.s, list->dlist[j].uri.len);
  			if(node == NULL)
  				return -1;
  
  			if (list->dlist[j].flags & DS_INACTIVE_DST) c = 'I';
  			else if (list->dlist[j].flags & DS_PROBING_DST) c = 'P';
  			else c = 'A';
  
  			attr = add_mi_attr (node, MI_DUP_VALUE, "flag",4, &c, 1);
  			if(attr == 0)
  				return -1;
  
 		}
	}

	return 0;
}

/*! \brief
 * Callback-Function for the OPTIONS-Request
 * This Function is called, as soon as the Transaction is finished
 * (e. g. a Response came in, the timeout was hit, ...)
 * 
 */ 
static void ds_options_callback( struct cell *t, int type,
		struct tmcb_params *ps )
{
	int group = 0;
	str uri = {0, 0};
	/* The Param does contain the group, in which the failed host
	 * can be found.*/
	if (!*ps->param)
	{
		LM_DBG("No parameter provided, OPTIONS-Request was finished"
				" with code %d\n", ps->code);
		return;
	}
	/* The param is a (void*) Pointer, so we need to dereference it and
	 *  cast it to an int. */
	group = (int)(long)(*ps->param);
	/* The SIP-URI is taken from the Transaction.
	 * Remove the "To: " (s+4) and the trailing new-line (s - 4 (To: )
	 * - 2 (\r\n)). */
	uri.s = t->to.s + 4;
	uri.len = t->to.len - 6;
	LM_DBG("OPTIONS-Request was finished with code %d (to %.*s, group %d)\n",
			ps->code, uri.len, uri.s, group);
	/* ps->code contains the result-code of the request.
	 * 
	 * We accept both a "200 OK", "501 Not supported" and "403" as an
	 * successful reply.
	 *   501: Cisco-Gateways reply with a "501 Not supported" to the request.
	 *   403: Aastra-Gateways reply with a "403" to the request. */
	if ((ps->code == 200) || (ps->code == 501) || (ps->code == 403))
	{
		/* Set the according entry back to "Active":
		 *  remove the Probing/Inactive Flag and reset the failure counter. */
		if (ds_set_state(group, &uri,
					DS_INACTIVE_DST|DS_PROBING_DST|DS_RESET_FAIL_DST, 0) != 0)
		{
			LM_ERR("Setting the state failed (%.*s, group %d)\n", uri.len,
					uri.s, group);
		}
	}
	if(ds_probing_mode==1 && ps->code == 407)
	{
		if (ds_set_state(group, &uri, DS_PROBING_DST, 1) != 0)
		{
			LM_ERR("Setting the probing state failed (%.*s, group %d)\n",
					uri.len, uri.s, group);
		}
	}

	return;
}

/*! \brief
 * Timer for checking inactive destinations
 * 
 * This timer is regularly fired.
 */
void ds_check_timer(unsigned int ticks, void* param)
{
	
	struct socket_info* send_sock = 0;
	struct sip_uri curi;
	union sockaddr_union to;
	struct hostent* he;
	unsigned short proto;
	dlg_t * dialog;	
	ds_set_p list;
	int j;
	
	/* Check for the list. */
	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LM_ERR("no destination sets\n");
		return;
	}

	/* Iterate over the groups and the entries of each group: */
	for(list = _ds_list; list!= NULL; list= list->next)
	{
		for(j=0; j<list->nr; j++) 
		{
			/* If the Flag of the entry has "Probing set, send a probe:	*/
			if (ds_probing_mode==1 ||
					(list->dlist[j].flags&DS_PROBING_DST) != 0)
			{
				LM_DBG("probing set #%d, URI %.*s\n", list->id,
						list->dlist[j].uri.len, list->dlist[j].uri.s);
				
				/* Parse the URI of the destination, we want to probe (we need
				 * more information about the destination) */
				if (parse_uri(list->dlist[j].uri.s,
							list->dlist[j].uri.len, &curi) < 0)
				{
					LM_ERR("unable to parse URI (%.*s)\n",
							list->dlist[j].uri.len, list->dlist[j].uri.s);
					continue;
				}
				
				/* If the port is not set, we use the default-SIP-Port.*/
				if (curi.port_no == 0)
					curi.port_no = SIP_PORT;

				/* Resolve the Hostnamme */
				proto = curi.proto;
				he = sip_resolvehost(&curi.host, &curi.port_no, &proto, 
						(curi.type==SIPS_URI_T)?1:0 , 0);
				if (he == NULL)
				{
					LM_ERR("can't resolve_host\n");
					continue;
				}
				
				/* Determine the sending-socket for sending the OPTIONS-Request
				 * Maybe, later, provide the possibility to use a specific
				 * socket */
				hostent2su(&to, he, 0, curi.port_no);
				if (send_sock==0) {
					/* send_sock=force_socket ? force_socket : 
						get_send_socket(0, &to, PROTO_UDP); */
					send_sock=get_send_socket(0, &to, PROTO_UDP);
				}
				if (send_sock == NULL) {
					LM_ERR("can't get sending socket\n");
					continue;
				}
				
				/* Execute the Dialog using the "req_outside"-Method of the
				 * TM-Module.
				 * req_outside(str* m, str* t, str* f, str* h, str* b,
				 *		dlg_t** d, transaction_cb c, void* cp); */
				if (tmb.t_request_outside(&ds_ping_method,
							&list->dlist[j].uri,
							&ds_ping_from,
							NULL,
							NULL,
							&dialog,
							ds_options_callback,
							(void*)(long)list->id) < 0) {
					LM_ERR("unable to execute dialog\n");
				}
			}
		}
	}
}
