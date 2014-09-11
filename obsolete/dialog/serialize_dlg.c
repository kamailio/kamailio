/* 
 * Copyright (C) 2005 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "dlg_mod_internal.h"
#include "serialize_dlg.h"

#include <cds/logger.h>
#include <cds/memory.h>
#include <cds/rr_serialize.h>

static int serialize_dlg_state(sstream_t *ss, dlg_state_t *state)
{
	int i = -1;
	
	if (is_input_sstream(ss)) { /* read state */
		if (serialize_int(ss, &i) != 0) return -1;
		switch (i) {
			case 0: *state = DLG_NEW; break;
			case 1: *state = DLG_EARLY; break;
			case 2: *state = DLG_CONFIRMED; break;
			case 3: *state = DLG_DESTROYED; break;
			default: 
				ERROR_LOG("deserializing unknow dialog state (%d)!\n", i);
				return -1; /* unknown dialog state */
		}	
	}
	else { /* store state */
		switch (*state) {
			case DLG_NEW: i = 0; break;
			case DLG_EARLY: i = 1; break;
			case DLG_CONFIRMED: i = 2; break;
			case DLG_DESTROYED: i = 3; break;
		}
		if (i == -1) {
			WARN_LOG("serializing unknow dialog state (probably unloadable!)\n");
		}
		serialize_int(ss, &i);
	}
	return 0;
}

#if 0

static int serialize_ptype(sstream_t *ss, ptype_t *type)
{
	int i = -1;
	
	if (is_input_sstream(ss)) { /* read ptype */
		if (serialize_int(ss, &i) != 0) return -1;
		switch (i) {
			case 0: *type = P_OTHER; break;
			case 1: *type = P_Q; break;
			case 2: *type = P_EXPIRES; break;
			case 3: *type = P_METHOD; break;
			case 4: *type = P_RECEIVED; break;
			case 5: *type = P_TRANSPORT; break;
			case 6: *type = P_LR; break;
			case 7: *type = P_R2; break;
			case 8: *type = P_MADDR; break;
			case 9: *type = P_TTL; break;
			case 10: *type = P_DSTIP; break;
			case 11: *type = P_DSTPORT; break;
			case 12: *type = P_INSTANCE; break;
			default: 
				ERROR_LOG("deserializing unknow ptype (%d)!\n", i);
				return -1;
		}	
	}
	else { /* store ptype */
		switch (*type) {
			case P_OTHER: i = 0; break;
			case P_Q: i = 1; break;
			case P_EXPIRES: i = 2; break;
			case P_METHOD: i = 3; break;
			case P_RECEIVED: i = 4; break;
			case P_TRANSPORT: i = 5; break;
			case P_LR: i = 6; break;
			case P_R2: i = 7; break;
			case P_MADDR: i = 8; break;
			case P_TTL: i = 9; break;
			case P_DSTIP: i = 10; break;
			case P_DSTPORT: i = 11; break;
			case P_INSTANCE: i = 12; break;
		}
		if (i == -1) {
			WARN_LOG("serializing unknow ptype (probably unloadable!)\n");
		}
		serialize_int(ss, &i);
	}
	return 0;
}
/*static int serialize_param(sstream_t *ss, rr_t **_r)
{
	int do_it = 0;
	int res = 0;
	
	if (is_input_sstream(ss)) { / * read route * /
		if (serialize_int(ss, &do_it) != 0) return -1;
		if (!do_it) *_r = NULL;
		else {
			*_r = (rr_t*)cds_malloc(sizeof(rr_t));
			if (!*_r) {
				ERROR_LOG("serialize_route(): can't allocate memory\n");
				return -1;
			}
			(*_r)->r2 = NULL;
			(*_r)->params = NULL;
			(*_r)->next = NULL;
		}
	}
	else { / * store route * /
		if (*_r) do_it = 1;
		else do_it = 0;
		if (serialize_int(ss, &do_it) != 0) return -1;
	}
		
	if (do_it) {
		rr_t *r = *_r;

		res = serialize_str(ss, &r->nameaddr.name) | res;
		res = serialize_str(ss, &r->nameaddr.uri) | res;
		res = serialize_int(ss, &r->nameaddr.len) | res;
		res = serialize_params(ss, &r->nameaddr.params) | res;
		res = serialize_int(ss, &r->nameaddr.len) | res;
	}
	
	return res;
}*/

static int serialize_param(sstream_t *ss, param_t *p)
{
	int res = 0;
		
	res = serialize_ptype(ss, &p->type) | res;
	res = serialize_str(ss, &p->name) | res;
	res = serialize_str(ss, &p->body) | res;
	res = serialize_int(ss, &p->len) | res;

	return res;
}

static int serialize_params(sstream_t *ss, param_t **params)
{
	if (is_input_sstream(ss)) { /* read */	
		int i = 0;
		param_t *p, *last = NULL;
		
		*params = NULL;
		if (serialize_int(ss, &i) != 0) return -1; /* can't read "terminator" */
		while (i) {
			p = (param_t *)cds_malloc(sizeof(param_t));
			if (!p) {
				ERROR_LOG("serialize_params(): can't allocate memory\n");
				return -1;
			}
			p->next = NULL;
			if (last) last->next = p;
			else *params = p;
			last = p;
			if (serialize_param(ss, p) != 0) return -1;
			if (serialize_int(ss, &i) != 0) return -1; /* can't read "terminator" */
		}
	}
	else {	/* store */
		param_t *p = *params;
		int i = 1;
		while (p) {
			if (serialize_int(ss, &i) != 0) return -1; /* params terminator ! */
			if (serialize_param(ss, p) != 0) return -1;
			p = p->next;
		}
		i = 0;
		if (serialize_int(ss, &i) != 0) return -1; /* params terminator ! */
	}
	return 0;
}

