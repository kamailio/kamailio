/**
 * $Id$
 *
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


#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "../../dprint.h"
#include "../../rand/fastrand.h"
#include "../../hashes.h"
#include "../../resolve.h"
#include "../../pvar.h"


#define PV_DNS_ADDR 64
#define PV_DNS_RECS 32

typedef struct _sr_dns_record {
	int type;
	char addr[PV_DNS_ADDR];
} sr_dns_record_t;

typedef struct _sr_dns_item {
	str name;
	unsigned int hashid;
	char hostname[256];
	int count;
	int ipv4;
	int ipv6;
	sr_dns_record_t r[PV_DNS_RECS];
	struct _sr_dns_item *next;
} sr_dns_item_t;

#define SR_DNS_PVIDX	1

typedef struct _dns_pv {
	sr_dns_item_t *item;
	int type;
	int flags;
	pv_spec_t *pidx;
	int nidx;
} dns_pv_t;

static sr_dns_item_t *_sr_dns_list = NULL;

/**
 *
 */
sr_dns_item_t *sr_dns_get_item(str *name)
{
	sr_dns_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid =  get_hash1_raw(name->s, name->len);

	it = _sr_dns_list;
	while(it!=NULL)
	{
		if(it->hashid==hashid && it->name.len == name->len
				&& strncmp(it->name.s, name->s, name->len)==0)
			return it;
		it = it->next;
	}
	return NULL;
}

/**
 *
 */
sr_dns_item_t *sr_dns_add_item(str *name)
{
	sr_dns_item_t *it = NULL;
	unsigned int hashid = 0;

	hashid =  get_hash1_raw(name->s, name->len);

	it = _sr_dns_list;
	while(it!=NULL)
	{
		if(it->hashid==hashid && it->name.len == name->len
				&& strncmp(it->name.s, name->s, name->len)==0)
			return it;
		it = it->next;
	}
	/* add new */
	it = (sr_dns_item_t*)pkg_malloc(sizeof(sr_dns_item_t));
	if(it==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(it, 0, sizeof(sr_dns_item_t));
	it->name.s = (char*)pkg_malloc(name->len+1);
	if(it->name.s==NULL)
	{
		LM_ERR("no more pkg.\n");
		pkg_free(it);
		return NULL;
	}
	memcpy(it->name.s, name->s, name->len);
	it->name.s[name->len] = '\0';
	it->name.len = name->len;
	it->hashid = hashid;
	it->next = _sr_dns_list;
	_sr_dns_list = it;
	return it;
}

/**
 *
 */
int pv_parse_dns_name(pv_spec_t *sp, str *in)
{
	dns_pv_t *dpv=NULL;
	char *p;
	str pvc;
	str pvs;
	str pvi;
	int sign;

	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	dpv = (dns_pv_t*)pkg_malloc(sizeof(dns_pv_t));
	if(dpv==NULL)
		return -1;

	memset(dpv, 0, sizeof(dns_pv_t));

	p = in->s;

	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		goto error;
	pvc.s = p;
	while(p < in->s + in->len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}
	if(p>in->s+in->len || *p=='\0')
		goto error;
	pvc.len = p - pvc.s;
	if(*p!='=')
	{
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!='=')
			goto error;
	}
	p++;
	if(*p!='>')
		goto error;
	p++;

	pvs.len = in->len - (int)(p - in->s);
	pvs.s = p;
	pvi.s = 0;
	pvi.len = 0;
	if(pvs.s[pvs.len-1]==']') {
		/* index */
		p = memchr(pvs.s, '[', pvs.len-1);
		if(p==NULL) {
			goto error;
		}
		pvi.s = p + 1;
		pvi.len = pvs.s + pvs.len - pvi.s;
		pvs.len = p - pvs.s;
	}
	LM_DBG("dns [%.*s] - key [%.*s] index [%.*s]\n", pvc.len, pvc.s,
			pvs.len, pvs.s, (pvi.len>0)?pvi.len:0, (pvi.s!=NULL)?pvi.s:0);

	dpv->item = sr_dns_add_item(&pvc);
	if(dpv->item==NULL)
		goto error;

	switch(pvs.len)
	{
		case 4: 
			if(strncmp(pvs.s, "addr", 4)==0)
				dpv->type = 0;
			else if(strncmp(pvs.s, "type", 4)==0)
				dpv->type = 1;
			else if(strncmp(pvs.s, "ipv4", 4)==0)
				dpv->type = 2;
			else if(strncmp(pvs.s, "ipv6", 4)==0)
				dpv->type = 3;
			else goto error;
			break;
		case 5: 
			if(strncmp(pvs.s, "count", 5)==0)
				dpv->type = 4;
			else goto error;
			break;
		default:
			goto error;
	}

	if(pvi.len>0)
	{
		if(pvi.s[0]==PV_MARKER)
		{
			dpv->pidx = pv_cache_get(&pvi);
			if(dpv->pidx==NULL)
				goto error;
			dpv->flags |= SR_DNS_PVIDX;
		} else {
			sign = 1;
			p = pvi.s;
			if(*p=='-')
			{
				sign = -1;
				p++;
			}
			dpv->nidx = 0;
			while(p<pvi.s+pvi.len && *p>='0' && *p<='9')
			{
				dpv->nidx = dpv->nidx * 10 + *p - '0';
				p++;
			}
			if(p!=pvi.s+pvi.len)
			{
				LM_ERR("invalid index [%.*s]\n", in->len, in->s);
				return -1;
			}
			dpv->nidx *= sign;
		}
	}
	sp->pvp.pvn.u.dname = (void*)dpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;

	return 0;

error:
	LM_ERR("error at PV dns name: %.*s\n", in->len, in->s);
	if(dpv) pkg_free(dpv);
	return -1;
}

