/**
 * $Id$
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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
 */

/**
 * History
 * -------
 * 2004-07-31  first version, by dcm
 * 2005-04-22  added ruri  & to_uri hashing (andrei)
 * 2005-07-19  fixup_int_12 now returns fparam_t*, updated (mma)
 * 2007-02-22  switched to core's case insensitive hash
 *             added 2 selectable hash functions (andrei)
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../trim.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../sr_module.h"
#include "../../hashes.h"

#include "dispatch.h"

typedef struct _ds_setidx
{
	int id;
	int index;
	struct _ds_setidx *next;
} ds_setidx_t, *ds_setidx_p;


typedef struct _ds_dest
{
	str uri;
	struct _ds_dest *next;
} ds_dest_t, *ds_dest_p;

typedef struct _ds_set
{
	int id;
	int nr;
	int index;
	ds_dest_p dlist;
	struct _ds_set *next;
} ds_set_t, *ds_set_p;

extern int force_dst;

ds_setidx_p _ds_index = NULL;
ds_set_p _ds_list = NULL;

/**
 *
 */
int ds_load_list(char *lfile)
{
	char line[256], *p;
	FILE *f = NULL;
	int id, i, j, setn;
	str uri;
	struct sip_uri puri;
	ds_dest_p dp = NULL, dp0 = NULL;
	ds_set_p  sp = NULL, sp0 = NULL;
	ds_setidx_p si = NULL, si0 = NULL;
	
	if(lfile==NULL || strlen(lfile)<=0)
	{
		LOG(L_ERR, "DISPATCHER:ds_load_list: bad list file\n");
		return -1;
	}

	f = fopen(lfile, "r");
	if(f==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_load_list: can't open list file [%s]\n",
				lfile);
		return -1;
		
	}

	id = setn = 0;
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
			LOG(L_ERR, "DISPATCHER:ds_load_list: bad line [%s]\n", line);
			goto error;
		}

		/* get uri */
		uri.s = p;
		while(*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n' && *p!='#')
			p++;
		uri.len = p-uri.s;

		/* check uri */
		if(parse_uri(uri.s, uri.len, &puri)!=0)
		{
			LOG(L_ERR, "DISPATCHER:ds_load_list: bad uri [%.*s]\n",
					uri.len, uri.s);
			goto error;
		}
		
		/* check index */
		si = _ds_index;
		while(si)
		{
			if(si->id == id)
				break;
			si = si->next;
		}

		if(si==NULL)
		{
			si = (ds_setidx_p)pkg_malloc(sizeof(ds_setidx_t));
			if(si==NULL)
			{
				LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory\n");
				goto error;
			}
			memset(si, 0, sizeof(ds_setidx_t));
			si->id = id;
			si->next = _ds_index;
			if(_ds_index!=NULL)
				si->index = _ds_index->index + 1;
			
			_ds_index = si;
		}
		
		/* get dest set */
		sp = _ds_list;
		while(sp)
		{
			if(sp->id == id)
				break;
			sp = sp->next;
		}

		if(sp==NULL)
		{
			sp = (ds_set_p)pkg_malloc(sizeof(ds_set_t));
			if(sp==NULL)
			{
				LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory.\n");
				goto error;
			}
			memset(sp, 0, sizeof(ds_set_t));
			sp->next = _ds_list;
			_ds_list = sp;
			setn++;
		}
		sp->id = id;
		sp->nr++;
		sp->index = si->index;

		/* store uri */
		dp = (ds_dest_p)pkg_malloc(sizeof(ds_dest_t));
		if(dp==NULL)
		{
			LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory!\n");
			goto error;
		}
		memset(dp, 0, sizeof(ds_dest_t));

		dp->uri.s = (char*)pkg_malloc(uri.len+1);
		if(dp->uri.s==NULL)
		{
			LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory!!\n");
			pkg_free(dp);
			goto error;
		}
		strncpy(dp->uri.s, uri.s, uri.len);
		dp->uri.s[uri.len]='\0';
		dp->uri.len = uri.len;

		dp->next = sp->dlist;
		sp->dlist = dp;
		DBG("DISPATCHER:ds_load_list: dest [%d/%d/%d] <%.*s>\n", sp->index,
				sp->id, sp->nr, dp->uri.len, dp->uri.s);
		
next_line:
		p = fgets(line, 256, f);
	}
		
	fclose(f);
	f = NULL;
	
	DBG("DISPATCHER:ds_load_list: found [%d] dest sets\n", setn);
	
	/* re-index destination sets for fast access */
	sp0 = (ds_set_p)pkg_malloc(setn*sizeof(ds_set_t));
	if(sp0==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory!!\n");
		goto error;
	}
	memset(sp0, 0, setn*sizeof(ds_set_t));
	sp = _ds_list;
	for(i=setn-1; i>=0; i--)
	{
		memcpy(&sp0[i], sp, sizeof(ds_set_t));
		if(i==setn-1)
			sp0[i].next = NULL;
		else
			sp0[i].next = &sp0[i+1];
		dp0 = (ds_dest_p)pkg_malloc(sp0[i].nr*sizeof(ds_dest_t));
		if(dp0==NULL)
		{
			LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory!\n");
			ds_destroy_list();
			goto error;
		}
		memset(dp0, 0, sp0[i].nr*sizeof(ds_dest_t));
		dp = sp0[i].dlist;
		for(j=sp0[i].nr-1; j>=0; j--)
		{
			memcpy(&dp0[j], dp, sizeof(ds_dest_t));
			if(j==sp0[j].nr-1)
				dp0[j].next = NULL;
			else
				dp0[j].next = &dp0[j+1];
			dp = dp->next;
		}
		sp0[i].dlist = dp0;
		sp = sp->next;
	}
	
	sp = _ds_list;
	_ds_list = sp0;
	while(sp)
	{
		dp = sp->dlist;
		while(dp)
		{
			dp0 = dp;
			dp = dp->next;
			pkg_free(dp0);
		}
		sp0 = sp;
		sp = sp->next;
		pkg_free(sp0);
	}
	
	return 0;

