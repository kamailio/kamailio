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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <curl/curl.h>

#include <xcap/resource_list.h>
#include <xcap/resource_lists_parser.h>
#include <xcap/rls_services_parser.h>
#include <xcap/xcap_client.h>

#define STR_OK(s)	(s)?(s):""

void print_indent(int indent)
{
	int i;
	
	for (i = 0; i < indent; i++) printf("  ");
	if (indent > 0) printf(" + ");
}

void trace_entry(entry_t *e, int indent)
{
	if (!e) return;
	
	print_indent(indent);
	if (e->uri) printf("%s\n", e->uri);
	else printf("???\n");
}

void trace_entry_ref(entry_ref_t *e, int indent)
{
	if (!e) return;
	
	print_indent(indent);
	if (e->ref) printf("ref: %s\n", e->ref);
	else printf("ref: ???\n");
}

void trace_external(external_t *e, int indent)
{
	if (!e) return;
	
	print_indent(indent);
	if (e->anchor) printf("ext: %s\n", e->anchor);
	else printf("ext: ???\n");
}

void trace_list(list_t *l, int indent)
{
	list_content_t *e;

	if (!l) return;
	
	print_indent(indent);
	
	if (l->name) printf("%s\n", l->name);
	else printf("???\n");

	e = SEQUENCE_FIRST(l->content);
	while (e) {
		switch (e->type) {
			case lct_list: trace_list(e->u.list, indent + 1); break;
			case lct_entry: trace_entry(e->u.entry, indent + 1); break;
			case lct_entry_ref: trace_entry_ref(e->u.entry_ref, indent + 1); break;
			case lct_external: trace_external(e->u.external, indent + 1); break;
		}
		e = SEQUENCE_NEXT(e);
	}
}

void trace_resource_lists(resource_lists_t *rl)
{
	list_t *e;
	if (!rl) {
		printf("empty list\n");
		return;
	}
	
	e = SEQUENCE_FIRST(rl->lists);
	while (e) {
		trace_list(e, 0);
		e = SEQUENCE_NEXT(e);
	}
}

void trace_packages(packages_t *p)
{
	package_t *e;
	int first = 1;
	
	printf(" [packages: ");
	if (p) {
		e = SEQUENCE_FIRST(p->package);
		while (e) {
			if (!first) printf(" ");
			else first = 0;
			printf("%s", e->name);
			e = SEQUENCE_NEXT(e);
		}
	}
	printf("]");
}

void trace_service(service_t *l, int indent)
{
	if (!l) return;
	
	print_indent(indent);
	
	if (l->uri) printf("%s", l->uri);
	else printf("???");
	if (l->packages) trace_packages(l->packages);
	printf("\n");

	switch (l->content_type) {
		case stc_list: trace_list(l->content.list, indent + 1); break;
		case stc_resource_list: print_indent(indent + 1);
								printf("@ %s\n", STR_OK(l->content.resource_list));
								break;
	}
}

void trace_rls_services(rls_services_t *rl)
{
	service_t *e;
	if (!rl) {
		printf("empty rls-services\n");
		return;
	}
	
	e = SEQUENCE_FIRST(rl->rls_services);
	while (e) {
		trace_service(e, 0);
		e = SEQUENCE_NEXT(e);
	}
}

void trace_flat_list(flat_list_t *list)
{
	flat_list_t *e = list;
	
	while (e) {
		if (e->uri) printf("%s\n", e->uri);
		else printf("???\n");
		e = e->next;
	}
}

/* -------------------------------------------------------------------------------- */

#if 0

static int xcap_test(const char *xcap_root, const char *uri)
{
	char *data = NULL;
	int dsize = 0;
	/* resource_lists_t *res_list = NULL; */
	rls_services_t *rls = NULL;
	service_t *service = NULL;
	xcap_query_t xcap;
	int res;
	str_t u = zt2str((char *)uri);
	
	/* XCAP test */
	xcap.uri = xcap_uri_for_rls_resource(xcap_root, &u);
	xcap.auth_user = "smith";
	xcap.auth_pass = "pass";
	xcap.enable_unverified_ssl_peer = 1;
	res = xcap_query(&xcap, &data, &dsize);
	if (res != 0) {
		printf("XCAP problems!\n");
		if (xcap.uri) printf("URI = %s\n", xcap.uri);
		else printf("XCAP URI not defined!\n");
		if (data) {
			printf("%s\n", data);
			free(data);
		}
		return -1;
	}

/*	printf("%s\n", data);*/

	/* parse input data */
	/*if (parse_resource_lists_xml(data, dsize, &res_list) != 0) {
		printf("Error occured during document parsing!\n");
	}
	else { 
		trace_resource_lists(res_list);
		if (res_list) free_resource_lists(res_list);
	}*/
	
	if (parse_rls_services_xml(data, dsize, &rls) == 0) {
		trace_rls_services(rls);
		if (rls) free_rls_services(rls);
	}
	else {
		/* try to take it as a service */
		if (parse_service(data, dsize, &service) == 0) {
			if (service) {
				trace_service(service, 0);
				free_service(service);
			}
		}
		else {
			printf("Error occured during document parsing! It is not rls-services nor service.\n");
			if (dsize > 0) printf("%.*s\n", dsize, data);
		}
	}

	if (data) free(data);
	return 0;
}

#endif

int test_flat(const str_t *xcap_root, const char *uri)
{
	str_t u = zt2str((char *)uri);
	xcap_query_params_t xcap;
	flat_list_t *list = NULL;
	str_t p = zt2str("presence");
	
	xcap.auth_user = "smith";
	xcap.auth_pass = "pass";
	xcap.enable_unverified_ssl_peer = 1;
	
	if (get_rls(xcap_root, &u, &xcap, &p, &list) != 0) {
		if (list) free_flat_list(list);
		printf("Failed !\n");
		return -1;
	}

	trace_flat_list(list);
	free_flat_list(list);
	
	return 0;
}