/**
 *
 */
int pv_get_dns(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res)
{
	dns_pv_t *dpv;
	pv_value_t val;

	if(msg==NULL || param==NULL)
		return -1;

	dpv = (dns_pv_t*)param->pvn.u.dname;
	if(dpv==NULL || dpv->item==NULL)
		return -1;

	if(dpv->pidx!=NULL)
	{
		if(pv_get_spec_value(msg, dpv->pidx, &val)<0
				|| (!(val.flags&PV_VAL_INT)))
		{
			LM_ERR("failed to evaluate index variable\n");
			return pv_get_null(msg, param, res);
		}
	} else {
		val.ri = dpv->nidx;
	}
	if(val.ri<0)
	{
		if(dpv->item->count+val.ri<0) {
			return pv_get_null(msg, param, res);
		}
		val.ri = dpv->item->count+val.ri;
	}
	if(val.ri>=dpv->item->count) {
		return pv_get_null(msg, param, res);
	}
	switch(dpv->type)
	{
		case 0: /* address */
			return pv_get_strzval(msg, param, res,
					dpv->item->r[val.ri].addr);
		case 1: /* type */
			return pv_get_sintval(msg, param, res,
					dpv->item->r[val.ri].type);
		case 2: /* ipv4 */
			return pv_get_sintval(msg, param, res,
					dpv->item->ipv4);
		case 3: /* ipv6 */
			return pv_get_sintval(msg, param, res,
					dpv->item->ipv6);
		case 4: /* count */
			return pv_get_sintval(msg, param, res,
					dpv->item->count);
		default: /* else */
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
int dns_init_pv(char *path)
{
	return 0;
}

/**
 *
 */
void dns_destroy_list(void)
{
	return;
}

/**
 *
 */
void dns_destroy_pv(void)
{
	return;
}

/**
 *
 */
int dns_update_pv(str *hostname, str *name)
{
	sr_dns_item_t *dr = NULL;
	struct addrinfo hints, *res, *p;
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;
	void *addr;
	int status;
	int i;

	if(hostname->len>255)
	{
		LM_DBG("target hostname too long (max 255): %s\n", hostname->s);
		return -2;
	}

	dr = sr_dns_get_item(name);
	if(dr==NULL)
	{
		LM_DBG("container not found: %s\n", name->s);
		return -3;
	}

	/* reset the counter */
	dr->count = 0;
	dr->ipv4  = 0;
	dr->ipv6  = 0;

	strncpy(dr->hostname, hostname->s, hostname->len);
	dr->hostname[hostname->len] = '\0';
	LM_DBG("attempting to resolve: %s\n", dr->hostname);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /* allow any of AF_INET or AF_INET6 */
	// hints.ai_socktype = SOCK_STREAM;
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(dr->hostname, NULL, &hints, &res)) != 0)
	{
		LM_ERR("unable to resolve %s - getaddrinfo: %s\n",
				dr->hostname, gai_strerror(status));
		return -4;
	}

	i=0;
	for(p=res; p!=NULL; p=p->ai_next)
	{
		if (p->ai_family==AF_INET)
		{
			dr->ipv4 = 1;
			dr->r[i].type = 4;
			ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
		} else {
			dr->ipv6 = 1;
			dr->r[i].type = 6;
			ipv6 = (struct sockaddr_in6 *)p->ai_addr;
			addr = &(ipv6->sin6_addr);
		}
		inet_ntop(p->ai_family, addr, dr->r[i].addr,
				PV_DNS_ADDR);
		LM_DBG("#%d - type %d addr: %s (%d)\n", i, dr->r[i].type,
				dr->r[i].addr, p->ai_socktype);
		i++;
		if(i==PV_DNS_RECS) {
			LM_WARN("more than %d addresses for %s - truncating\n",
					PV_DNS_RECS, dr->hostname);
			break;
		}
	}
	freeaddrinfo(res);

	dr->count = i;

	LM_DBG("dns PV updated for: %s (%d)\n", dr->hostname, i);

	return 1;
}

