/*
 * $Id$
 *
 * usrloc module
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "../../core/ip_addr.h"
#include "../../core/dprint.h"

#include "ul_rpc.h"
#include "dlist.h"
#include "ucontact.h"
#include "udomain.h"

static const char *ul_rpc_dump_doc[2] = {"Dump SCSCF user location tables", 0};
static const char *ul_rpc_showimpu_doc[2] = {"Dump SCSCF IMPU information", 0};
static const char *ul_rpc_snapshot_doc[2] = {
		"Dump snapshot of SCSCF usrloc to file", 0};

static unsigned int contact_buflen = 0;
static str contact_buf = {0, 0};

static void ul_rpc_snapshot(rpc_t *rpc, void *ctx)
{
	str filename;
	time_t current_time;
	struct tm *t;
	char str_time[100];

	if(rpc->scan(ctx, "S", &filename) < 1) {
		rpc->fault(ctx, 400, "required filename to dump usrloc snapshot to");
		return;
	}


	LM_DBG("Dumping S-CSCF usrloc snapshot to file: [%.*s]\n", filename.len,
			filename.s);

	current_time = time(NULL);
	t = localtime(&current_time);
	strftime(str_time, 100, "%Y-%m-%d %H:%M:%S", t);
	FILE *f = fopen(filename.s, "a");
	fprintf(f, "########################################################\n");
	fprintf(f, "Dumping USRLOC for S-CSCF: %s\n", str_time);
	fprintf(f, "########################################################\n\n");
	print_all_udomains(f);
	fflush(f);
	fclose(f);
}

static void ul_rpc_show_impu(rpc_t *rpc, void *ctx)
{
	int i, j;
	impu_contact_t *impucontact;
	str impu;
	int res;
	udomain_t *domain;
	struct impurecord *impu_rec;
	ucontact_t *contact;
	void *ah, *sh, *ch, *cdh, *sdh, *sph, *spi;
	char numstr[21
				+ 16]; //enough for all numbers up to 64bits + longest length of field name (16)

	if(rpc->scan(ctx, "S", &impu) < 1) {
		rpc->fault(ctx, 400, "required IMPU argument");
		return;
	}

	LM_DBG("searching for impu <%.*s>\n", impu.len, impu.s);

	res = get_udomain("location", &domain);
	if(res != 0) {
		LM_ERR("Failed to get domain\n");
		return;
	}

	lock_udomain(domain, &impu);
	res = get_impurecord(domain, &impu, &impu_rec);
	if(res != 0) {
		unlock_udomain(domain, &impu);
		return;
	}

	//now print the data for this IMPU record
	if(rpc->add(ctx, "{", &ah) < 0) {
		rpc->fault(ctx, 500, "Internal error creating IMPU struct");
		unlock_udomain(domain, &impu);
		return;
	}

	if(rpc->struct_add(ah, "SsdSSSS", "impu", &impu, "state",
			   get_impu_regstate_as_string(impu_rec->reg_state), "barring",
			   impu_rec->barring, "ccf1", &impu_rec->ccf1, "ccf2",
			   &impu_rec->ccf2, "ecf1", &impu_rec->ecf1, "ecf2",
			   &impu_rec->ecf2)
			< 0) {
		rpc->fault(ctx, 500, "Internal error adding impu data");
		unlock_udomain(domain, &impu);
		return;
	}

	if(rpc->struct_add(ah, "{", "subscription", &sh) < 0) {
		rpc->fault(ctx, 500, "Internal error adding impu subscription data");
		unlock_udomain(domain, &impu);
		return;
	}

	ims_subscription *subscription = impu_rec->s;
	lock_subscription(subscription);
	//add subscription data
	if(rpc->struct_add(sh, "S{", "impi", &subscription->private_identity,
			   "service profiles", &sph)
			< 0) {
		rpc->fault(ctx, 500, "Internal error adding impu subscription data");
		unlock_subscription(subscription);
		unlock_udomain(domain, &impu);
		return;
	}

	//add subscription detail information
	for(i = 0; i < subscription->service_profiles_cnt; i++) {
		sprintf(numstr, "%d", i + 1);
		if(rpc->struct_add(sph, "{", numstr, &sdh) < 0) {
			rpc->fault(ctx, 500,
					"Internal error adding impu subscription detail data");
			unlock_subscription(subscription);
			unlock_udomain(domain, &impu);
			return;
		}
		if(rpc->struct_add(sdh, "{", "impus", &spi) < 0) {
			rpc->fault(
					ctx, 500, "Internal error adding impu subscription data");
			unlock_subscription(subscription);
			unlock_udomain(domain, &impu);
			return;
		}

		for(j = 0; j < subscription->service_profiles[i].public_identities_cnt;
				j++) {
			sprintf(numstr, "%d", j + 1);
			if(rpc->struct_add(spi, "S", numstr,
					   &subscription->service_profiles[i]
								.public_identities[j]
								.public_identity)
					< 0) {
				rpc->fault(ctx, 500,
						"Internal error adding impu subscription detail data");
				unlock_subscription(subscription);
				unlock_udomain(domain, &impu);
				return;
			}
		}
	}

	unlock_subscription(subscription);

	//add contact data
	if(rpc->struct_add(ah, "{", "contacts", &ch) < 0) {
		rpc->fault(ctx, 500, "Internal error adding impu contact data");
		unlock_udomain(domain, &impu);
		return;
	}

	i = 0;


	impucontact = impu_rec->linked_contacts.head;
	while(impucontact) {
		contact = impucontact->contact;
		//contact is not null terminated so we need to create a null terminated version
		if(!contact_buf.s || (contact_buf.len <= contact->c.len)) {
			if(contact_buf.s && contact_buf.len <= contact->c.len) {
				pkg_free(contact_buf.s);
			}
			contact_buflen = contact->c.len + 1;
			contact_buf.s = (char *)pkg_malloc(contact_buflen);
			if(!contact_buf.s) {
				LM_ERR("no more pkg memory");
				rpc->fault(
						ctx, 500, "Internal error adding impu contact header");
				unlock_udomain(domain, &impu);
				return;
			}
		}
		memcpy(contact_buf.s, contact->c.s, contact->c.len);
		contact_buf.s[contact->c.len] = '\0';
		contact_buf.len = contact->c.len;

		LM_DBG("contact is %s\n", contact_buf.s);
		if(rpc->struct_add(ch, "{", contact_buf.s, &cdh) < 0) {
			rpc->fault(ctx, 500, "Internal error adding impu contact header");
			unlock_udomain(domain, &impu);
			return;
		}
		if(rpc->struct_add(cdh, "dS", "expires", contact->expires - time(NULL),
				   "client", &contact->user_agent)
				< 0) {
			rpc->fault(ctx, 500, "Internal error adding impu contact data");
			unlock_udomain(domain, &impu);
			return;
		}
		impucontact = impucontact->next;
	}

	unlock_udomain(domain, &impu);
}

static void ul_rpc_dump(rpc_t *rpc, void *ctx)
{
	dlist_t *dl;
	udomain_t *dom;
	void *th;
	void *sh;
	int max, n, i;

	for(dl = root; dl; dl = dl->next) {
		dom = dl->d;
		if(rpc->add(ctx, "{", &th) < 0) {
			rpc->fault(ctx, 500, "Internal error creating top rpc");
			return;
		}
		if(rpc->struct_add(
				   th, "Sd", "Domain", &dl->name, "Size", (int)dom->size)
				< 0) {
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		for(i = 0, n = 0, max = 0; i < dom->size; i++) {
			n += dom->table[i].n;
			if(max < dom->table[i].n)
				max = dom->table[i].n;
		}

		/* extra attributes node */
		if(rpc->struct_add(th, "{", "Stats", &sh) < 0) {
			rpc->fault(ctx, 500, "Internal error creating stats struct");
			return;
		}
		if(rpc->struct_add(sh, "dd", "Records", n, "Max-Slots", max) < 0) {
			rpc->fault(ctx, 500, "Internal error adding stats");
			return;
		}
	}
}

rpc_export_t ul_rpc[] = {{"ulscscf.status", ul_rpc_dump, ul_rpc_dump_doc, 0},
		{"ulscscf.showimpu", ul_rpc_show_impu, ul_rpc_showimpu_doc, 0},
		{"ulscscf.snapshot", ul_rpc_snapshot, ul_rpc_snapshot_doc, 0},
		{0, 0, 0, 0}};
