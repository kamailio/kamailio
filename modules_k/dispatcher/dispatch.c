/*
 * $Id$
 *
 * dispatcher module
 *
 * Copyright (C) 2004-2006 FhG Fokus
 * Copyright (C) 2005 Voice-System.ro
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
 * History
 * -------
 * 2004-07-31  first version, by daniel
 * 2005-04-22  added ruri  & to_uri hashing (andrei)
 * 2005-12-10  added failover support via avp (daniel)
 * 
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
	int flags;
	struct _ds_dest *next;
} ds_dest_t, *ds_dest_p;

typedef struct _ds_set
{
	int id;				/* id of dst set */
	int nr;				/* number of items in dst set */
	int index;			/* index pf dst set */
	int last;			/* last used item in dst set */
	ds_dest_p dlist;
	struct _ds_set *next;
} ds_set_t, *ds_set_p;

extern int ds_force_dst;

ds_setidx_p _ds_index = NULL;
ds_set_p _ds_list = NULL;
int _ds_list_nr = 0;

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
			si = (ds_setidx_p)shm_malloc(sizeof(ds_setidx_t));
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
			sp = (ds_set_p)shm_malloc(sizeof(ds_set_t));
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
		dp = (ds_dest_p)shm_malloc(sizeof(ds_dest_t));
		if(dp==NULL)
		{
			LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory!\n");
			goto error;
		}
		memset(dp, 0, sizeof(ds_dest_t));

		dp->uri.s = (char*)shm_malloc(uri.len+1);
		if(dp->uri.s==NULL)
		{
			LOG(L_ERR, "DISPATCHER:ds_load_list: no more memory!!\n");
			shm_free(dp);
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
	sp0 = (ds_set_p)shm_malloc(setn*sizeof(ds_set_t));
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
		dp0 = (ds_dest_p)shm_malloc(sp0[i].nr*sizeof(ds_dest_t));
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
			shm_free(dp0);
		}
		sp0 = sp;
		sp = sp->next;
		shm_free(sp0);
	}

	_ds_list_nr = setn;
	
	return 0;