struct _hn_pv_data {
	str data;
	str fullname;
	str hostname;
	str domain;
	str ipaddr;
};

static struct _hn_pv_data *_hn_data = NULL;

/**
 *
 */
int hn_pv_data_init(void)
{
	char hbuf[512];
	int hlen;
	char *d;
	struct hostent *he;
	int i;

	if(_hn_data != NULL)
		return 0;

	if (gethostname(hbuf, 512)<0) {
		LM_WARN("gethostname failed - host pvs will be null\n");
		return -1;
	}

	hlen = strlen(hbuf);
	if(hlen<=0) {
		LM_WARN("empty hostname result - host pvs will be null\n");
		return -1;
	}

	_hn_data = (struct _hn_pv_data*)pkg_malloc(sizeof(struct _hn_pv_data)+46+2*(hlen+1));
	if(_hn_data==NULL) {
		LM_ERR("no more pkg to init hostname data\n");
		return -1;
	}
	memset(_hn_data, 0, sizeof(struct _hn_pv_data)+46+2*(hlen+1));

	_hn_data->data.len = hlen;
	_hn_data->data.s = (char*)_hn_data + sizeof(struct _hn_pv_data);
	_hn_data->fullname.len = hlen;
	_hn_data->fullname.s = _hn_data->data.s + hlen + 1;

	strcpy(_hn_data->data.s, hbuf);
	strcpy(_hn_data->fullname.s, hbuf);

	d=strchr(_hn_data->data.s, '.');
	if (d) {
		_hn_data->hostname.len   = d - _hn_data->data.s;
		_hn_data->hostname.s     = _hn_data->data.s;
		_hn_data->domain.len     = _hn_data->fullname.len
			- _hn_data->hostname.len-1;
		_hn_data->domain.s       = d+1;
	} else {
		_hn_data->hostname       = _hn_data->fullname;
	}

	he=resolvehost(_hn_data->fullname.s);
	if (he) {
		if ((strlen(he->h_name)!=_hn_data->fullname.len)
				|| strncmp(he->h_name, _hn_data->fullname.s,
					_hn_data->fullname.len)) {
			LM_WARN("hostname '%.*s' different than gethostbyname '%s'\n",
					_hn_data->fullname.len, _hn_data->fullname.s, he->h_name);
		}

		if (he->h_addr_list) {
			for (i=0; he->h_addr_list[i]; i++) {
				if (inet_ntop(he->h_addrtype, he->h_addr_list[i], hbuf, 46)) {
					if (_hn_data->ipaddr.len==0) {
						_hn_data->ipaddr.len = strlen(hbuf);
						_hn_data->ipaddr.s = _hn_data->fullname.s + hlen + 1;
						strcpy(_hn_data->ipaddr.s, hbuf);
					} else if (strncmp(_hn_data->ipaddr.s, hbuf,
								_hn_data->ipaddr.len)!=0) {
						LM_WARN("many IPs to hostname: %s not used\n", hbuf);
					}
				}
			}
		} else {
			LM_WARN(" can't resolve hostname's address: %s\n",
					_hn_data->fullname.s);
		}
	}

	DBG("Hostname: %.*s\n", _hn_data->hostname.len, ZSW(_hn_data->hostname.s));
	DBG("Domain:   %.*s\n", _hn_data->domain.len, ZSW(_hn_data->domain.s));
	DBG("Fullname: %.*s\n", _hn_data->fullname.len, ZSW(_hn_data->fullname.s));
	DBG("IPaddr:   %.*s\n", _hn_data->ipaddr.len, ZSW(_hn_data->ipaddr.s));

	return 0;
}