error:
	if(f!=NULL)
		fclose(f);

	si = _ds_index;
	while(si)
	{
		si0 = si;
		si = si->next;
		pkg_free(si0);
	}
	_ds_index = NULL;

	sp = _ds_list;
	while(sp)
	{
		dp = sp->dlist;
		while(dp)
		{
			if(dp->uri.s!=NULL)
				pkg_free(dp->uri.s);
			dp->uri.s=NULL;
			dp0 = dp;
			dp = dp->next;
			pkg_free(dp0);
		}
		sp0 = sp;
		sp = sp->next;
		pkg_free(sp0);
	}
	return -1;
}

/**
 *
 */
int ds_destroy_list()
{
	int i;
	ds_set_p  sp = NULL;
	ds_setidx_p si = NULL, si0 = NULL;
	
	sp = _ds_list;
	while(sp)
	{
		for(i=0; i<sp->nr; i++)
		{
			if(sp->dlist[i].uri.s!=NULL)
			{
				pkg_free(sp->dlist[i].uri.s);
				sp->dlist[i].uri.s = NULL;
			}
		}
		pkg_free(sp->dlist);
		sp = sp->next;
	}
	if (_ds_list) pkg_free(_ds_list);
	
	si = _ds_index;
	while(si)
	{
		si0 = si;
		si = si->next;
		pkg_free(si0);
	}
	_ds_index = NULL;

	return 0;
}



/* hash function 1, 1 param */
static unsigned int hash1_f1(str* x)
{
	return get_hash1_case_raw(x->s, x->len);
}

/* hash function 1, 2 params */
static unsigned int hash2_f1(str* x, str* y)
{
	return get_hash2_case_raw(x, y);
}

/* hash function 2, 1 param */
static unsigned int hash1_f2(str* x)
{
	return get_hash1_case_raw2(x->s, x->len);
}

