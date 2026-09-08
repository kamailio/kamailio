/*
 * Copyright (C) 2026 kamailio.org
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

/*!
 * \file
 * \brief Regression test for the NAPTR failover state ownership (GH #4911).
 *
 * The list of the NAPTR records already used while failing over must be
 * local to the dns_srv_handle (i.e. to the SIP transaction) and must never
 * be stored in the shared dns cache: a transaction which exhausts a NAPTR
 * record must not remove that record from the candidate list of any other
 * (concurrent or subsequent) transaction resolving the same name.
 *
 * The test drives dns_naptr_sip_iterate() - the NAPTR selection state
 * machine - directly, with a hand built dns_rr list standing in for a cached
 * NAPTR entry, so that neither a dns server nor the dns cache is needed.
 *
 * Build & run it with the helper script from the top of the source tree:
 *
 \verbatim
   sh test/misc/code/dns_naptr_iterate.sh
 \endverbatim
 *
 * Exits with 0 if all the checks pass, 1 otherwise.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/core/dns_cache.h"
#include "../../../src/core/resolve.h"
#include "../../../src/core/ip_addr.h"

static int tests_failed = 0;

#define CHECK(cond, fmt, ...)                                           \
	do {                                                                \
		if(cond) {                                                      \
			printf("ok   - " fmt "\n", ##__VA_ARGS__);                  \
		} else {                                                        \
			printf("FAIL - " fmt " (%s:%d)\n", ##__VA_ARGS__, __FILE__, \
					__LINE__);                                          \
			tests_failed++;                                             \
		}                                                               \
	} while(0)


/** build a naptr_rdata as the dns cache would hold it */
static struct naptr_rdata *mk_naptr(unsigned short order, unsigned short pref,
		const char *flags, const char *services, const char *repl)
{
	struct naptr_rdata *n;
	int flags_len, services_len, repl_len;

	flags_len = strlen(flags);
	services_len = strlen(services);
	repl_len = strlen(repl);
	n = malloc(sizeof(struct naptr_rdata) + flags_len + services_len + repl_len
			   + 1);
	if(n == 0) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	memset(n, 0, sizeof(struct naptr_rdata));
	n->order = order;
	n->pref = pref;
	n->flags = &n->str_table[0];
	n->flags_len = flags_len;
	memcpy(n->flags, flags, flags_len);
	n->services = &n->str_table[flags_len];
	n->services_len = services_len;
	memcpy(n->services, services, services_len);
	/* no regexp - required for a valid sip naptr record */
	n->regexp = &n->str_table[flags_len + services_len];
	n->regexp_len = 0;
	n->repl = &n->str_table[flags_len + services_len];
	n->repl_len = repl_len;
	memcpy(n->repl, repl, repl_len);
	n->repl[repl_len] = 0;
	return n;
}


static struct dns_rr *mk_naptr_rr_lst(void)
{
	struct dns_rr *head, *rr;
	int i;
	/* all of them udp, so that the selection depends only on order/pref and
	 * not on the (config dependent) protocol preferences */
	static const char *repl[3] = {"_sip._udp.edge1.example.net",
			"_sip._udp.edge2.example.net", "_sip._udp.edge3.example.net"};

	head = 0;
	for(i = 2; i >= 0; i--) {
		rr = malloc(sizeof(struct dns_rr));
		if(rr == 0) {
			fprintf(stderr, "out of memory\n");
			exit(1);
		}
		memset(rr, 0, sizeof(struct dns_rr));
		rr->rdata = mk_naptr(10 + i * 10, 100, "s", "SIP+D2U", repl[i]);
		rr->expire = (ticks_t)-1;
		rr->next = head;
		head = rr;
	}
	return head;
}


static void free_naptr_rr_lst(struct dns_rr *head)
{
	struct dns_rr *rr, *nxt;

	for(rr = head; rr; rr = nxt) {
		nxt = rr->next;
		free(rr->rdata);
		free(rr);
	}
}


/** returns the repl of the naptr record selected next by h, 0 if none left */
static const char *next_naptr(struct dns_srv_handle *h, struct dns_rr *rr_lst)
{
	static char buf[MAX_DNS_NAME];
	str srv_name;
	char proto;

	if(dns_naptr_sip_iterate(rr_lst, &h->naptr_tried_rrs, &srv_name, &proto)
			== 0)
		return 0;
	if(srv_name.len >= (int)sizeof(buf))
		return "<too long>";
	memcpy(buf, srv_name.s, srv_name.len);
	buf[srv_name.len] = 0;
	return buf;
}


static int streq(const char *a, const char *b)
{
	return (a != 0) && (b != 0) && (strcmp(a, b) == 0);
}


/** snapshot of the rdata of every record, to check that the (shared) cached
 * entry is never modified by the failover of a single transaction */
static char *snap_naptr_rr_lst(struct dns_rr *head, int *len)
{
	struct dns_rr *rr;
	char *buf;
	int sz, off;

	sz = 0;
	for(rr = head; rr; rr = rr->next)
		sz += (int)NAPTR_RDATA_SIZE(*(struct naptr_rdata *)rr->rdata);
	buf = malloc(sz);
	if(buf == 0) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	off = 0;
	for(rr = head; rr; rr = rr->next) {
		sz = (int)NAPTR_RDATA_SIZE(*(struct naptr_rdata *)rr->rdata);
		memcpy(buf + off, rr->rdata, sz);
		off += sz;
	}
	*len = off;
	return buf;
}


