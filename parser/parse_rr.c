/*
 * Route & Record-Route header field parser
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \brief Parser :: Route & Record-Route header field parser
 *
 * \ingroup parser
 */

#include <string.h>
#include "parse_rr.h"
#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "../dprint.h"
#include "../trim.h"
#include "../ut.h"

/*! \brief
 * Parse Route or Record-Route body
 */
static inline int do_parse_rr_body(char *buf, int len, rr_t **head)
{
	rr_t* r, *last;
	str s;
	param_hooks_t hooks;

	/* Make a temporary copy of the string pointer */
	if(buf==0 || len<=0)
	{
		DBG("parse_rr_body(): No body for record-route\n");
		*head = 0;
		return -2;
	}
	s.s = buf;
	s.len = len;
	trim_leading(&s);

	last = 0;

	while(1) {
		     /* Allocate and clear rr structure */
		r = (rr_t*)pkg_malloc(sizeof(rr_t));
		if (!r) {
			LOG(L_ERR, "parse_rr(): No memory left\n");
			goto error;
		}
		memset(r, 0, sizeof(rr_t));
		
		     /* Parse name-addr part of the header */
		if (parse_nameaddr(&s, &r->nameaddr) < 0) {
			LOG(L_ERR, "parse_rr(): Error while parsing name-addr (%.*s)\n",
					s.len, ZSW(s.s));
			goto error;
		}
		r->len = r->nameaddr.len;

		     /* Shift just behind the closing > */
		s.s = r->nameaddr.name.s + r->nameaddr.len;  /* Point just behind > */
		s.len -= r->nameaddr.len;

		trim_leading(&s); /* Skip any white-chars */

		if (s.len == 0) goto ok; /* Nothing left, finish */
		
		if (s.s[0] == ';') {         /* Route parameter found */
			s.s++;
			s.len--;
			trim_leading(&s);
			
			if (s.len == 0) {
				LOG(L_ERR, "parse_rr(): Error while parsing params\n");
				goto error;
			}

			     /* Parse all parameters */
			if (parse_params(&s, CLASS_ANY, &hooks, &r->params) < 0) {
				LOG(L_ERR, "parse_rr(): Error while parsing params\n");
				goto error;
			}
			r->len = r->params->name.s + r->params->len - r->nameaddr.name.s;

			     /* Copy hooks */
			     /*r->r2 = hooks.rr.r2; */

			trim_leading(&s);
			if (s.len == 0) goto ok;
		}

		if (s.s[0] != ',') {
			LOG(L_ERR, "parse_rr(): Invalid character '%c', comma expected\n", s.s[0]);
			goto error;
		}
		
		     /* Next character is comma or end of header*/
		s.s++;
		s.len--;
		trim_leading(&s);

		if (s.len == 0) {
			LOG(L_ERR, "parse_rr(): Text after comma missing\n");
			goto error;
		}

		     /* Append the structure as last parameter of the linked list */
		if (!*head) *head = r;
		if (last) last->next = r;
		last = r;
	}

 error:
	if (r) free_rr(&r);
	free_rr(head); /* Free any contacts created so far */
	return -1;

 ok:
	if (!*head) *head = r;
	if (last) last->next = r;
	return 0;
}

/*! \brief
 * Wrapper to do_parse_rr_body() for external calls
 */
int parse_rr_body(char *buf, int len, rr_t **head)
{
	return do_parse_rr_body(buf, len, head);
}

/*! \brief
 * Parse Route and Record-Route header fields
 */
int parse_rr(struct hdr_field* _h)
{
	rr_t* r = NULL;

	if (!_h) {
		LOG(L_ERR, "parse_rr(): Invalid parameter value\n");
		return -1;
	}

	if (_h->parsed) {
		     /* Already parsed, return */
		return 0;
	}

	if(do_parse_rr_body(_h->body.s, _h->body.len, &r) < 0)
		return -1;
	_h->parsed = (void*)r;
	return 0;
}

