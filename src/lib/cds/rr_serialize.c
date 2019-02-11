#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/rr_serialize.h>

#ifdef SER

static void rr_dup(rr_t **dst, rr_t *pkg_rr)
{
	rr_t *tmp = NULL;
	int res;
	
	tmp = pkg_rr;
	
	while (pkg_rr) {
		res = shm_duplicate_rr(dst, pkg_rr);
		dst = &(*dst)->next;
		pkg_rr = pkg_rr->next;
	}
	if (tmp) free_rr(&tmp);
}

static int serialize_route(sstream_t *ss, rr_t **_r)
{
	int do_it = 0;
	int res = 0;
	if (is_input_sstream(ss)) { /* read route */
		if (serialize_int(ss, &do_it) != 0) return -1;
		*_r = NULL;
	}
	else { /* store route */
		if (*_r) do_it = 1;
		else do_it = 0;
		if (serialize_int(ss, &do_it) != 0) return -1;
	}
		
	if (do_it) {
		str s;
		if (*_r) {
			s.s = (*_r)->nameaddr.name.s;
			s.len = (*_r)->len;
		}

		res = serialize_str_ex(ss, &s) | res;
		if (is_input_sstream(ss)) {
			rr_t *pkg_rr = NULL;
			
			parse_rr_body(s.s, s.len, &pkg_rr);
			rr_dup(_r, pkg_rr);
		}
	}
	
	return res;
}

int serialize_route_set(sstream_t *ss, rr_t **route_set)
{
	rr_t *r, *first = NULL, *last = NULL;
	int res = 0;

	if (is_input_sstream(ss)) { /* read */
		do {
			res = serialize_route(ss, &r) | res;
			if (last) last->next = r; 
			else first = r;
			last = r;
			if (last) {
				/* due to parsing rr (may be more rr_t than 1) */
				while (last->next) last = last->next; 
			}
		} while (r);
		*route_set = first;
	}
	else {	/* store */
		r = *route_set;
		while (r) {
			serialize_route(ss, &r);
			r = r->next;
		}
		r = NULL;
		serialize_route(ss, &r); /* store terminating route */
	}
	
	return 0;
}

int route_set2str(rr_t *rr, str_t *dst_str)
{
	int res = 0;
	sstream_t store;
	
	init_output_sstream(&store, 256);
	
	if (serialize_route_set(&store, &rr) != 0) {
		ERROR_LOG("can't serialize route set\n");
		res = -1;
	}
	else {
		if (get_serialized_sstream(&store, dst_str) != 0) {
			ERROR_LOG("can't get serialized data\n");
			res = -1;
		}
	}

	destroy_sstream(&store);
	return res;
}

int str2route_set(const str_t *s, rr_t **rr)
{
	int res = 0;
	sstream_t store;

	if (!s) return -1;
	
	init_input_sstream(&store, s->s, s->len);
	if (serialize_route_set(&store, rr) != 0) {
		ERROR_LOG("can't de-serialize route set\n");
		res = -1;
	}	
	destroy_sstream(&store);
	
	return res;
}

#endif
