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

#include "reg_rpc.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "registrar_notify.h"


static const char *reg_rpc_dereg_impu_doc[2] = {
		"De-register IMPU from S-CSCF", 0};

extern usrloc_api_t ul;

static void reg_rpc_dereg_impu(rpc_t *rpc, void *ctx)
{
	str impu;
	int res;
	udomain_t *domain;
	struct impurecord *impu_rec;
	impu_contact_t *impucontact;

	if(rpc->scan(ctx, "S", &impu) < 1) {
		rpc->fault(ctx, 400, "required IMPU argument");
		return;
	}

	LM_DBG("Request to re-register impu <%.*s>\n", impu.len, impu.s);

	res = ul.get_udomain("location", &domain);
	if(res != 0) {
		LM_ERR("Failed to get domain\n");
		return;
	}

	ul.lock_udomain(domain, &impu);
	res = ul.get_impurecord(domain, &impu, &impu_rec);
	if(res != 0) {
		LM_ERR("Trying to de-register '%.*s' Not found in usrloc\n", impu.len,
				impu.s);
		ul.unlock_udomain(domain, &impu);
		return;
	}

	impucontact = impu_rec->linked_contacts.head;
	while(impucontact) {
		LM_DBG("Deleting contact with AOR [%.*s]\n",
				impucontact->contact->aor.len, impucontact->contact->aor.s);
		ul.lock_contact_slot_i(impucontact->contact->sl);
		impucontact->contact->state = CONTACT_DELETE_PENDING;
		if(impu_rec->shead) {
			//send NOTIFY to all subscribers of this IMPU.
			notify_subscribers(impu_rec, impucontact->contact, 0, 0,
					IMS_REGISTRAR_CONTACT_UNREGISTERED);
		}
		impucontact->contact->state = CONTACT_DELETED;
		ul.unlock_contact_slot_i(impucontact->contact->sl);
		impucontact = impucontact->next;
	}

	ul.unlock_udomain(domain, &impu);
}

rpc_export_t reg_rpc[] = {
		{"regscscf.dereg_impu", reg_rpc_dereg_impu, reg_rpc_dereg_impu_doc, 0},
		{0, 0, 0, 0}};