/* hash function 2, 2 params */
static unsigned int hash2_f2(str* x, str* y)
{
	return get_hash2_case_raw2(x, y);
}


/* hash function pointers */
static unsigned int (*ds_get_hash1)(str *x)=hash1_f1;
static unsigned int (*ds_get_hash2)(str *x, str *y)=hash2_f1;


/* use hash function n
 * if hash n not defined, keep the default one and return -1 */
int ds_set_hash_f(int n)
{
	switch(n){
		case 0:
			break;
		case 1:
			ds_get_hash1=hash1_f2;
			ds_get_hash2=hash2_f2;
			break;
		default:
			return -1;
	}
	return 0;
}

/*
 * gets the part of the uri we will use as a key for hashing
 * params:  key1       - will be filled with first part of the key
 *                       (uri user or "" if no user)
 *          key2       - will be filled with the second part of the key
 *                       (uri host:port)
 *          uri        - str with the whole uri
 *          parsed_uri - struct sip_uri pointer with the parsed uri
 *                       (it must point inside uri). It can be null
 *                       (in this case the uri will be parsed internally).
 *          flags  -    if & DS_HASH_USER_ONLY, only the user part of the uri
 *                      will be used
 * returns: -1 on error, 0 on success
 */
static inline int get_uri_hash_keys(str* key1, str* key2,
							str* uri, struct sip_uri* parsed_uri, int flags)
{
	struct sip_uri tmp_p_uri; /* used only if parsed_uri==0 */
	
	if (parsed_uri==0){
		if (parse_uri(uri->s, uri->len, &tmp_p_uri)<0){
			LOG(L_ERR, "DISPATCHER: get_uri_hash_keys: invalid uri %.*s\n",
					uri->len, uri->len?uri->s:"");
			goto error;
		}
		parsed_uri=&tmp_p_uri;
	}
	/* uri sanity checks */
	if (parsed_uri->host.s==0){
			LOG(L_ERR, "DISPATCHER: get_uri_hash_keys: invalid uri, no host"
					   "present: %.*s\n", uri->len, uri->len?uri->s:"");
			goto error;
	}
	
	/* we want: user@host:port if port !=5060
	 *          user@host if port==5060
	 *          user if the user flag is set*/
	*key1=parsed_uri->user;
	key2->s=0;
	key2->len=0;
	if ((!(flags & (DS_HASH_USER_ONLY | DS_HASH_USER_OR_HOST))) ||
		((key1->s==0) && (flags & DS_HASH_USER_OR_HOST))){
		/* key2=host */
		*key2=parsed_uri->host;
		/* add port if needed */
		if (parsed_uri->port.s!=0){ /* uri has a port */
			/* skip port if == 5060 or sips and == 5061 */
			if (parsed_uri->port_no !=
					((parsed_uri->type==SIPS_URI_T)?SIPS_PORT:SIP_PORT))
				key2->len+=parsed_uri->port.len+1 /* ':' */;
		}
	}
	if (key1->s==0 && (flags & DS_HASH_USER_ONLY)){
		LOG(L_WARN, "DISPATCHER: get_uri_hash_keys: empty username in:"
					" %.*s\n", uri->len, uri->len?uri->s:"");
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
		LOG(L_ERR, "DISPATCHER:ds_hash_fromuri: bad parameters\n");
		return -1;
	}
	
	if(parse_from_header(msg)==-1)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_fromuri:ERROR cannot parse From hdr\n");
		return -1;
	}
	
	if(msg->from==NULL || get_from(msg)==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_fromuri:ERROR cannot get From uri\n");
		return -1;
	}
	
	from   = get_from(msg)->uri;
	trim(&from);
	if (get_uri_hash_keys(&key1, &key2, &from, 0, ds_flags)<0)
		return -1;
	*hash = ds_get_hash2(&key1, &key2);
	
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
		LOG(L_ERR, "DISPATCHER:ds_hash_touri: bad parameters\n");
		return -1;
	}
	if ((msg->to==0) && ((parse_headers(msg, HDR_TO_F, 0)==-1) ||
				(msg->to==0)))
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_touri:ERROR cannot parse To hdr\n");
		return -1;
	}
	
	
	to   = get_to(msg)->uri;
	trim(&to);
	
	if (get_uri_hash_keys(&key1, &key2, &to, 0, ds_flags)<0)
		return -1;
	*hash = ds_get_hash2(&key1, &key2);
	
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
		LOG(L_ERR, "DISPATCHER:ds_hash_callid: bad parameters\n");
		return -1;
	}
	
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1) ||
				(msg->callid==NULL)) )
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_callid:ERROR cannot parse Call-Id\n");
		return -1;
	}
	
	cid.s   = msg->callid->body.s;
	cid.len = msg->callid->body.len;
	trim(&cid);
	
	*hash = ds_get_hash1(&cid);
	
	return 0;
}



