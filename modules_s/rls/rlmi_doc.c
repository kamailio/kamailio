#include "rlmi_doc.h"
#include "result_codes.h"
#include <cds/dstring.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
	
static void add_virtual_subscriptions_to_rlmi(dstring_t *doc, rl_subscription_t *s, const char *part_id)
{
	int i, j, ncnt, cnt;
	virtual_subscription_t *vs;
	vs_display_name_t dn;
	char tmp[32];
	
	/* add all list elements */
	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;

		dstr_append_zt(doc, "\t<resource uri=\"");
		dstr_append_str(doc, &vs->uri);
		dstr_append_zt(doc, "\">\r\n");

		/* add display names */
		ncnt = vector_size(&vs->display_names);
		for (j = 0; j < ncnt; j++) {
			if (vector_get(&vs->display_names, j, &dn) != 0) continue;

			if (dn.lang.len > 0) {
				dstr_append_zt(doc, "\t\t<name language=\"");
				dstr_append_str(doc, &dn.lang);
				dstr_append_zt(doc, "\">");
			}
			else dstr_append_zt(doc, "\t\t<name>");
			dstr_append_str(doc, &dn.name);
			dstr_append_zt(doc, "</name>\r\n");
		}
	
		sprintf(tmp, "vs%di%d", i, 1);
		dstr_append_zt(doc, "\t\t<instance id=\"");
		dstr_append_zt(doc, tmp);
		dstr_append_zt(doc, "\" state=\"");
		switch (vs->status) {
			case subscription_active: 
					dstr_append_zt(doc, "active\"");
					break;
			case subscription_pending: 
					dstr_append_zt(doc, "pending\"");
					break;
			case subscription_terminated_pending: 
			case subscription_terminated: 
					dstr_append_zt(doc, "terminated\" reason=\"closed\"");
					break;
			case subscription_terminated_pending_to: 
			case subscription_terminated_to: 
					dstr_append_zt(doc, 
						"terminated\" reason=\"timeout\"");
					break;
			case subscription_uninitialized: 
					dstr_append_zt(doc, "pending\"");
					/* this is an error ! */
					LOG(L_ERR, "generating RLMI for an unitialized virtual subscription!\n");
					break;
		}
	
		if (vs->state_document.len > 0) {
			sprintf(tmp, "%d", i);
			dstr_append_zt(doc, " cid=\"");
			dstr_append_zt(doc, part_id);
			dstr_append_zt(doc, tmp);
			dstr_append_zt(doc, "\"/>\r\n");
		}
		else dstr_append_zt(doc, "/>\r\n");
		
		dstr_append_zt(doc, "\t</resource>\r\n");
	}
}

static void add_virtual_subscriptions_documents(dstring_t *doc, 
		rl_subscription_t *s, const char *boundary_str, const char *part_id)
{
	int i, cnt;
	virtual_subscription_t *vs;
	char tmp[32];

	/* add all list elements */
	cnt = ptr_vector_size(&s->vs);
	for (i = 0; i < cnt; i++) {
		vs = ptr_vector_get(&s->vs, i);
		if (!vs) continue;
		
		if (vs->state_document.len < 1) {
			vs = vs->next;
			continue;
		}
		if (vs->content_type.len < 1) {
			LOG(L_ERR, "can't send resource status document for unknown type\n");
			vs = vs->next;
			continue;
		}

		dstr_append(doc, "--", 2);
		dstr_append_zt(doc, boundary_str);
		dstr_append(doc, "\r\n", 2);
		sprintf(tmp, "%d", i);
		dstr_append_zt(doc, "Content-Transfer-Encoding: binary\r\nContent-ID: ");
		dstr_append_zt(doc, part_id);
		dstr_append_zt(doc, tmp);
		dstr_append_zt(doc, "\r\nContent-Type: ");
		dstr_append_str(doc, &vs->content_type);
		dstr_append_zt(doc, "\r\n\r\n");

		dstr_append_str(doc, &vs->state_document);
		dstr_append(doc, "\r\n", 2);
		dstr_append(doc, "\r\n", 2);
	}
	
	
}

int create_rlmi_document(str *dst, str *content_type_dst, rl_subscription_t *s, int full_info)
{
	dstring_t doc, cont;
	char tmp[32];
	char start_str[64];
	char boundary_str[64];
	char part_id[64];

	if ((!s) || (!dst) || (!content_type_dst)) return RES_INTERNAL_ERR;

	sprintf(start_str, "qwW%dpPdxX%d", rand(), rand());
	sprintf(boundary_str, "RewXdpxR%dxA%d", rand(), rand());
	sprintf(part_id, "id%di%dx", rand(), rand());
	
	/* --- build NOTIFY body --- */
	dstr_init(&doc, 256);

	dstr_append(&doc, "--", 2);
	dstr_append_zt(&doc, boundary_str);
	dstr_append(&doc, "\r\n", 2);
	dstr_append_zt(&doc, 
			"Content-Transfer-Encoding: binary\r\n"
			"Content-ID: ");
	dstr_append_zt(&doc, start_str);
	dstr_append_zt(&doc, "\r\n");

	dstr_append_zt(&doc, 
		"Content-Type: application/rlmi+xml;charset=\"UTF-8\"\r\n");
	dstr_append(&doc, "\r\n", 2);
	
	/* -- RLMI document -- */
	dstr_append_zt(&doc, 
		"<?xml version=\"1.0\"?>\r\n"
		"<list xmlns=\"urn:ietf:params:xml:ns:rlmi\" "
		"uri=\"");
	dstr_append_str(&doc, rls_get_uri(s));
	dstr_append_zt(&doc, "\" version=\"");
	sprintf(tmp, "%d", s->doc_version); 
	dstr_append_zt(&doc, tmp);

	dstr_append_zt(&doc, "\" fullState=\"true\">\r\n");
	
	/* FIXME: as soon as will be finished partial notification document
	 * if (full_info) dstr_append_zt(&doc, "\" fullState=\"true\">\r\n");
	else dstr_append_zt(&doc, "\" fullState=\"false\">\r\n");
	*/

	/* add all virtual subscriptions to the RLMI document */
	add_virtual_subscriptions_to_rlmi(&doc, s, part_id);

	dstr_append_zt(&doc, "</list>\r\n\r\n");
	
	/* add all virtual subscriptions status documents */
	add_virtual_subscriptions_documents(&doc, s, boundary_str, part_id);
	
	dstr_append(&doc, "--", 2);
	dstr_append_zt(&doc, boundary_str);
	dstr_append(&doc, "--\r\n", 4);
	dstr_append(&doc, "\r\n", 2);
	
	/* --- build content type --- */
	dstr_init(&cont, 256);
	dstr_append_zt(&cont, 
			"multipart/related;type=\"application/rlmi+xml\";"
			"start=\"");
	dstr_append_zt(&cont, start_str);
	dstr_append_zt(&cont, "\";boundary=\"");
	dstr_append_zt(&cont, boundary_str);
	dstr_append_zt(&cont, "\";");
	
	/* --- store output strings --- */
	
	dstr_get_str(&doc, dst);
	dstr_destroy(&doc);
	
	dstr_get_str(&cont, content_type_dst);
	dstr_destroy(&cont);
	
	/* increment version for next NOTIFY document */
	s->doc_version++;

	return RES_OK;
}