int main(int argc, char **argv)
{
	struct dns_rr *rr_lst;
	struct dns_srv_handle ha, hb;
	const char *r;
	char *snap, *snap2;
	int snap_len, snap2_len;

	rr_lst = mk_naptr_rr_lst();
	snap = snap_naptr_rr_lst(rr_lst, &snap_len);

	/* --- scenario 2: one handle walks through all the naptr records ---- */
	dns_srv_handle_init(&ha);
	CHECK(ha.naptr_tried_rrs == 0, "a fresh handle has no naptr record tried");

	r = next_naptr(&ha, rr_lst);
	CHECK(streq(r, "_sip._udp.edge1.example.net"),
			"handle A selects the first naptr record (got '%s')", r ? r : "-");
	CHECK(ha.naptr_tried_rrs != 0, "handle A recorded the record it used");

	r = next_naptr(&ha, rr_lst);
	CHECK(streq(r, "_sip._udp.edge2.example.net"),
			"handle A continues with the next naptr record (got '%s')",
			r ? r : "-");

	/* the srv/ip state of the previous naptr record is dropped between two
	 * naptr records, the naptr state must survive it */
	dns_srv_handle_reset(&ha);
	CHECK(ha.naptr_tried_rrs != 0,
			"dns_srv_handle_reset() keeps the naptr records already tried");

	r = next_naptr(&ha, rr_lst);
	CHECK(streq(r, "_sip._udp.edge3.example.net"),
			"handle A continues with the last naptr record (got '%s')",
			r ? r : "-");

	r = next_naptr(&ha, rr_lst);
	CHECK(r == 0, "handle A has no naptr record left (got '%s')", r ? r : "-");

	/* --- scenario 3/5/6: an independent handle is not affected ---------- */
	dns_srv_handle_init(&hb);
	CHECK(hb.naptr_tried_rrs == 0,
			"a new handle starts with all the naptr records available");
	r = next_naptr(&hb, rr_lst);
	CHECK(streq(r, "_sip._udp.edge1.example.net"),
			"handle B still selects the first naptr record, the one handle A"
			" exhausted (got '%s')",
			r ? r : "-");

	/* --- the shared cached records were not modified at all ------------- */
	snap2 = snap_naptr_rr_lst(rr_lst, &snap2_len);
	CHECK((snap_len == snap2_len) && (memcmp(snap, snap2, snap_len) == 0),
			"exhausting the naptr records of a handle leaves the cached"
			" entry byte for byte unchanged");
	free(snap2);

	/* --- scenario 4: two interleaved handles ---------------------------- */
	dns_srv_handle_init(&ha);
	dns_srv_handle_init(&hb);
	r = next_naptr(&ha, rr_lst);
	CHECK(streq(r, "_sip._udp.edge1.example.net"),
			"interleaved: A gets record 1 (got '%s')", r ? r : "-");
	r = next_naptr(&hb, rr_lst);
	CHECK(streq(r, "_sip._udp.edge1.example.net"),
			"interleaved: B gets record 1 too (got '%s')", r ? r : "-");
	r = next_naptr(&ha, rr_lst);
	CHECK(streq(r, "_sip._udp.edge2.example.net"),
			"interleaved: A gets record 2 (got '%s')", r ? r : "-");
	r = next_naptr(&hb, rr_lst);
	CHECK(streq(r, "_sip._udp.edge2.example.net"),
			"interleaved: B gets record 2 (got '%s')", r ? r : "-");
	r = next_naptr(&ha, rr_lst);
	CHECK(streq(r, "_sip._udp.edge3.example.net"),
			"interleaved: A gets record 3 (got '%s')", r ? r : "-");
	r = next_naptr(&ha, rr_lst);
	CHECK(r == 0, "interleaved: A is exhausted (got '%s')", r ? r : "-");
	r = next_naptr(&hb, rr_lst);
	CHECK(streq(r, "_sip._udp.edge3.example.net"),
			"interleaved: B still gets record 3 after A gave up (got '%s')",
			r ? r : "-");

	/* --- a copied handle inherits the state of the original ------------- */
	hb = ha; /* dns_srv_handle_cpy() without the refcnt update, the handles
			  * hold no dns cache entry here */
	CHECK(hb.naptr_tried_rrs == ha.naptr_tried_rrs,
			"copying a handle carries over the naptr records already tried");

	snap2 = snap_naptr_rr_lst(rr_lst, &snap2_len);
	CHECK((snap_len == snap2_len) && (memcmp(snap, snap2, snap_len) == 0),
			"the cached entry is still unchanged after the interleaved runs");
	free(snap2);
	free(snap);

	free_naptr_rr_lst(rr_lst);
	printf("%s\n", tests_failed ? "TESTS FAILED" : "all tests passed");
	return tests_failed ? 1 : 0;
}