int ds_hash_ruri(struct sip_msg *msg, unsigned int *hash)
{
	str* uri;
	str key1;
	str key2;
	
	
	if(msg==NULL || hash == NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_ruri: bad parameters\n");
		return -1;
	}
	if (parse_sip_msg_uri(msg)<0){
		LOG(L_ERR, "DISPATCHER: ds_hash_ruri: ERROR: bad request uri\n");
		return -1;
	}
	
	uri=GET_RURI(msg);
	if (get_uri_hash_keys(&key1, &key2, uri, &msg->parsed_uri, ds_flags)<0)
		return -1;
	
	*hash = ds_get_hash2(&key1, &key2);
	return 0;
}

static int set_new_uri_simple(struct sip_msg *msg, str *uri)
{
	if (msg->new_uri.s)
	{
		pkg_free(msg->new_uri.s);
		msg->new_uri.len=0;
	}

	msg->parsed_uri_ok=0;
	msg->new_uri.s = (char*)pkg_malloc(uri->len+1);
	if (msg->new_uri.s==0)
	{
		ERR("no more pkg memory\n");
		return -1;
	}
	memcpy(msg->new_uri.s, uri->s, uri->len);
	msg->new_uri.s[uri->len]=0;
	msg->new_uri.len=uri->len;
	return 0;
}

static int set_new_uri_with_user(struct sip_msg *msg, str *uri, str *user)
{
	struct sip_uri dst;
	int start_len, stop_len;
	
	if (parse_uri(uri->s, uri->len, &dst) < 0) {
		ERR("can't parse destination URI\n");
		return -1;
	}
	if ((!dst.host.s) || (dst.host.len <= 0)) {
		ERR("destination URI host not set\n");
		return -1;
	}
	if (dst.user.s && (dst.user.len > 0)) {
		DBG("user already exists\n");
		/* don't replace the user */
		return set_new_uri_simple(msg, uri);
	}
	
	if (msg->new_uri.s)
	{
		pkg_free(msg->new_uri.s);
		msg->new_uri.len=0;
	}
	
	start_len = dst.host.s - uri->s;
	stop_len = uri->len - start_len;
	
	msg->parsed_uri_ok=0;
	msg->new_uri.s = (char*)pkg_malloc(uri->len+1+user->len+1);
	if (msg->new_uri.s==0)
	{
		ERR("no more pkg memory\n");
		return -1;
	}
	memcpy(msg->new_uri.s, uri->s, start_len);
	memcpy(msg->new_uri.s + start_len, user->s, user->len);
	*(msg->new_uri.s + start_len + user->len) = '@';
	memcpy(msg->new_uri.s + start_len + user->len + 1, dst.host.s, stop_len);
	
	msg->new_uri.len=uri->len + user->len + 1;
	msg->new_uri.s[msg->new_uri.len]=0;
	
	return 0;
}

static int set_new_uri(struct sip_msg *msg, str *uri)
{
	struct to_body* to;
	struct sip_uri to_uri;
	
	/* we need to leave original user */
	to = get_to(msg);
	if (to) {
		if (parse_uri(to->uri.s, to->uri.len, &to_uri) >= 0) {
			if (to_uri.user.s && (to_uri.user.len > 0)) {
				return set_new_uri_with_user(msg, uri, &to_uri.user);
			}
		}
	}

	return set_new_uri_simple(msg, uri);
}

