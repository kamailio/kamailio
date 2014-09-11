/*
 * $Id$
 *
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */


#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "../../str.h"
#include "../../resolve.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"

#include "routing.h"
#include "prefix_tree.h"
#include "dr_time.h"
#include "parse.h"

#define IDX_SIZE 32

extern int dr_force_dns;

rt_data_t*
build_rt_data( void )
{
	rt_data_t *rdata;

	if( NULL==(rdata=shm_malloc(sizeof(rt_data_t)))) {
		LM_ERR("no more shm mem\n");
		goto err_exit;
	}
	memset(rdata, 0, sizeof(rt_data_t));

	INIT_PTREE_NODE(NULL, rdata->pt);

	return rdata;
err_exit:
	return 0;
}


rt_info_t*
build_rt_info(
	int priority,
	tmrec_t *trec,
	/* script routing table index */
	int route_idx,
	/* list of destinations indexes */
	char* dstlst,
	pgw_t* pgw_l
	) 
{
	char *tmp=NULL;
	char *ep=NULL;
	rt_info_t* rt = NULL;
	int *idx = NULL, *t_idx=NULL;
	int n=0, idx_size=0,i, grp_idx=0;
	long t=0;
	pgw_t *pgw=NULL;

	if(NULL == (rt = (rt_info_t*)shm_malloc(sizeof(rt_info_t)))) {
		LM_ERR("no more shm mem(1)\n");
		goto err_exit;
	}
	memset(rt, 0, sizeof(rt_info_t));

	idx_size = IDX_SIZE;
	if( NULL == (idx = (int*)shm_malloc(2*idx_size*sizeof(int)))) {
		LM_ERR("no more shm mem(2)\n");
		goto err_exit;
	}
	memset(idx, 0, 2*idx_size*sizeof(int));

	rt->priority = priority;
	rt->time_rec = trec;
	rt->route_idx = route_idx;
	tmp=dstlst;
	n=0;
	/* parse the dstlst */
	while(tmp && (*tmp!=0)) {
		errno = 0;
		t = strtol(tmp, &ep, 10);
		if (ep == tmp) {
			LM_ERR("bad id '%c' (%d)[%s]\n",
					*ep, (int)(ep-dstlst), dstlst);
			goto err_exit;
		}
		if ((!IS_SPACE(*ep)) && (*ep != SEP) && (*ep != SEP1)
				&& (*ep != SEP_GRP) && (*ep!=0)) {
			LM_ERR("bad char %c (%d) [%s]\n",
					*ep, (int)(ep-dstlst), dstlst);
			goto err_exit;
		}
		if (errno == ERANGE && (t== LONG_MAX || t== LONG_MIN)) {
			LM_ERR("out of bounds\n");
			goto err_exit;
		}
		idx[2*n]=t;
		idx[2*n+1]=grp_idx;
		if(*ep == SEP_GRP)
			grp_idx++;
		n++;
		/* reallocate the array which keeps the parsed indexes */
		if(n>=idx_size){
			if(NULL==((t_idx)=(int*)shm_malloc((idx_size*2*2)*sizeof(int)))) {
				LM_ERR("out of shm\n");
				goto err_exit;
			}
			memset(t_idx+(2*idx_size), 0, 2*idx_size*sizeof(int));
			memcpy(t_idx, idx, 2*idx_size*sizeof(int));
			shm_free(idx);
			idx_size*=2;
			idx=t_idx;
		}
		if(IS_SPACE(*ep))
			EAT_SPACE(ep);
		if(ep && (*ep == SEP || *ep == SEP1 || *ep == SEP_GRP))
			ep++;
		tmp = ep;
	}
	if(n==0) {
		LM_ERR("invalid n\n");
		goto err_exit;
	}
	/* create the pgwl */
	rt->pgwa_len = n;
	if(NULL ==
		(rt->pgwl=(pgw_list_t*)shm_malloc(rt->pgwa_len*sizeof(pgw_list_t)))) {
		goto err_exit;
	}
	memset(rt->pgwl, 0, rt->pgwa_len*sizeof(pgw_list_t));
	/* translate GW ids to GW pointers */
	for(i=0;i<n; i++){
		if ( NULL == (pgw = get_pgw(pgw_l, idx[2*i]))) {
			LM_ERR("invalid GW id %d\n",
				idx[2*i]);
			goto err_exit;
		}
		rt->pgwl[i].pgw=pgw;
		rt->pgwl[i].grpid=idx[2*i+1];
		/* LM_DBG("added to gwlist [%d/%d/%p]\n",
				idx[2*i], idx[2*i+1], pgw); */
	}

	shm_free(idx);
	return rt;

err_exit:
	if(NULL!=idx)
		shm_free(idx);
	if((NULL != rt) && 
		(NULL!=rt->pgwl))
		shm_free(rt->pgwl); 
	if(NULL!=rt)
		shm_free(rt);
	return NULL;
}