/**
 *
 */
int pv_parse_hn_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 1: 
			if(strncmp(in->s, "n", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "f", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "d", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "i", 1)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	hn_pv_data_init();

	return 0;

error:
	LM_ERR("unknown host PV name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_hn(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(param==NULL)
		return -1;
	if(_hn_data==NULL)
		return pv_get_null(msg, param, res);;
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			if(_hn_data->fullname.len==0)
				return pv_get_null(msg, param, res);;
			return pv_get_strval(msg, param, res, &_hn_data->fullname);
		case 2:
			if(_hn_data->domain.len==0)
				return pv_get_null(msg, param, res);;
			return pv_get_strval(msg, param, res, &_hn_data->domain);
		case 3:
			if(_hn_data->ipaddr.len==0)
				return pv_get_null(msg, param, res);;
			return pv_get_strval(msg, param, res, &_hn_data->ipaddr);
		default:
			if(_hn_data->hostname.len==0)
				return pv_get_null(msg, param, res);;
			return pv_get_strval(msg, param, res, &_hn_data->hostname);
	}
}

/**********
 * srvquery PV
 **********/

static char *srvqrylst []
= {"count", "port", "priority", "target", "weight", NULL};

#define PV_SRV_MAXSTR 64
#define PV_SRV_MAXRECS 32

typedef struct _sr_srv_record {
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	char target [PV_SRV_MAXSTR + 1];
} sr_srv_record_t;

typedef struct _sr_srv_item {
	str pvid;
	unsigned int hashid;
	int count;
	sr_srv_record_t rr [PV_SRV_MAXRECS];
	struct _sr_srv_item *next;
} sr_srv_item_t;

typedef struct _srv_pv {
	sr_srv_item_t *item;
	int type;
	int flags;
	pv_spec_t *pidx;
	int nidx;
} srv_pv_t;

static sr_srv_item_t *_sr_srv_list = NULL;

/**********
 * Add srvquery Item
 *
 * INPUT:
 *   Arg (1) = pvid string pointer
 *   Arg (2) = find flag; <>0=search only
 * OUTPUT: srv record pointer; NULL=not found
 **********/

sr_srv_item_t *sr_srv_add_item (str *pvid, int findflg)

{
	sr_srv_item_t *pitem;
	unsigned int hashid;

	/**********
	 * o get hash
	 * o already exists?
	 **********/

	hashid = get_hash1_raw (pvid->s, pvid->len);
	for (pitem = _sr_srv_list; pitem; pitem = pitem->next) {
		if (pitem->hashid == hashid
				&& pitem->pvid.len == pvid->len
				&& !strncmp (pitem->pvid.s, pvid->s, pvid->len))
			return pitem;
	}
	if (findflg)
		return NULL;

	/**********
	 * o alloc/init item structure
	 * o link in new item
	 **********/

	pitem = (sr_srv_item_t *) pkg_malloc (sizeof (sr_srv_item_t));
	if (!pitem) {
		LM_ERR ("No more pkg memory!\n");
		return NULL;
	}
	memset (pitem, 0, sizeof (sr_srv_item_t));
	pitem->pvid.s = (char *) pkg_malloc (pvid->len + 1);
	if (!pitem->pvid.s) {
		LM_ERR ("No more pkg memory!\n");
		pkg_free (pitem);
		return NULL;
	}
	memcpy (pitem->pvid.s, pvid->s, pvid->len);
	pitem->pvid.len = pvid->len;
	pitem->hashid = hashid;
	pitem->next = _sr_srv_list;
	_sr_srv_list = pitem;
	return pitem;
}

/**********
 * Skip Over
 *
 * INPUT:
 *   Arg (1) = string pointer
 *   Arg (2) = starting position
 *   Arg (3) = whitespace flag
 * OUTPUT: position past skipped
 **********/