/**
 *
 */
int ds_select_dst_impl(struct sip_msg *msg, char *set, char *alg, int set_new)
{
	int a, s, idx;
	ds_setidx_p si = NULL;
	unsigned int hash;

	if(msg==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_select_dst: bad parameters\n");
		return -1;
	}
	
	if(_ds_list==NULL || _ds_index==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_select_dst: no destination sets\n");
		return -1;
	}

	if((force_dst==0) && (msg->dst_uri.s!=NULL || msg->dst_uri.len>0))
	{
		LOG(L_ERR,
			"DISPATCHER:ds_select_dst: destination already set [%.*s]\n",
			msg->dst_uri.len, msg->dst_uri.s);
		return -1;
	}
	
	get_int_fparam(&s, msg, (fparam_t*)set);
	get_int_fparam(&a, msg, (fparam_t*)alg);

	/* get the index of the set */
	si = _ds_index;
	while(si)
	{
		if(si->id == s)
		{
			idx = si->index;
			break;
		}
		si = si->next;
	}

	if(si==NULL)
	{
		LOG(L_ERR,
			"DISPATCHER:ds_select_dst: destination set [%d] not found\n",s);
		return -1;
	}

	DBG("DISPATCHER:ds_select_dst: set index [%d]\n", idx);

	hash = 0;
	switch(a)
	{
		case 0:
			if(ds_hash_callid(msg, &hash)!=0)
			{
				LOG(L_ERR,
					"DISPATCHER:ds_select_dst: can't get callid hash\n");
				return -1;
			}
		break;
		case 1:
			if(ds_hash_fromuri(msg, &hash)!=0)
			{
				LOG(L_ERR,
					"DISPATCHER:ds_select_dst: can't get From uri hash\n");
				return -1;
			}
		break;
		case 2:
			if(ds_hash_touri(msg, &hash)!=0)
			{
				LOG(L_ERR,
					"DISPATCHER:ds_select_dst: can't get To uri hash\n");
				return -1;
			}
		break;
		case 3:
			if (ds_hash_ruri(msg, &hash)!=0)
			{
				LOG(L_ERR,	
					"DISPATCHER:ds_select_dst: can't get ruri hash\n");
				return -1;
			}
		break;
		default:
			LOG(L_WARN,
					"WARNING: ds_select_dst: algo %d not implemented"
					" using callid hashing instead...\n", a);
			hash = 0;
	}

	DBG("DISPATCHER:ds_select_dst: alg hash [%u]\n", hash);

	hash = hash%_ds_list[idx].nr;

	if (!set_new) {
		if (set_dst_uri(msg, &_ds_list[idx].dlist[hash].uri) < 0) {
			LOG(L_ERR, "DISPATCHER:dst_select_dst: Error while setting dst_uri\n");
			return -1;
		}

		DBG("DISPATCHER:ds_select_dst: selected [%d-%d/%d/%d] <%.*s>\n",
				a, s, idx, hash, msg->dst_uri.len, msg->dst_uri.s);
	}
	else {
		if (set_new_uri(msg, &_ds_list[idx].dlist[hash].uri) < 0) {
			LOG(L_ERR, "DISPATCHER:dst_select_dst: Error while setting new_uri\n");
			return -1;
		}
		DBG("DISPATCHER:ds_select_new: selected [%d-%d/%d/%d] <%.*s>\n",
				a, s, idx, hash, msg->new_uri.len, msg->new_uri.s);
	}
	
	return 1;
}

/**
 *
 */
int ds_select_dst(struct sip_msg *msg, char *set, char *alg)
{
	return ds_select_dst_impl(msg, set, alg, 0);
}

/**
 *
 */
int ds_select_new(struct sip_msg *msg, char *set, char *alg)
{
	return ds_select_dst_impl(msg, set, alg, 1);
}

