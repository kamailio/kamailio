/**
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../ut.h"

#include "../../lib/srutils/sruid.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../sr_module.h"

#include "msrp_netio.h"
#include "msrp_env.h"
#include "msrp_cmap.h"

static msrp_cmap_t *_msrp_cmap_head = NULL;

static sruid_t _msrp_sruid;

extern int msrp_auth_min_expires;
extern int msrp_auth_max_expires;
extern int msrp_tls_module_loaded;
extern str msrp_use_path_addr;

/**
 *
 */
int msrp_sruid_init(void)
{
	return sruid_init(&_msrp_sruid, '-', "msrp", SRUID_INC);
}

/**
 *
 */
int msrp_citem_free(msrp_citem_t *it)
{
	if(it==NULL)
		return -1;
	shm_free(it);
	return 0;
}

/**
 *
 */
int msrp_cmap_init(int msize)
{
	int i;

	_msrp_cmap_head = (msrp_cmap_t*)shm_malloc(sizeof(msrp_cmap_t));
	if(_msrp_cmap_head==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(_msrp_cmap_head, 0, sizeof(msrp_cmap_t));
	_msrp_cmap_head->mapsize = msize;

	_msrp_cmap_head->cslots = (msrp_centry_t*)shm_malloc(
							_msrp_cmap_head->mapsize*sizeof(msrp_centry_t) );
	if(_msrp_cmap_head->cslots==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(_msrp_cmap_head);
		_msrp_cmap_head = NULL;
		return -1;
	}
	memset(_msrp_cmap_head->cslots, 0,
						_msrp_cmap_head->mapsize*sizeof(msrp_centry_t));

	for(i=0; i<_msrp_cmap_head->mapsize; i++)
	{
		if(lock_init(&_msrp_cmap_head->cslots[i].lock)==0)
		{
			LM_ERR("cannot initalize lock[%d]\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_msrp_cmap_head->cslots[i].lock);
				i--;
			}
			shm_free(_msrp_cmap_head->cslots);
			shm_free(_msrp_cmap_head);
			_msrp_cmap_head = NULL;
			return -1;
		}
	}

	return 0;
}

/**
 *
 */
int msrp_cmap_destroy(void)
{
	int i;
	msrp_citem_t *ita, *itb;

	if(_msrp_cmap_head==NULL)
		return -1;

	for(i=0; i<_msrp_cmap_head->mapsize; i++)
	{
		/* free entries */
		ita = _msrp_cmap_head->cslots[i].first;
		while(ita)
		{
			itb = ita;
			ita = ita->next;
			msrp_citem_free(itb);
		}
		/* free locks */
		lock_destroy(&_msrp_cmap_head->cslots[i].lock);
	}
	shm_free(_msrp_cmap_head->cslots);
	shm_free(_msrp_cmap_head);
	_msrp_cmap_head = NULL;
	return 0;
}

#define msrp_get_hashid(_s)        core_case_hash(_s,0,0)
#define msrp_get_slot(_h, _size)    (_h)&((_size)-1)


static str msrp_reply_200_code = {"200", 3};
static str msrp_reply_200_text = {"OK", 2};
static str msrp_reply_423_code = {"423", 3};
static str msrp_reply_423_text = {"Interval Out Of Bounds", 22};

/**
 *
 */
int msrp_cmap_save(msrp_frame_t *mf)
{
	unsigned int idx;
	unsigned int hid;
	str fpeer;
#define MSRP_SBUF_SIZE	256
	char sbuf[MSRP_SBUF_SIZE];
	str srcaddr;
	str srcsock;
	int msize;
	int expires;
	msrp_citem_t *it;
	msrp_citem_t *itb;

	if(_msrp_cmap_head==NULL || mf==NULL)
		return -1;
	if(mf->fline.rtypeid!=MSRP_REQ_AUTH)
	{
		LM_DBG("save can be used only for AUTH\n");
		return -2;
	}

	if(msrp_frame_get_expires(mf, &expires)<0)
		expires = msrp_auth_max_expires;
	if(expires<msrp_auth_min_expires)
	{
		LM_DBG("expires is lower than min value\n");
		srcaddr.len = snprintf(sbuf, MSRP_SBUF_SIZE, "Min-Expires: %d\r\n",
				msrp_auth_min_expires);
		msrp_reply(mf, &msrp_reply_423_code, &msrp_reply_423_text,
				&srcaddr);
		return -3;
	}
	if(expires>msrp_auth_max_expires)
	{
		LM_DBG("expires is greater than max value\n");
		srcaddr.len = snprintf(sbuf, MSRP_SBUF_SIZE, "Max-Expires: %d\r\n",
				msrp_auth_max_expires);
		msrp_reply(mf, &msrp_reply_423_code, &msrp_reply_423_text,
				&srcaddr);
		return -4;
	}
	if(msrp_frame_get_first_from_path(mf, &fpeer)<0)
	{
		LM_ERR("cannot get first path uri\n");
		return -1;
	}

	if(sruid_next(&_msrp_sruid)<0)
	{
		LM_ERR("cannot get next msrp uid\n");
		return -1;
	}
	hid = msrp_get_hashid(&_msrp_sruid.uid);	
	idx = msrp_get_slot(hid, _msrp_cmap_head->mapsize);

	srcaddr.s = sbuf;
	if (msrp_tls_module_loaded)
	{
		memcpy(srcaddr.s, "msrps://", 8);
		srcaddr.s+=8;
	} else {
		memcpy(srcaddr.s, "msrp://", 7);
		srcaddr.s+=7;
	}
	strcpy(srcaddr.s, ip_addr2a(&mf->tcpinfo->rcv->src_ip));
	strcat(srcaddr.s, ":");
	strcat(srcaddr.s, int2str(mf->tcpinfo->rcv->src_port, NULL));
	srcaddr.s = sbuf;
	srcaddr.len = strlen(srcaddr.s);
	srcsock = mf->tcpinfo->rcv->bind_address->sock_str;
	LM_DBG("saving connection info for [%.*s] [%.*s] (%u/%u)\n",
			fpeer.len, fpeer.s, _msrp_sruid.uid.len, _msrp_sruid.uid.s,
			idx, hid);
	LM_DBG("frame received from [%.*s] via [%.*s]\n",
			srcaddr.len, srcaddr.s, srcsock.len, srcsock.s);

	msize = sizeof(msrp_citem_t) + (_msrp_sruid.uid.len
			+ fpeer.len + srcaddr.len + srcsock.len + 4)*sizeof(char);

	/* build the item */
	it = (msrp_citem_t*)shm_malloc(msize);
	if(it==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(it, 0, msize);
	it->citemid = hid;

	it->sessionid.s = (char*)it +  + sizeof(msrp_citem_t);
	it->sessionid.len = _msrp_sruid.uid.len;
	memcpy(it->sessionid.s, _msrp_sruid.uid.s, _msrp_sruid.uid.len);
	it->sessionid.s[it->sessionid.len] = '\0';

	it->peer.s = it->sessionid.s + it->sessionid.len + 1;
	it->peer.len = fpeer.len;
	memcpy(it->peer.s, fpeer.s, fpeer.len);
	it->peer.s[it->peer.len] = '\0';

	it->addr.s = it->peer.s + it->peer.len + 1;
	it->addr.len = srcaddr.len;
	memcpy(it->addr.s, srcaddr.s, srcaddr.len);
	it->addr.s[it->addr.len] = '\0';

	it->sock.s = it->addr.s + it->addr.len + 1;
	it->sock.len = srcsock.len;
	memcpy(it->sock.s, srcsock.s, srcsock.len);
	it->sock.s[it->sock.len] = '\0';

	it->expires = time(NULL) + expires;
	it->conid = mf->tcpinfo->con->id;

	/* insert item in cmap */
	lock_get(&_msrp_cmap_head->cslots[idx].lock);
	if(_msrp_cmap_head->cslots[idx].first==NULL) {
		_msrp_cmap_head->cslots[idx].first = it;
	} else {
		for(itb=_msrp_cmap_head->cslots[idx].first; itb; itb=itb->next)
		{
			if(itb->citemid>it->citemid || itb->next==NULL) {
				if(itb->next==NULL) {
					itb->next=it;
					it->prev = itb;
				} else {
					it->next = itb;
					if(itb->prev==NULL) {
						_msrp_cmap_head->cslots[idx].first = it;
					} else {
						itb->prev->next = it;
					}
					it->prev = itb->prev;
					itb->prev = it;
				}
				break;
			}
		}
	}
	_msrp_cmap_head->cslots[idx].lsize++;
	lock_release(&_msrp_cmap_head->cslots[idx].lock);

	if(mf->tcpinfo->rcv->proto==PROTO_TLS || mf->tcpinfo->rcv->proto==PROTO_WSS)
	{
		srcaddr.len = snprintf(sbuf, MSRP_SBUF_SIZE,
				"Use-Path: msrps://%.*s/%.*s;tcp\r\nExpires: %d\r\n",
				(msrp_use_path_addr.s)?msrp_use_path_addr.len:(srcsock.len-4),
				(msrp_use_path_addr.s)?msrp_use_path_addr.s:(srcsock.s+4),
				_msrp_sruid.uid.len, _msrp_sruid.uid.s,
				expires);
	} else {
		srcaddr.len = snprintf(sbuf, MSRP_SBUF_SIZE,
				"Use-Path: msrp://%.*s/%.*s;tcp\r\nExpires: %d\r\n",
				(msrp_use_path_addr.s)?msrp_use_path_addr.len:(srcsock.len-4),
				(msrp_use_path_addr.s)?msrp_use_path_addr.s:(srcsock.s+4),
				_msrp_sruid.uid.len, _msrp_sruid.uid.s,
				expires);
	}
	srcaddr.s = sbuf;

	if(msrp_reply(mf, &msrp_reply_200_code, &msrp_reply_200_text,
				&srcaddr)<0)
		return -5;
	return 0;
}

/**
 *
 */
int msrp_cmap_lookup(msrp_frame_t *mf)
{
	unsigned int idx;
	unsigned int hid;
	str sesid;
	msrp_citem_t *itb;
	int ret;

	if(_msrp_cmap_head==NULL || mf==NULL)
		return -1;
	if(mf->fline.rtypeid==MSRP_REQ_AUTH)
	{
		LM_DBG("save cannot be used for AUTH\n");
		return -2;
	}
	if(msrp_frame_get_sessionid(mf, &sesid)<0)
	{
		LM_ERR("cannot get session id\n");
		return -3;
	}

	LM_DBG("searching for session [%.*s]\n", sesid.len, sesid.s);

	hid = msrp_get_hashid(&sesid);	
	idx = msrp_get_slot(hid, _msrp_cmap_head->mapsize);

	ret = 0;
	lock_get(&_msrp_cmap_head->cslots[idx].lock);
	for(itb=_msrp_cmap_head->cslots[idx].first; itb; itb=itb->next)
	{
		if(itb->citemid>hid) {
			break;
		} else {
			if(itb->sessionid.len == sesid.len
					&& memcmp(itb->sessionid.s, sesid.s, sesid.len)==0) {
				LM_DBG("found session [%.*s]\n", sesid.len, sesid.s);
				ret = msrp_env_set_dstinfo(mf, &itb->addr, &itb->sock, 0);
				break;
			}
		}
	}
	lock_release(&_msrp_cmap_head->cslots[idx].lock);
	if(itb==NULL)
		return -4;
	return (ret<0)?-5:0;
}

/**
 *
 */
int msrp_cmap_clean(void)
{
	time_t tnow;
	msrp_citem_t *ita;
	msrp_citem_t *itb;
	int i;

	if(_msrp_cmap_head==NULL)
		return -1;
	tnow = time(NULL);
	for(i=0; i<_msrp_cmap_head->mapsize; i++)
	{
		lock_get(&_msrp_cmap_head->cslots[i].lock);
		ita = _msrp_cmap_head->cslots[i].first;
		while(ita)
		{
			itb = ita;
			ita = ita->next;
			if(itb->expires<tnow) {
				if(itb->prev==NULL) {
					_msrp_cmap_head->cslots[i].first = itb->next;
				} else {
					itb->prev->next = ita;
				}
				if(ita!=NULL)
					ita->prev = itb->prev;
				msrp_citem_free(itb);
				_msrp_cmap_head->cslots[i].lsize--;
			}
		}
		lock_release(&_msrp_cmap_head->cslots[i].lock);
	}

	return 0;
}

static const char* msrp_cmap_rpc_list_doc[2] = {
	"Return the content of dispatcher sets",
	0
};


/*
 * RPC command to print connections map table
 */
static void msrp_cmap_rpc_list(rpc_t* rpc, void* ctx)
{
	void* th;
	void* ih;
	void* vh;
	msrp_citem_t *it;
	int i;
	int n;
	str edate;

	if(_msrp_cmap_head==NULL)
	{
		LM_ERR("no connections map table\n");
		rpc->fault(ctx, 500, "No Connections Map Table");
		return;
	}

	/* add entry node */
	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}

	if(rpc->struct_add(th, "d{",
				"MAP_SIZE", _msrp_cmap_head->mapsize,
				"CONLIST",  &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error set structure");
		return;
	}
	n = 0;
	for(i=0; i<_msrp_cmap_head->mapsize; i++)
	{
		lock_get(&_msrp_cmap_head->cslots[i].lock);
		for(it=_msrp_cmap_head->cslots[i].first; it; it=it->next)
		{
			if(rpc->struct_add(ih, "{",
						"CONDATA", &vh)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating connection");
				lock_release(&_msrp_cmap_head->cslots[i].lock);
				return;
			}
			edate.s = ctime(&it->expires);
			edate.len = 24;
			if(rpc->struct_add(vh, "dSSSSSdd",
						"CITEMID", it->citemid,
						"SESSIONID", &it->sessionid,
						"PEER", &it->peer,
						"ADDR", &it->addr,
						"SOCK", &it->sock,
						"EXPIRES", &edate,
						"CONID", it->conid,
						"FLAGS", it->cflags)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				lock_release(&_msrp_cmap_head->cslots[i].lock);
				return;
			}
			n++;
		}
		lock_release(&_msrp_cmap_head->cslots[i].lock);
	}
	if(rpc->struct_add(th, "d", "CONCOUNT", n)<0)
	{
		rpc->fault(ctx, 500, "Internal error connection counter");
		return;
	}
	return;
}

rpc_export_t msrp_cmap_rpc_cmds[] = {
	{"msrp.cmaplist",   msrp_cmap_rpc_list,
		msrp_cmap_rpc_list_doc,   0},
	{0, 0, 0, 0}
};

/**
 *
 */
int msrp_cmap_init_rpc(void)
{
	if (rpc_register_array(msrp_cmap_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	return 0;
}