int skip_over (str *pstr, int pos, int bWS)

{
	char *pchar;

	/**********
	 * o string exists?
	 * o skip over
	 **********/

	if (pos >= pstr->len)
		return pstr->len;
	for (pchar = &pstr->s [pos]; pos < pstr->len; pchar++, pos++) {
		if (*pchar == ' ' || *pchar == '\t' || *pchar == '\n' || *pchar == '\r') {
			if (bWS)
				continue;
		}
		if ((*pchar>='A' && *pchar<='Z') || (*pchar>='a' && *pchar<='z')
				|| (*pchar>='0' && *pchar<='9')) {
			if (!bWS)
				continue;
		}
		break;
	}
	return pos;
}

/**********
 * Sort SRV Records by Weight (RFC 2782)
 *
 * INPUT:
 *   Arg (1) = pointer to array of SRV records
 *   Arg (2) = first record in range
 *   Arg (3) = last record in range
 * OUTPUT: position past skipped
 **********/

void sort_weights (struct srv_rdata **plist, int pos1, int pos2)

{
	int idx1, idx2, lastfound;
	struct srv_rdata *wlist [PV_SRV_MAXRECS];
	unsigned int rand, sum, sums [PV_SRV_MAXRECS];

	/**********
	 * place zero weights in the unordered list and then non-zero
	 **********/

	idx2 = 0;
	for (idx1 = pos1; idx1 <= pos2; idx1++) {
		if (!plist [idx1]->weight) {
			wlist [idx2++] = plist [idx1];
		}
	}
	for (idx1 = pos1; idx1 <= pos2; idx1++) {
		if (plist [idx1]->weight) {
			wlist [idx2++] = plist [idx1];
		}
	}

	/**********
	 * generate running sum list
	 **********/

	sum = 0;
	for (idx1 = 0; idx1 < idx2; idx1++) {
		sum += wlist [idx1]->weight;
		sums [idx1] = sum;
	}

	/**********
	 * resort randomly
	 **********/

	lastfound = 0;
	for (idx1 = pos1; idx1 <= pos2; idx1++) {
		/**********
		 * o calculate a random number in range
		 * o find first unsorted
		 **********/

		rand = fastrand_max (sum);
		for (idx2 = 0; idx2 <= pos2 - pos1; idx2++) {
			if (!wlist [idx2]) {
				continue;
			}
			if (sums [idx2] >= rand) {
				plist [idx1] = wlist [idx2];
				wlist [idx2] = 0;
				break;
			}
			lastfound = idx2;
		}
		if (idx2 > pos2 - pos1) {
			plist [idx1] = wlist [lastfound];
			wlist [lastfound] = 0;
		}
	}
	return;
}

/**********
 * Sort SRV Records by Priority/Weight
 *
 * INPUT:
 *   Arg (1) = pointer to array of SRV records
 *   Arg (2) = record count
 * OUTPUT: position past skipped
 **********/

void sort_srv (struct srv_rdata **plist, int rcount)

{
	int idx1, idx2;
	struct srv_rdata *pswap;

	/**********
	 * sort by priority
	 **********/

	for (idx1 = 1; idx1 < rcount; idx1++) {
		pswap = plist [idx1];
		for (idx2 = idx1;
				idx2 && (plist [idx2 - 1]->priority > pswap->priority); --idx2) {
			plist [idx2] = plist [idx2 - 1];
		}
		plist [idx2] = pswap;
	}

	/**********
	 * check for multiple priority
	 **********/

	idx2 = 0;
	pswap = plist [0];
	for (idx1 = 1; idx1 <= rcount; idx1++) {
		if ((idx1 == rcount) || (pswap->priority != plist [idx1]->priority)) {
			/**********
			 * o range has more than one element?
			 * o restart range
			 **********/

			if (idx1 - idx2 - 1) {
				sort_weights (plist, idx2, idx1 - 1);
			}
			idx2 = idx1;
			pswap = plist [idx2];
		}
	}
	return;
}

/**********
 * Parse srvquery Name
 *
 * INPUT:
 *   Arg (1) = pv spec pointer
 *   Arg (2) = input string pointer
 * OUTPUT: 0=success
 **********/

int pv_parse_srv_name (pv_spec_t *sp, str *in)