/*! \brief
 * Free list of rrs
 * _r is head of the list
 */
static inline void do_free_rr(rr_t** _r, int _shm)
{
	rr_t* ptr;

	while(*_r) {
		ptr = *_r;
		*_r = (*_r)->next;
		if (ptr->params) {
			if (_shm) shm_free_params(ptr->params);
			else free_params(ptr->params);
		}
		if (_shm) shm_free(ptr);
		else pkg_free(ptr);
	}
}


/*! \brief
 * Free list of rrs
 * _r is head of the list
 */

void free_rr(rr_t** _r)
{
	do_free_rr(_r, 0);
}


/*! \brief
 * Free list of rrs
 * _r is head of the list
 */

void shm_free_rr(rr_t** _r)
{
	do_free_rr(_r, 1);
}


/*! \brief
 * Print list of RRs, just for debugging
 */
void print_rr(FILE* _o, rr_t* _r)
{
	rr_t* ptr;

	ptr = _r;

	while(ptr) {
		fprintf(_o, "---RR---\n");
		print_nameaddr(_o, &ptr->nameaddr);
		fprintf(_o, "r2 : %p\n", ptr->r2);
		if (ptr->params) {
			print_params(_o, ptr->params);
		}
		fprintf(_o, "len: %d\n", ptr->len);
		fprintf(_o, "---/RR---\n");
		ptr = ptr->next;
	}
}


/*! \brief
 * Translate all pointers in the structure and also
 * in all parameters in the list
 */
static inline void xlate_pointers(rr_t* _orig, rr_t* _r)
{
	param_t* ptr;
	_r->nameaddr.uri.s = translate_pointer(_r->nameaddr.name.s, _orig->nameaddr.name.s, _r->nameaddr.uri.s);
	
	ptr = _r->params;
	while(ptr) {
		     /*		if (ptr->type == P_R2) _r->r2 = ptr; */
		ptr->name.s = translate_pointer(_r->nameaddr.name.s, _orig->nameaddr.name.s, ptr->name.s);
		ptr->body.s = translate_pointer(_r->nameaddr.name.s, _orig->nameaddr.name.s, ptr->body.s);		
		ptr = ptr->next;
	}
}


/*! \brief
 * Duplicate a single rr_t structure using pkg_malloc or shm_malloc
 */
static inline int do_duplicate_rr(rr_t** _new, rr_t* _r, int _shm)
{
	int len, ret;
	rr_t* res, *prev, *it;

	if (!_new || !_r) {
		LOG(L_ERR, "duplicate_rr(): Invalid parameter value\n");
		return -1;
	}
	prev  = NULL;
	*_new = NULL;
	it    = _r;
	while(it)
	{
		if (it->params) {
			len = it->params->name.s + it->params->len - it->nameaddr.name.s;
		} else {
			len = it->nameaddr.len;
		}

		if (_shm) res = shm_malloc(sizeof(rr_t) + len);
		else res = pkg_malloc(sizeof(rr_t) + len);
		if (!res) {
			LOG(L_ERR, "duplicate_rr(): No memory left\n");
			return -2;
		}
		memcpy(res, it, sizeof(rr_t));

		res->nameaddr.name.s = (char*)res + sizeof(rr_t);
		memcpy(res->nameaddr.name.s, it->nameaddr.name.s, len);

		if (_shm) {
			ret = shm_duplicate_params(&res->params, it->params);
		} else {
			ret = duplicate_params(&res->params, it->params);
		}

		if (ret < 0) {
			LOG(L_ERR, "duplicate_rr(): Error while duplicating parameters\n");
			if (_shm) shm_free(res);
			else pkg_free(res);
			return -3;
		}

		xlate_pointers(it, res);

		res->next=NULL;
		if(*_new==NULL)
			*_new = res;
		if(prev)
			prev->next = res;
		prev = res;
		it = it->next;
	}
	return 0;
}