int
add_rt_info(
	ptree_node_t *pn,
	rt_info_t* r,
	unsigned int rgid
	)
{
	rg_entry_t    *trg=NULL;
	rt_info_wrp_t *rtl_wrp=NULL;
	rt_info_wrp_t *rtlw=NULL;
	int i=0;

	if((NULL == pn) || (NULL == r))
		goto err_exit;

	if (NULL == (rtl_wrp = (rt_info_wrp_t*)shm_malloc(sizeof(rt_info_wrp_t)))) {
		LM_ERR("no more shm mem\n");
		goto err_exit;
	}
	memset( rtl_wrp, 0, sizeof(rt_info_wrp_t));
	rtl_wrp->rtl = r;

	if(NULL==pn->rg) {
		/* allocate the routing groups array */
		pn->rg_len = RG_INIT_LEN;
		if(NULL == (pn->rg = (rg_entry_t*)shm_malloc(
						pn->rg_len*sizeof(rg_entry_t)))) {
			/* recover the old pointer to be able to shm_free mem */
			goto err_exit;
		}
		memset( pn->rg, 0, pn->rg_len*sizeof(rg_entry_t));
		pn->rg_pos=0;
	}
	/* search for the rgid up to the rg_pos */
	for(i=0; (i<pn->rg_pos) && (pn->rg[i].rgid!=rgid); i++);
	if((i==pn->rg_len-1)&&(pn->rg[i].rgid!=rgid)) {
		/* realloc & copy the old rg */
		trg = pn->rg;
		if(NULL == (pn->rg = (rg_entry_t*)shm_malloc(
						2*pn->rg_len*sizeof(rg_entry_t)))) {
			/* recover the old pointer to be able to shm_free mem */
			pn->rg = trg;
			goto err_exit;
		}
		memset(pn->rg+pn->rg_len, 0, pn->rg_len*sizeof(rg_entry_t));
		memcpy(pn->rg, trg, pn->rg_len*sizeof(rg_entry_t));
		pn->rg_len*=2;
		shm_free( trg );
	}
	/* insert into list */
	r->ref_cnt++;
	if(NULL==pn->rg[i].rtlw){
		pn->rg[i].rtlw = rtl_wrp;
		pn->rg[i].rgid = rgid;
		pn->rg_pos++;
		goto ok_exit;
	}
	if( r->priority > pn->rg[i].rtlw->rtl->priority) {
		/* change the head of the list */
		rtl_wrp->next = pn->rg[i].rtlw;
		pn->rg[i].rtlw = rtl_wrp;
		goto ok_exit;
	}
	rtlw = pn->rg[i].rtlw;
	while( rtlw->next !=NULL) {
		if(r->priority > rtlw->next->rtl->priority) {
			rtl_wrp->next = rtlw->next;
			rtlw->next = rtl_wrp;
			goto ok_exit;
		}
		rtlw = rtlw->next;
	}
	/* the smallest priority is linked at the end */
	rtl_wrp->next=NULL;
	rtlw->next=rtl_wrp;
ok_exit:
	return 0;

err_exit:
	if (rtl_wrp) shm_free(rtl_wrp);
	return -1;
}

