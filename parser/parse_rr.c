/*
 * $Id$
 *
 * Route & Record-Route header field parser
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */

/**
 * History:
 * --------
 * 2003-10-07  parse_rr() splited and added parse_rr_body()
 * 2003-10-21  duplicate_rr() duplicate the whole linked list of RR
 */
#include <string.h>
#include "parse_rr.h"
#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "../dprint.h"
#include "../trim.h"
#include "../ut.h"

/*
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
		return 0;
	}
	s.s = buf;
	s.len = len;
	trim_leading(&s);

	last = 0;

	while(1) {
		     /* Allocate and clear rr stucture */
		r = (rr_t*)pkg_malloc(sizeof(rr_t));
		if (!r) {
			LOG(L_ERR, "parse_rr(): No memory left\n");
			goto error;
		}
		memset(r, 0, sizeof(rr_t));
		
		     /* Parse name-addr part of the header */
		if (parse_nameaddr(&s, &r->nameaddr) < 0) {
			LOG(L_ERR, "parse_rr(): Error while parsing name-addr\n");
			goto error;
		}
		r->len = r->nameaddr.len;

		     /* Shift just behind the closing > */
		s.s = r->nameaddr.name.s + r->nameaddr.len;  /* Point just behind > */
		s.len -= r->nameaddr.len;

		     /* Nothing left, finish */
		if (s.len == 0) goto ok;
		
		if (s.s[0] == ';') {         /* Contact parameter found */
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

			if (s.len == 0) goto ok;
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
	if (r) pkg_free(r);
	free_rr(head); /* Free any contacts created so far */
	return -1;

 ok:
	if (!*head) *head = r;
	if (last) last->next = r;
	return 0;
}

/*
 * Wrapper to do_parse_rr_body() for external calls
 */
int parse_rr_body(char *buf, int len, rr_t **head)
{
	return do_parse_rr_body(buf, len, head);
}

/*
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

/*
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


/*
 * Free list of rrs
 * _r is head of the list
 */

void free_rr(rr_t** _r)
{
	do_free_rr(_r, 0);
}


/*
 * Free list of rrs
 * _r is head of the list
 */

void shm_free_rr(rr_t** _r)
{
	do_free_rr(_r, 1);
}


/*
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


/*
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


/*
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


/*
 * Duplicate a single rr_t structure using pkg_malloc
 */
int duplicate_rr(rr_t** _new, rr_t* _r)
{
	return do_duplicate_rr(_new, _r, 0);
}


/*
 * Duplicate a single rr_t structure using pkg_malloc
 */
int shm_duplicate_rr(rr_t** _new, rr_t* _r)
{
	return do_duplicate_rr(_new, _r, 1);
}
