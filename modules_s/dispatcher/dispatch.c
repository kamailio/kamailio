/**
 * $Id$
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 Fhg Fokus
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
 * MERCHA:b1
 * NTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
	
	/* reindex destination sets for fast access */
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
	pkg_free(_ds_list);
	
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
		for (p=x->s; p<=(x->s+x->len-4); p+=4)
		{
			v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
			h+=v^(v>>3);
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
		for (p=y->s; p<=(y->s+y->len-4); p+=4)
		{
			v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
			h+=v^(v>>3);
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

	return (h)?(h<0)?-h:h:1;
}

/**
 *
 */
int ds_hash_fromuri(struct sip_msg *msg, unsigned int *hash)
{
	str from;
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

	from.s   = get_from(msg)->uri.s;
	from.len = get_from(msg)->uri.len; 
	trim(&from);

	*hash = ds_get_hash(&from, NULL);
	
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
	
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID, 0)==-1) ||
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

/**
 *
 */
int ds_select_dst(struct sip_msg *msg, char *set, char *alg)
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
	
	s = (int)set;
	a = (int)alg;

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
		default:
			hash = 0;
	}

	DBG("DISPATCHER:ds_select_dst: alg hash [%u]\n", hash);

	hash = hash%_ds_list[idx].nr;
	
	msg->dst_uri.s   = _ds_list[idx].dlist[hash].uri.s;
	msg->dst_uri.len = _ds_list[idx].dlist[hash].uri.len;

	DBG("DISPATCHER:ds_select_dst: selected [%d-%d/%d/%d] <%.*s>\n",
			a, s, idx, hash, msg->dst_uri.len, msg->dst_uri.s);
	
	return 1;
}