error:
	if(f!=NULL)
		fclose(f);

	si = _ds_index;
	while(si)
	{
		si0 = si;
		si = si->next;
		shm_free(si0);
	}
	_ds_index = NULL;

	sp = _ds_list;
	while(sp)
	{
		dp = sp->dlist;
		while(dp)
		{
			if(dp->uri.s!=NULL)
				shm_free(dp->uri.s);
			dp->uri.s=NULL;
			dp0 = dp;
			dp = dp->next;
			shm_free(dp0);
		}
		sp0 = sp;
		sp = sp->next;
		shm_free(sp0);
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
				shm_free(sp->dlist[i].uri.s);
				sp->dlist[i].uri.s = NULL;
			}
		}
		shm_free(sp->dlist);
		sp = sp->next;
	}
	if (_ds_list) shm_free(_ds_list);
	
	si = _ds_index;
	while(si)
	{
		si0 = si;
		si = si->next;
		shm_free(si0);
	}
	_ds_index = NULL;

	return 0;
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
	if (!(flags & DS_HASH_USER_ONLY)){
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
	if (key1->s==0){
		LOG(L_WARN, "DISPATCHER: get_uri_hashs_keys: empty username in:"
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
	
	if(parse_from_header(msg)<0)
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
	
	*hash = ds_get_hash(&key1, &key2);
	return 0;
}


static inline int ds_get_index(int group, int *index)
{
	ds_setidx_p si = NULL;
	
	if(index==NULL || group<0 || _ds_index==NULL)
		return -1;
	
	/* get the index of the set */
	si = _ds_index;
	while(si)
	{
		if(si->id == group)
		{
			*index = si->index;
			break;
		}
		si = si->next;
	}

	if(si==NULL)
	{
		LOG(L_ERR,
			"DISPATCHER:ds_get_index: destination set [%d] not found\n",group);
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
				LOG(L_ERR,
					"DISPATCHER:dst_update_dst: Error while setting host\n");
				return -1;
			}
			if(route_type==FAILURE_ROUTE)
			{
				if (append_branch(msg, 0, 0, 0, Q_UNSPECIFIED, 0, 0)!=1 )
				{
					LOG(L_ERR,
						"DISPATCHER:dst_update_dst: append_branch action"
						" failed\n");
					return -1;
				}
			}
		break;
		default:
			if(route_type==FAILURE_ROUTE)
			{
				if (append_branch(msg, 0, uri, 0, Q_UNSPECIFIED, 0, 0)!=1 )
				{
					LOG(L_ERR,
						"DISPATCHER:dst_update_dst: append_branch action"
						" failed\n");
					return -1;
				}
			} else {
				if (set_dst_uri(msg, uri) < 0) {
					LOG(L_ERR,
					"DISPATCHER:dst_update_dst: Error while setting dst_uri\n");
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
	int idx, i, cnt;
	unsigned int hash;
	int_str avp_name, avp_val;

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

	if((mode==0) && (ds_force_dst==0)
			&& (msg->dst_uri.s!=NULL || msg->dst_uri.len>0))
	{
		LOG(L_ERR,
			"DISPATCHER:ds_select_dst: destination already set [%.*s]\n",
			msg->dst_uri.len, msg->dst_uri.s);
		return -1;
	}
	

	/* get the index of the set */
	if(ds_get_index(set, &idx)!=0)
	{
		LOG(L_ERR,
			"DISPATCHER:ds_select_dst: destination set [%d] not found\n",set);
		return -1;
	}
	
	DBG("DISPATCHER:ds_select_dst: set index [%d->%d]\n", set, idx);

	hash = 0;
	switch(alg)
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
		case 4:
			hash = _ds_list[idx].last;
			_ds_list[idx].last = (_ds_list[idx].last+1) % _ds_list[idx].nr;
		break;
		default:
			LOG(L_WARN,
					"WARNING: ds_select_dst: algo %d not implemented"
					" - using first entry...\n", alg);
			hash = 0;
	}

	DBG("DISPATCHER:ds_select_dst: alg hash [%u]\n", hash);
	cnt = 0;
	if(ds_use_default!=0)
		hash = hash%(_ds_list[idx].nr-1);
	else
		hash = hash%_ds_list[idx].nr;
	i=hash;
	while(_ds_list[idx].dlist[i].flags & DS_INACTIVE_DST)
	{
		if(ds_use_default!=0)
			i = (i+1)%(_ds_list[idx].nr-1);
		else
			i = (i+1)%_ds_list[idx].nr;
		if(i==hash)
		{
			if(ds_use_default!=0)
			{
				i = _ds_list[idx].nr-1;
			} else {
				return -1;
			}
		}
	}

	hash = i;

	if(ds_update_dst(msg, &_ds_list[idx].dlist[hash].uri, mode)!=0)
	{
		LOG(L_ERR, "DISPATCHER:ds_select_dst: cannot set dst addr\n");
		return -1;
	}
	
	DBG("DISPATCHER:ds_select_dst: selected [%d-%d/%d/%d] <%.*s>\n",
		alg, set, idx, hash, _ds_list[idx].dlist[hash].uri.len,
		_ds_list[idx].dlist[hash].uri.s);

	if(!(ds_flags&DS_FAILOVER_ON))
		return 1;

	if(ds_use_default!=0 && hash!=_ds_list[idx].nr-1)
	{
		avp_val.s = _ds_list[idx].dlist[_ds_list[idx].nr-1].uri;
		avp_name.n = dst_avp_id;
		if(add_avp(AVP_VAL_STR, avp_name, avp_val)!=0)
			return -1;
		cnt++;
	}
	
	/* add to avp */

	for(i=hash-1; i>=0; i--)
	{	
		if((_ds_list[idx].dlist[i].flags & DS_INACTIVE_DST)
				|| (ds_use_default!=0 && i==(_ds_list[idx].nr-1)))
			continue;
		DBG("DISPATCHER:ds_select_dst: using entry [%d/%d]\n",
				set, i);
		avp_val.s = _ds_list[idx].dlist[i].uri;
		avp_name.n = dst_avp_id;
		if(add_avp(AVP_VAL_STR, avp_name, avp_val)!=0)
			return -1;
		cnt++;
	}

	for(i=_ds_list[idx].nr-1; i>hash; i--)
	{	
		if((_ds_list[idx].dlist[i].flags & DS_INACTIVE_DST)
				|| (ds_use_default!=0 && i==(_ds_list[idx].nr-1)))
			continue;
		DBG("DISPATCHER:ds_select_dst: using entry [%d/%d]\n",
				set, i);
		avp_val.s = _ds_list[idx].dlist[i].uri;
		avp_name.n = dst_avp_id;
		if(add_avp(AVP_VAL_STR, avp_name, avp_val)!=0)
			return -1;
		cnt++;
	}

	/* add to avp the first used dst */
	avp_val.s = _ds_list[idx].dlist[hash].uri;
	avp_name.n = dst_avp_id;
	if(add_avp(AVP_VAL_STR, avp_name, avp_val)!=0)
		return -1;
	cnt++;
	
	/* add to avp the group id */
	avp_val.n = set;
	avp_name.n = grp_avp_id;
	if(add_avp(0, avp_name, avp_val)!=0)
		return -1;
	
	/* add to avp the number of dst */
	avp_val.n = cnt;
	avp_name.n = cnt_avp_id;
	if(add_avp(0, avp_name, avp_val)!=0)
		return -1;
	
	return 1;
}

int ds_next_dst(struct sip_msg *msg, int mode)
{
	struct usr_avp *avp;
	struct usr_avp *prev_avp;
	int_str avp_name;
	int_str avp_value;
	
	if(!(ds_flags&DS_FAILOVER_ON))
	{
		LOG(L_WARN, "DISPATCHER:ds_next_dst: failover support disabled\n");
		return -1;
	}

	avp_name.n = dst_avp_id;

	prev_avp = search_first_avp(0, avp_name, &avp_value, 0);
	if(prev_avp==NULL)
		return -1; /* used avp deleted -- strange */

	avp = search_next_avp(prev_avp, &avp_value);
	destroy_avp(prev_avp);
	if(avp==NULL || !(avp->flags&AVP_VAL_STR))
		return -1; /* no more avps or value is int */
	
	if(ds_update_dst(msg, &avp_value.s, mode)!=0)
	{
		LOG(L_ERR, "DISPATCHER:ds_next_dst: cannot set dst addr\n");
		return -1;
	}
	DBG("DISPATCHER:ds_next_dst: using [%.*s]\n",
			avp_value.s.len, avp_value.s.s);
	
	return 1;
}

int ds_mark_dst(struct sip_msg *msg, int mode)
{
	int group, ret;
	struct usr_avp *prev_avp;
	int_str avp_name;
	int_str avp_value;
	
	if(!(ds_flags&DS_FAILOVER_ON))
	{
		LOG(L_WARN, "DISPATCHER:ds_mark_dst: failover support disabled\n");
		return -1;
	}

	avp_name.n = grp_avp_id;
	prev_avp = search_first_avp(0, avp_name, &avp_value, 0);
	
	if(prev_avp==NULL || prev_avp->flags&AVP_VAL_STR)
		return -1; /* grp avp deleted -- strange */
	group = avp_value.n;
	
	avp_name.n = dst_avp_id;
	prev_avp = search_first_avp(0, avp_name, &avp_value, 0);
	
	if(prev_avp==NULL || !(prev_avp->flags&AVP_VAL_STR))
		return -1; /* dst avp deleted -- strange */
	
	if(mode==1)
		ret = ds_set_state(group, &avp_value.s, DS_INACTIVE_DST, 0);
	else
		ret = ds_set_state(group, &avp_value.s, DS_INACTIVE_DST, 1);

	DBG("DISPATCHER:ds_mark_dst: mode [%d] grp [%d] dst [%.*s]\n",
			mode, group, avp_value.s.len, avp_value.s.s);
	
	return (ret==0)?1:-1;
}

int ds_set_state(int group, str *address, int state, int type)
{
	int i=0, idx=0;

	if(_ds_list==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_set_state: the list is null\n");
		return -1;
	}
	
	/* get the index of the set */
	if(ds_get_index(group, &idx)!=0)
	{
		LOG(L_ERR,
			"DISPATCHER:ds_set_state: destination set [%d] not found\n",group);
		return -1;
	}

	while(i<_ds_list[idx].nr)
	{
		if(_ds_list[idx].dlist[i].uri.len==address->len 
				&& strncasecmp(_ds_list[idx].dlist[i].uri.s, address->s,
					address->len)==0)
		{
			if(type)
				_ds_list[idx].dlist[i].flags |= state;
			else
				_ds_list[idx].dlist[i].flags &= ~state;
				
			return 0;
		}
		i++;
	}

	return -1;
}

int ds_print_list(FILE *fout)
{
	int i, j;
	
	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LOG(L_ERR, "DISPATCHER:ds_print_list: the list is null\n");
		return -1;
	}
	
	fprintf(fout, "\nnumber of destination sets: %d\n", _ds_list_nr);
	for(i=0; i<_ds_list_nr; i++)
	{
		fprintf(fout, "\n set #%d\n", _ds_list[i].id);
		for(j=0; j<_ds_list[i].nr; j++)
		{
			fprintf(fout, "    %c   %.*s\n",
				(_ds_list[i].dlist[j].flags&DS_INACTIVE_DST)?'I':'A',
				_ds_list[i].dlist[j].uri.len, _ds_list[i].dlist[j].uri.s);
		}
	}
	return 0;
}


int ds_print_mi_list(struct mi_node* rpl)
{
	int i, j, len;
	char* p;
	char c;
	struct mi_node* node = NULL;
	struct mi_node* set_node = NULL;
	struct mi_attr* attr = NULL;

	if(_ds_list==NULL || _ds_list_nr<=0)
	{
		LOG(L_ERR, "DISPATCHER:ds_mi_print_list: the list is null\n");
		return  0;
	}

	p= int2str(_ds_list_nr, &len); 
	node = add_mi_node_child(rpl, MI_DUP_VALUE, "SET_NO",6, p,
			len);
	if(node== NULL)
		return -1;

	for(i=0; i<_ds_list_nr; i++)
	{
		p = int2str(_ds_list[i].id, &len);
		set_node= add_mi_node_child(rpl, MI_DUP_VALUE,"SET", 3, p, len);
		if(set_node == NULL)
			return -1;

		for(j=0; j<_ds_list[i].nr; j++)
		{
			node= add_mi_node_child(set_node, 0, "URI", 3,
					_ds_list[i].dlist[j].uri.s, _ds_list[i].dlist[j].uri.len);
			if(node == NULL)
				return -1;

			c =  (_ds_list[i].dlist[j].flags&DS_INACTIVE_DST)?'I':'A';
			attr = add_mi_attr (node, MI_DUP_VALUE, "flag",4, &c, 1);
			if(attr == 0)
				return -1;

		}
	}

	return 0;
}