{
	char *pstr;
	int i, pos, sign;
	srv_pv_t *dpv;
	str pvi = {0}, pvk = {0}, pvn = {0};

	/**********
	 * o alloc/init pvid structure
	 * o extract pvid name
	 * o check separator
	 **********/

	if (!sp || !in || in->len<=0)
		return -1;
	dpv = (srv_pv_t *) pkg_malloc (sizeof (srv_pv_t));
	if (!dpv) {
		LM_ERR ("No more pkg memory!\n");
		return -1;
	}
	memset (dpv, 0, sizeof (srv_pv_t));
	pos = skip_over (in, 0, 1);
	if (pos == in->len)
		goto error;
	pvn.s = &in->s [pos];
	pvn.len = pos;
	pos = skip_over (in, pos, 0);
	pvn.len = pos - pvn.len;
	if (!pvn.len)
		goto error;
	pos = skip_over (in, pos, 1);
	if ((pos + 2) > in->len)
		goto error;
	if (strncmp (&in->s [pos], "=>", 2))
		goto error;

	/**********
	 * o extract key name
	 * o check key name
	 * o count?
	 **********/

	pos = skip_over (in, pos + 2, 1);
	pvk.s = &in->s [pos];
	pvk.len = pos;
	pos = skip_over (in, pos, 0);
	pvk.len = pos - pvk.len;
	if (!pvk.len)
		goto error;
	for (i = 0; srvqrylst [i]; i++) {
		if (strlen (srvqrylst [i]) != pvk.len)
			continue;
		if (!strncmp (pvk.s, srvqrylst [i], pvk.len)) {
			dpv->type = i;
			break;
		}
	}
	if (!srvqrylst [i])
		goto error;
	if (!i)
		goto noindex;

	/**********
	 * o check for array
	 * o extract array index and check
	 **********/

	pos = skip_over (in, pos, 1);
	if ((pos + 3) > in->len)
		goto error;
	if (in->s [pos] != '[')
		goto error;
	pos = skip_over (in, pos + 1, 1);
	if ((pos + 2) > in->len)
		goto error;
	pvi.s = &in->s [pos];
	pvi.len = pos;
	if (in->s [pos] == PV_MARKER) {
		/**********
		 * o search from the end back to array close
		 * o get PV value
		 **********/

		for (i = in->len - 1; i != pos; --i) {
			if (in->s [i] == ']')
				break;
		}
		if (i == pos)
			goto error;
		pvi.len = i - pvi.len;
		pos = i + 1;
		dpv->pidx = pv_cache_get (&pvi);
		if (!dpv->pidx)
			goto error;
		dpv->flags |= SR_DNS_PVIDX;
	} else {
		/**********
		 * o get index value
		 * o check for reverse index
		 * o convert string to number
		 **********/

		pos = skip_over (in, pos, 0);
		pvi.len = pos - pvi.len;
		sign = 1;
		i = 0;
		pstr = pvi.s;
		if (*pstr == '-') {
			sign = -1;
			i++;
			pstr++;
		}
		for (dpv->nidx = 0; i < pvi.len; i++) {
			if (*pstr >= '0' && *pstr <= '9')
				dpv->nidx = (dpv->nidx * 10) + *pstr++ - '0';
		}
		if (i != pvi.len)
			goto error;
		dpv->nidx *= sign;
		pos = skip_over (in, pos, 1);
		if (pos == in->len)
			goto error;
		if (in->s [pos++] != ']')
			goto error;
	}

	/**********
	 * o check for trailing whitespace
	 * o add data to PV
	 **********/

noindex:
	if (skip_over (in, pos, 1) != in->len)
		goto error;
	LM_DBG ("srvquery (%.*s => %.*s [%.*s])\n",
			pvn.len, ZSW(pvn.s), pvk.len, ZSW(pvk.s), pvi.len, ZSW(pvi.s));
	dpv->item = sr_srv_add_item (&pvn, 0);
	if (!dpv->item)
		goto error;
	sp->pvp.pvn.u.dname = (void *)dpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;

error:
	LM_ERR ("error at PV srvquery: %.*s@%d\n", in->len, in->s, pos);
	pkg_free (dpv);
	return -1;
}

int srv_update_pv (str *srvcname, str *pvid)