/*! \brief
 * Duplicate a single rr_t structure using pkg_malloc
 */
int duplicate_rr(rr_t** _new, rr_t* _r)
{
	return do_duplicate_rr(_new, _r, 0);
}


/*! \brief
 * Duplicate a single rr_t structure using pkg_malloc
 */
int shm_duplicate_rr(rr_t** _new, rr_t* _r)
{
	return do_duplicate_rr(_new, _r, 1);
}

/*!
 * get first RR header and print comma separated bodies in oroute
 * - order = 0 normal; order = 1 reverse
 * - nb_recs - input=skip number of rr; output=number of printed rrs
 */
int print_rr_body(struct hdr_field *iroute, str *oroute, int order,
												unsigned int * nb_recs)
{
	rr_t *p;
	int n = 0, nr=0;
	int i = 0;
	int route_len;
#define MAX_RR_HDRS	64
	static str route[MAX_RR_HDRS];
	char *cp, *start;

	if(iroute==NULL)
		return 0;

	route_len= 0;
	memset(route, 0, MAX_RR_HDRS*sizeof(str));

	while (iroute!=NULL) 
	{
		if (parse_rr(iroute) < 0) 
		{
			LM_ERR("failed to parse RR\n");
			goto error;
		}

		p =(rr_t*)iroute->parsed;
		while (p)
		{
			route[n].s = p->nameaddr.name.s;
			route[n].len = p->len;
			LM_DBG("current rr is %.*s\n", route[n].len, route[n].s);

			n++;
			if(n==MAX_RR_HDRS)
			{
				LM_ERR("too many RR\n");
				goto error;
			}
			p = p->next;
		}

		iroute = next_sibling_hdr(iroute);
	}

	for(i=0;i<n;i++){
		if(!nb_recs || (nb_recs && 
		 ( (!order&& (i>=*nb_recs)) || (order && (i<=(n-*nb_recs)) )) ) )
		{
			route_len+= route[i].len;
			nr++;
		}
	
	}

	if(nb_recs)
		LM_DBG("skipping %i route records\n", *nb_recs);
	
	route_len += --nr; /* for commas */

	oroute->s=(char*)pkg_malloc(route_len);


	if(oroute->s==0)
	{
		LM_ERR("no more pkg mem\n");
		goto error;
	}
	cp = start = oroute->s;
	if(order==0)
	{
		i= (nb_recs == NULL) ? 0:*nb_recs;

		while (i<n)
		{
			memcpy(cp, route[i].s, route[i].len);
			cp += route[i].len;
			if (++i<n)
				*(cp++) = ',';
		}
	} else {
		
		i = (nb_recs == NULL) ? n-1 : (n-*nb_recs-1);
			
		while (i>=0)
		{
			memcpy(cp, route[i].s, route[i].len);
			cp += route[i].len;
			if (i-->0)
				*(cp++) = ',';
		}
	}
	oroute->len=cp - start;

	LM_DBG("out rr [%.*s]\n", oroute->len, oroute->s);
	LM_DBG("we have %i records\n", (nb_recs == NULL) ? n : n-*nb_recs);
	if(nb_recs != NULL)
		*nb_recs = (unsigned int)n-*nb_recs;

	return 0;

error:
	return -1;
}


/*!
 * Path must be available. Function returns the first uri 
 * from Path without any duplication.
 */
int get_path_dst_uri(str *_p, str *_dst)
{
	rr_t *route = 0;

	LM_DBG("path for branch: '%.*s'\n", _p->len, _p->s);
	if(parse_rr_body(_p->s, _p->len, &route) < 0) {	
		LM_ERR("failed to parse Path body\n");
		return -1;
	}

	if(!route) {
		LM_ERR("failed to parse Path body no head found\n");
		return -1;
	}
	*_dst = route->nameaddr.uri;

	free_rr(&route);
	
	return 0;
}