static int serialize_route_ex(sstream_t *ss, rr_t **_r)
{
	int do_it = 0;
	int res = 0;
	
	if (is_input_sstream(ss)) { /* read route */
		if (serialize_int(ss, &do_it) != 0) return -1;
		if (!do_it) *_r = NULL;
		else {
			*_r = (rr_t*)cds_malloc(sizeof(rr_t));
			if (!*_r) {
				ERROR_LOG("serialize_route(): can't allocate memory\n");
				return -1;
			}
			(*_r)->r2 = NULL;
			(*_r)->params = NULL;
			(*_r)->next = NULL;
		}
	}
	else { /* store route */
		if (*_r) do_it = 1;
		else do_it = 0;
		if (serialize_int(ss, &do_it) != 0) return -1;
	}
		
	if (do_it) {
		rr_t *r = *_r;
		str s = { r->nameaddr.name.s, r->len };
		int delta = r->nameaddr.uri.s - r->nameaddr.name.s;

		res = serialize_str(ss, &s) | res;
		res = serialize_int(ss, &r->nameaddr.name.len) | res;
		res = serialize_int(ss, &r->nameaddr.uri.len) | res;
		res = serialize_int(ss, &r->nameaddr.len) | res;
		res = serialize_int(ss, &delta) | res;
		if (is_input_sstream(ss)) {
			r->nameaddr.name.s = s.s;
			r->nameaddr.uri.s = s.s + delta;
		}
		/* !!! optimalized strings - use carefuly !!! */
		/*res = serialize_str(ss, &r->nameaddr.name) | res;
		res = serialize_str(ss, &r->nameaddr.uri) | res;
		res = serialize_int(ss, &r->nameaddr.len) | res;*/
		
		/* ??? res = serialize_r2(ss, &r->nameaddr.params) | res; ??? */
		res = serialize_params(ss, &r->params) | res;
		res = serialize_int(ss, &r->len) | res;

		TRACE_LOG("ROUTE: rlen=%d name=%.*s, uri=%.*s\n len=%d WHOLE=%.*s\n", 
				r->len,
				FMT_STR(r->nameaddr.name),
				FMT_STR(r->nameaddr.uri),
				r->nameaddr.len,
				r->nameaddr.len, r->nameaddr.name.s
				);
	}
	
	return res;
}
#endif

/*static void trace_dlg(const char *s, dlg_t *d)
{
	rr_t *r;
	
	TRACE_LOG("%s: callid = %.*s \nrem tag = %.*s \nloc tag = %.*s\n"
			"loc uri = %.*s\n rem uri = %.*s\n rem target = %.*s\n"
			"loc_cseq = %d rem_cseq = %d\n", 
			s,
			FMT_STR(d->id.call_id),
			FMT_STR(d->id.rem_tag),
			FMT_STR(d->id.loc_tag),
			FMT_STR(d->loc_uri),
			FMT_STR(d->rem_uri),
			FMT_STR(d->rem_target),
			d->loc_seq.value,
			d->rem_seq.value
			);
	
	r = d->route_set;
	while (r) {
		TRACE_LOG(" ... name = %.*s\n uri = %.*s", 
				FMT_STR(r->nameaddr.name),  
				FMT_STR(r->nameaddr.uri));
		r = r->next;
	}
}*/

int serialize_dlg(sstream_t *ss, dlg_t *dlg)
{
	int res = 0;
	
	if (is_input_sstream(ss)) {
		memset(dlg, 0, sizeof(*dlg));
	}
	res = serialize_str(ss, &dlg->id.call_id) | res;
	res = serialize_str(ss, &dlg->id.rem_tag) | res;
	res = serialize_str(ss, &dlg->id.loc_tag) | res;
	res = serialize_uint(ss, &dlg->loc_seq.value) | res;
	res = serialize_uchar(ss, &dlg->loc_seq.is_set) | res;
	res = serialize_uint(ss, &dlg->rem_seq.value) | res;
	res = serialize_uchar(ss, &dlg->rem_seq.is_set) | res;
	res = serialize_str(ss, &dlg->loc_uri) | res;
	res = serialize_str(ss, &dlg->rem_uri) | res;
	res = serialize_str(ss, &dlg->rem_target) | res;
	res = serialize_uchar(ss, &dlg->secure) | res;
	res = serialize_dlg_state(ss, &dlg->state) | res;
	res = serialize_route_set(ss, &dlg->route_set) | res;

	if ((res == 0) && (is_input_sstream(ss))) {
		/* tmb.w_calculate_hooks(dlg); */
		res = tmb.calculate_hooks(dlg);
		if (res < 0) {
			ERROR_LOG("error during calculate_hooks (%d)!\n", res);
		}
	}
	
	return res;
}

int dlg2str(dlg_t *dlg, str *dst_str)
{
	int res = 0;
	sstream_t store;
	
	init_output_sstream(&store, 256);
	
	if (serialize_dlg(&store, dlg) != 0) {
		ERROR_LOG("can't serialize dialog\n");
		res = -1;
	}
	else {
		if (get_serialized_sstream(&store, dst_str) != 0) {
			ERROR_LOG("can't get serialized dialog data\n");
			res = -1;
		}
	}

	destroy_sstream(&store);
	return res;
}

int str2dlg(const str *s, dlg_t *dst_dlg)
{
	int res = 0;
	sstream_t store;

	if (!s) return -1;
	
	init_input_sstream(&store, s->s, s->len);
	if (serialize_dlg(&store, dst_dlg) != 0) {
		ERROR_LOG("can't de-serialize dialog\n");
		res = -1;
	}	
	destroy_sstream(&store);
	
	return res;
}