int
add_dst(
	rt_data_t *r,
	/* id */
	int id,
	/* ip address */ 
	char* ip,
	/* strip len */
	int strip,
	/* pri prefix */
	char* pri,
	/* dst type*/
	int type,
	/* dst attrs*/
	char* attrs
	)
{
	pgw_t *pgw=NULL, *tmp=NULL;
	pgw_addr_t *tmpa=NULL;
	struct hostent* he;
	struct sip_uri uri;
	struct ip_addr ipa;
	int l_ip,l_pri,l_attrs;
#define GWABUF_MAX_SIZE	512
	char gwabuf[GWABUF_MAX_SIZE];
	str gwas;

	if (NULL==r || NULL==ip) {
		LM_ERR("invalid parametres\n");
		goto err_exit;
	}

	l_ip = strlen(ip);
	l_pri = pri?strlen(pri):0;
	l_attrs = attrs?strlen(attrs):0;

	pgw = (pgw_t*)shm_malloc(sizeof(pgw_t) + l_ip + l_pri + l_attrs);
	if (NULL==pgw) {
		LM_ERR("no more shm mem (%u)\n",
			(unsigned int)(sizeof(pgw_t)+l_ip+l_pri +l_attrs));
		goto err_exit;
	}
	memset(pgw,0,sizeof(pgw_t));

	pgw->ip.len= l_ip;
	pgw->ip.s = (char*)(pgw+1);
	memcpy(pgw->ip.s, ip, l_ip);

	if (pri) {
		pgw->pri.len = l_pri;
		pgw->pri.s = ((char*)(pgw+1))+l_ip;
		memcpy(pgw->pri.s, pri, l_pri);
	}
	if (attrs) {
		pgw->attrs.len = l_attrs;
		pgw->attrs.s = ((char*)(pgw+1))+l_ip+l_pri;
		memcpy(pgw->attrs.s, attrs, l_attrs);
	}
	pgw->id = id;
	pgw->strip = strip;
	pgw->type = type;

	/* add address in the list */
	if(pgw->ip.len<5 || (strncasecmp("sip:", ip, 4)
			&&strncasecmp("sips:", ip, 5)))
	{
		if(pgw->ip.len+4>=GWABUF_MAX_SIZE) {
			LM_ERR("GW address (%d) longer "
				"than %d\n",pgw->ip.len+4,GWABUF_MAX_SIZE);
			goto err_exit;
		}
		memcpy(gwabuf, "sip:", 4);
		memcpy(gwabuf+4, ip, pgw->ip.len);
		gwas.s = gwabuf;
		gwas.len = 4+pgw->ip.len;
	} else {
		gwas.s = ip;
		gwas.len = pgw->ip.len;
	}

	memset(&uri, 0, sizeof(struct sip_uri));
	if(parse_uri(gwas.s, gwas.len, &uri)!=0) {
		LM_ERR("invalid uri <%.*s>\n",
			gwas.len, gwas.s);
		goto err_exit;
	}
	/* note we discard the port discovered by the resolve function - we are
	interested only in the port that was actually configured. */
	if ((he=sip_resolvehost( &uri.host, NULL, (char*)(void*)&uri.proto))==0 ) {
		if(dr_force_dns)
		{
			LM_ERR("cannot resolve <%.*s>\n",
				uri.host.len, uri.host.s);
			goto err_exit;
		} else {
			LM_DBG("cannot resolve <%.*s> - won't be used"
					" by is_from_gw()\n", uri.host.len, uri.host.s);
			goto done;
		}
	}
	hostent2ip_addr(&ipa, he, 0);
	tmpa = r->pgw_addr_l;
	while(tmpa) {
		if(tmpa->type==type && uri.port_no==tmpa->port
		&& ip_addr_cmp(&ipa, &tmpa->ip)) {
			LM_DBG("gw ip addr [%s]:%d loaded\n",
				ip_addr2a(&ipa), uri.port_no);
			goto done;
		}
		tmpa = tmpa->next;
	}
	
	LM_DBG("new gw ip addr [%s]\n", ip);
	tmpa = (pgw_addr_t*)shm_malloc(sizeof(pgw_addr_t));
	if(tmpa==NULL) {
		LM_ERR("no more shm mem (%u)\n",
			(unsigned int)sizeof(pgw_addr_t));
		goto err_exit;
	}
	memset(tmpa, 0, sizeof(pgw_addr_t));
	memcpy(&tmpa->ip, &ipa, sizeof(struct ip_addr));
	tmpa->port = uri.port_no;
	tmpa->type = type;
	tmpa->strip = strip;
	tmpa->next = r->pgw_addr_l;
	r->pgw_addr_l = tmpa;

done:
	if(NULL==r->pgw_l)
		r->pgw_l = pgw;
	else {
		tmp = r->pgw_l;
		while(NULL != tmp->next)
			tmp = tmp->next;
		tmp->next = pgw;
	}
	return 0;

err_exit:
	if(NULL!=pgw)
		shm_free(pgw);
	return -1;
}

void
del_pgw_list(
		pgw_t *pgw_l
		)
{
	pgw_t *t;
	while(NULL!=pgw_l){
		t = pgw_l;
		pgw_l=pgw_l->next;
		shm_free(t);
	}
}

void
del_pgw_addr_list(
		pgw_addr_t *pgw_addr_l
		)
{
	pgw_addr_t *t;
	while(NULL!=pgw_addr_l){
		t = pgw_addr_l;
		pgw_addr_l=pgw_addr_l->next;
		shm_free(t);
	}
}

void 
free_rt_data(
		rt_data_t* rt_data,
		int all
		)
{
	int j;
	if(NULL!=rt_data) {
		/* del GW list */
		del_pgw_list(rt_data->pgw_l);
		rt_data->pgw_l = 0 ;
		/* del GW addr list */
		del_pgw_addr_list(rt_data->pgw_addr_l);
		rt_data->pgw_addr_l =0;
		/* del prefix tree */
		del_tree(rt_data->pt);
		/* del prefixless rules */
		if(NULL!=rt_data->noprefix.rg) {
			for(j=0;j<rt_data->noprefix.rg_pos;j++) {
				if(rt_data->noprefix.rg[j].rtlw !=NULL) {
					del_rt_list(rt_data->noprefix.rg[j].rtlw);
					rt_data->noprefix.rg[j].rtlw = 0;
				}
			}
			shm_free(rt_data->noprefix.rg);
			rt_data->noprefix.rg = 0;
		}
		/* del top level or reset to 0 it's content */
		if (all) shm_free(rt_data);
		else memset(rt_data, 0, sizeof(rt_data_t));
	}
}