{
	int idx1, idx2, rcount;
	struct rdata *phead, *psrv;
	struct srv_rdata *plist [PV_SRV_MAXRECS];
	sr_srv_item_t *pitem;
	sr_srv_record_t *prec;

	/**********
	 * o service name missing?
	 * o find pvid
	 **********/

	if (!srvcname->len) {
		LM_DBG ("service name missing: %.*s\n", srvcname->len, srvcname->s);
		return -2;
	}
	pitem = sr_srv_add_item (pvid, 1);
	if (!pitem) {
		LM_DBG ("pvid not found: %.*s\n", pvid->len, pvid->s);
		return -3;
	}

	/**********
	 * o get records
	 * o sort by priority/weight
	 * o save to PV
	 **********/

	LM_DBG ("attempting to query: %.*s\n", srvcname->len, srvcname->s);
	phead = get_record (srvcname->s, T_SRV, RES_ONLY_TYPE);
	rcount = 0;
	for (psrv = phead; psrv; psrv = psrv->next) {
		if (rcount < PV_SRV_MAXRECS) {
			plist [rcount++] = (struct srv_rdata *) psrv->rdata;
		} else {
			LM_WARN ("truncating srv_query list to %d records!", PV_SRV_MAXRECS);
			break;
		}
	}
	pitem->count = rcount;
	if (rcount)
		sort_srv (plist, rcount);
	for (idx1 = 0; idx1 < rcount; idx1++) {
		prec = &pitem->rr [idx1];
		prec->priority = plist [idx1]->priority;
		prec->weight = plist [idx1]->weight;
		prec->port = plist [idx1]->port;
		idx2 = plist [idx1]->name_len;
		if (idx2 > PV_SRV_MAXSTR) {
			LM_WARN ("truncating srv_query target (%.*s)!", idx2, plist [idx1]->name);
			idx2 = PV_SRV_MAXSTR;
		}
		strncpy (prec->target, plist [idx1]->name, idx2);
		prec->target [idx2] = '\0';
	}
	if (phead)
		free_rdata_list (phead);
	LM_DBG ("srvquery PV updated for: %.*s (%d)\n",
			srvcname->len, srvcname->s, rcount);
	return 1;
}

/**********
 * Get srvquery Values
 *
 * INPUT:
 *   Arg (1) = SIP message pointer
 *   Arg (2) = parameter pointer
 *   Arg (3) = PV value pointer
 * OUTPUT: 0=success
 **********/

int pv_get_srv (sip_msg_t *pmsg, pv_param_t *param, pv_value_t *res)

{
	pv_value_t val;
	srv_pv_t *dpv;

	/**********
	 * o sipmsg and param exist?
	 * o PV name exists?
	 * o count?
	 **********/

	if(!pmsg || !param)
		return -1;
	dpv = (srv_pv_t *) param->pvn.u.dname;
	if(!dpv || !dpv->item)
		return -1;
	if (!dpv->type)
		return pv_get_sintval (pmsg, param, res, dpv->item->count);

	/**********
	 * o get index value
	 * o reverse index?
	 * o extract data
	 **********/

	if (!dpv->pidx) {
		val.ri = dpv->nidx;
	} else {
		if (pv_get_spec_value (pmsg, dpv->pidx, &val) < 0
				|| !(val.flags & PV_VAL_INT)) {
			LM_ERR ("failed to evaluate index variable!\n");
			return pv_get_null (pmsg, param, res);
		}
	}
	if (val.ri < 0) {
		if ((dpv->item->count + val.ri) < 0)
			return pv_get_null (pmsg, param, res);
		val.ri = dpv->item->count + val.ri;
	}
	if (val.ri >= dpv->item->count)
		return pv_get_null(pmsg, param, res);
	switch (dpv->type) {
		case 1: /* port */
			return pv_get_sintval (pmsg, param, res, dpv->item->rr [val.ri].port);
		case 2: /* priority */
			return pv_get_sintval (pmsg, param, res, dpv->item->rr [val.ri].priority);
		case 3: /* target */
			return pv_get_strzval (pmsg, param, res, dpv->item->rr [val.ri].target);
		case 4: /* weight */
			return pv_get_sintval (pmsg, param, res, dpv->item->rr [val.ri].weight);
	}
	return pv_get_null (pmsg, param, res);
}
