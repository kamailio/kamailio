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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cds/dstring.h>
#include <xcap/xcap_client.h>
#include <xcap/resource_list.h>
#include <xcap/resource_lists_parser.h>
#include <xcap/rls_services_parser.h>
#include <xcap/xcap_result_codes.h>
#include <libxml/parser.h>
#include <cds/logger.h>

#define STR_OK(s)	(s)?(s):""

typedef struct _traversed_list_t {
	struct _traversed_list_t *next;
	char *uri;
} traversed_list_t;

typedef struct {
	const str_t *xcap_root;
	xcap_query_params_t *xcap_params;
	traversed_list_t *traversed;
	traversed_list_t *traversed_last;
	flat_list_t *flat;
	flat_list_t *flat_last;
} process_params_t;

void canonicalize_uri(const str_t *uri, str_t *dst)
{
	/* TODO: do the operation according to draft-ietf-simple-xcap-list-usage-05.txt */
	
	if (!dst) return;
	if (!uri) {
		dst->len = 0;
		dst->s = NULL;
		return;
	}
	if (uri->len > 0) {
		dst->s = (char*)cds_malloc(uri->len);
		if (!dst->s) dst->len = 0;
		else {
			memcpy(dst->s, uri->s, uri->len);
			dst->len = uri->len;
		}
	}
	else {
		dst->len = 0;
		dst->s = NULL;
	}
	/* DEBUG_LOG("canonicalized uri: \'%.*s\'\n", dst->len, dst->s); */
}

char *xcap_uri_for_rls_resource(const str_t *xcap_root, const str_t *uri)
{
	dstring_t s;
	int l;
	str_t c_uri;
	char *dst = NULL;

	if (!xcap_root) return NULL;
	dstr_init(&s, 2 * xcap_root->len + 32);
	dstr_append_str(&s, xcap_root);
	if (xcap_root->s[xcap_root->len - 1] != '/') dstr_append(&s, "/", 1);
	dstr_append_zt(&s, "rls-services/global/index/~~/rls-services/service[@uri=%22");
	canonicalize_uri(uri, &c_uri);
	dstr_append_str(&s, &c_uri);
	if (c_uri.s) cds_free(c_uri.s);
	
	dstr_append_zt(&s, "%22]");
	l = dstr_get_data_length(&s);
	if (l > 0) {
		dst = (char *)cds_malloc(l + 1);
		if (dst) {
			dstr_get_data(&s, dst);
			dst[l] = 0;
		}
	}
	dstr_destroy(&s);
	return dst;
}

char *xcap_uri_for_rls_services(const str_t *xcap_root)
{
	dstring_t s;
	int l;
	char *dst = NULL;

	if (!xcap_root) return NULL;
	dstr_init(&s, 2 * xcap_root->len + 32);
	dstr_append_str(&s, xcap_root);
	if (xcap_root->s[xcap_root->len - 1] != '/') dstr_append(&s, "/", 1);
	dstr_append_zt(&s, "rls-services/global/index");
	
	l = dstr_get_data_length(&s);
	if (l > 0) {
		dst = (char *)cds_malloc(l + 1);
		if (dst) {
			dstr_get_data(&s, dst);
			dst[l] = 0;
		}
	}
	dstr_destroy(&s);
	return dst;
}

void free_flat_list(flat_list_t *list)
{
	flat_list_t *f, *e;
	e = list;
	while (e) {
		f = e->next;
		if (e->uri) cds_free(e->uri);
		free_display_names(e->names);
		cds_free(e);
		e = f;
	}
}

void free_traversed_list(traversed_list_t *list)
{
	traversed_list_t *f, *e;
	e = list;
	while (e) {
		f = e->next;
		if (e->uri) cds_free(e->uri);
		cds_free(e);
		e = f;
	}
}

/* ------- helper functions (doing flat list) ------- */

static char *relative2absolute_uri(const str_t *xcap_root, const char *relative)
{
	/* FIXME: do absolute uri from ref (RFC 3986, section 5.2) */
	int len;
	int root_len = 0;
	int rel_len = 0;
	int slash_len = 0;
	char *dst = NULL;

	if (xcap_root) {
		root_len = xcap_root->len;
		if (xcap_root->s[root_len - 1] != '/') slash_len = 1;
	}
	if (relative) rel_len = strlen(relative);
	len = root_len + slash_len + rel_len + 1;

	dst = (char *)cds_malloc(len);
	if (!dst) return NULL;

	if (xcap_root) memcpy(dst, xcap_root->s, root_len);
	if (slash_len) dst[root_len] = '/';
	if (relative) memcpy(dst + root_len + slash_len, relative, rel_len);
	dst[len - 1] = 0;
	
	return dst;
}

static display_name_t *duplicate_display_name(display_name_t *src)
{
	display_name_t *n;
	if (!src) return NULL;
	n = (display_name_t*)cds_malloc(sizeof(*n));
	if (!n) return NULL;
	memset(n, 0, sizeof(*n));
	if (src->name) n->name = zt_strdup(src->name);
	if (src->lang) n->lang = zt_strdup(src->lang);
	return n;
}

int add_entry_to_flat(process_params_t *params, entry_t *entry)
{
	flat_list_t *f;
	display_name_t *d, *n, *last;
	char *uri;
	
	if (!entry) return -1;
	uri = entry->uri;
	if (!uri) return -1; /* can't be added */
	
	/* try to find the uri in the flat list first */
	f = params->flat;
	while (f) {
		if (strcmp(f->uri, uri) == 0) return 1; /* not significant for the caller */
		f = f->next;
	}

	f = (flat_list_t*)cds_malloc(sizeof(flat_list_t));
	if (!f) return -1;
	memset(f, 0, sizeof(*f));
	f->uri = zt_strdup(uri);
	f->next = NULL;
		
	if (params->flat_last) params->flat_last->next = f;
	else params->flat = f;
	params->flat_last = f;
		
	/* add all entry's names */
	last = NULL;
	d = SEQUENCE_FIRST(entry->display_names);
	while (d) {
		n = duplicate_display_name(d);
		if (n) SEQUENCE_ADD(f->names, last, n);
		d = SEQUENCE_NEXT(d);
	}

	return 0;
}

int add_uri_to_traversed(process_params_t *params, const char *uri)
{
	traversed_list_t *f;
	
	if (!uri) return -1; /* can't be added */

	/* try to find the uri in the flat list first */
	f = params->traversed;
	while (f) {
		if (!f->uri) continue;
		if (strcmp(f->uri, uri) == 0) return 1; /* this should be taken as an error */
		f = f->next;
	}

	f = (traversed_list_t*)cds_malloc(sizeof(traversed_list_t));
	if (!f) return -1;
	f->uri = zt_strdup(uri);
	f->next = NULL;
		
	if (params->traversed_last) params->traversed_last->next = f;
	else params->traversed = f;
	params->traversed_last = f;
	
	return 0;
}

/* ------- processing functions (doing flat list) ------- */

static int process_list(list_t *list, process_params_t *params);

static int process_entry(entry_t *entry, process_params_t *params)
{
	if (!entry) return RES_OK;
	if (!entry->uri) return RES_OK;
	
	/* DEBUG_LOG("processing entry with uri \'%s\'\n", STR_OK(entry->uri)); */

	add_entry_to_flat(params, entry);
	return RES_OK;
}

static int process_entry_ref(entry_ref_t *entry_ref, process_params_t *params)
{
	char *data = NULL;
	int dsize = 0;
	entry_t *entry = NULL;
	char *xcap_uri;
	int res;
	
	/* DEBUG_LOG("processing entry-ref with ref \'%s\'\n", STR_OK(entry_ref->ref)); */
	
	if (!entry_ref) return RES_OK;
	if (!entry_ref->ref) return RES_OK;
	
	if (add_uri_to_traversed(params, entry_ref->ref) != 0) {
		/* It is existing yet? */
		ERROR_LOG("Duplicate URI in traversed set\n");
		return RES_BAD_GATEWAY_ERR; /* 502 Bad GW */
	}

	/* XCAP query for the ref uri */
	xcap_uri = relative2absolute_uri(params->xcap_root, entry_ref->ref);
	res = xcap_query(xcap_uri, params->xcap_params, &data, &dsize);
	if (res != 0) {
		ERROR_LOG("XCAP problems for uri \'%s\'\n", xcap_uri ? xcap_uri: "???");
		if (data) cds_free(data);
		if (xcap_uri) cds_free(xcap_uri);
		return RES_BAD_GATEWAY_ERR; /* 502 Bad GW */
	}
	if (xcap_uri) cds_free(xcap_uri);

	/* parse document as an entry element */
	if (parse_entry_xml(data, dsize, &entry) != 0) {
		ERROR_LOG("Parsing problems!\n");
		if (entry) free_entry(entry);
		if (data) cds_free(data);
		return RES_BAD_GATEWAY_ERR; /* 502 Bad GW */
	}
	if (data) cds_free(data);
	if (!entry) return RES_INTERNAL_ERR; /* ??? */

	res = process_entry(entry, params);
	free_entry(entry);
	return res;
}

static int process_external(external_t *external, process_params_t *params)
{
	char *data = NULL;
	int dsize = 0;
	list_t *list = NULL;
	int res;
	
	/* DEBUG_LOG("processing external with anchor \'%s\'\n", STR_OK(external->anchor)); */
	
	if (!external) return RES_OK;
	if (!external->anchor) return RES_OK;
	
	if (add_uri_to_traversed(params, external->anchor) != 0) {
		/* It is existing yet? */
		ERROR_LOG("Duplicate URI in traversed set\n");
		return RES_BAD_GATEWAY_ERR; /* 502 Bad GW */
	}

	/* XCAP query for the ref uri */
	res = xcap_query(external->anchor, params->xcap_params, &data, &dsize);
	if (res != 0) {
		ERROR_LOG("XCAP problems for uri \'%s\'\n", external->anchor ? external->anchor: "???");
		if (data) cds_free(data);
		return RES_BAD_GATEWAY_ERR; /* 502 Bad GW */
	}

	/* parse document as an entry element */
	if (parse_list_xml(data, dsize, &list) != 0) {
		ERROR_LOG("Parsing problems!\n");
		if (list) free_list(list);
		if (data) cds_free(data);
		return RES_BAD_GATEWAY_ERR; /* 502 Bad GW */
	}
	if (data) cds_free(data);
	if (!list) return RES_INTERNAL_ERR; /* ??? */

	res = process_list(list, params);

	free_list(list);
	return res;
}
	
static int process_list(list_t *list, process_params_t *params)
{
	list_content_t *e;
	int res = 0;
	
	if (!list) return RES_INTERNAL_ERR;
	/* DEBUG_LOG("processing list \'%s\'\n", STR_OK(list->name)); */

	e = SEQUENCE_FIRST(list->content);
	
	while (e) {
		switch (e->type) {
			case lct_list:
				res = process_list(e->u.list, params);
				break;
			case lct_entry:
				res = process_entry(e->u.entry, params);
				break;
			case lct_entry_ref:
				res = process_entry_ref(e->u.entry_ref, params);
				break;
			case lct_external:
				res = process_external(e->u.external, params);
				break;
		}
		if (res != 0) break;
		e = SEQUENCE_NEXT(e);
	}
	
	return res;
}

static int process_resource_list(const char *rl_uri, process_params_t *params)
{
	char *data = NULL;
	int dsize = 0;
	int res = 0;
	list_t *list = NULL;

	/* DEBUG_LOG("processing resource list\n"); */

	/* do an xcap query */
	if (xcap_query(rl_uri, params->xcap_params, &data, &dsize) != 0) {
		ERROR_LOG("XCAP problems for uri \'%s\'\n", rl_uri ? rl_uri: "???");
		if (data) cds_free(data);
		return RES_BAD_GATEWAY_ERR; /* -> 502 Bad GW */
	}
	
	/* parse query result */
	if (parse_list_xml(data, dsize, &list) != 0) {
		if (data) cds_free(data);
		return RES_BAD_GATEWAY_ERR; /* -> 502 Bad GW */
	}
	if (data) {
		cds_free(data);
	}
	if (!list) return RES_INTERNAL_ERR; /* ??? */
	
	res = process_list(list, params);
	if (list) {
		free_list(list);
	}

	return res;
}

static int create_flat_list(service_t *srv, 
		xcap_query_params_t *xcap_params, 
		flat_list_t **dst)
{
	process_params_t params;
	int res = -1;
	if (!srv) return RES_INTERNAL_ERR;

	params.xcap_params = xcap_params;
	params.flat = NULL;
	params.flat_last = NULL;
	params.traversed = NULL;
	params.traversed_last = NULL;
	
	if (srv->content_type == stc_list) {
		res = process_list(srv->content.list, &params);
	}
	else {
		res = process_resource_list(srv->content.resource_list, &params);
	}
	if (dst) *dst = params.flat;

	free_traversed_list(params.traversed);
	
	return res;
}

/* ------- helper functions for rls examining ------- */

/** compare str_t and zero terminated string */
static int str_strcmp(const str_t *a, const char *b)
{
	int i;
	
	if (!a) {
		if (!b) return 0;
		else return 1;
	}
	if (!a->s) {
		if (!b) return 0;
		else return 1;
	}
	if (!b) return -1;
	for (i = 0; i < a->len; i++) {
		if (a->s[i] != b[i]) return -1;
		if (b[i] == 0) break;
	}
	if (i == a->len) return 0;
	else return 1;
}

static int verify_package(service_t *srv, const str_t *package)
{
	package_t *e;

	if (!package) return 0;
	if (!package->len) return 0;
	if (!package->s) return 0;
	if (!srv) return 1;
	
	if (srv->packages) {
		e = SEQUENCE_FIRST(srv->packages->package);
		while (e) {
			if (str_strcmp(package, e->name) == 0) return 0;
			e = SEQUENCE_NEXT(e);
		}
		ERROR_LOG("Unsupported package \"%.*s\"\n", package->len, package->s);
		return -1;
	}
	return 0;
}

static service_t *find_service(rls_services_t *rls, const str_t *uri)
{
	service_t *srv;
	
	if (!rls) return NULL;
	
	srv = SEQUENCE_FIRST(rls->rls_services);
	while (srv) {
		/* TRACE_LOG("comparing %s to %.*s\n", srv->uri, FMT_STR(*uri)); */
		if (str_strcmp(uri, srv->uri) == 0) return srv;
		srv = SEQUENCE_NEXT(srv);
	}
	return NULL;
}

/* ------- rls examining ------- */

int get_rls(const str_t *uri, xcap_query_params_t *xcap_params, 
		const str_t *package, flat_list_t **dst)
{
	char *data = NULL;
	int dsize = 0;
	service_t *service = NULL;
	char *xcap_uri = NULL;
	str_t *filename = NULL;
	int res;

	if (!dst) return RES_INTERNAL_ERR;
	
	/* get basic document */
	xcap_uri = xcap_uri_for_global_document(xcap_doc_rls_services, 
			filename, xcap_params);
	if (!xcap_uri) {
		ERROR_LOG("can't get XCAP uri\n");
		return RES_XCAP_QUERY_ERR;
	}
	
	res = xcap_query(xcap_uri, xcap_params, &data, &dsize);
	if (res != 0) {
		ERROR_LOG("XCAP problems for uri \'%s\'\n", xcap_uri);
		if (data) {
			cds_free(data);
		}
		cds_free(xcap_uri);
		return RES_XCAP_QUERY_ERR;
	}
	cds_free(xcap_uri);
	
	/* parse document as a service element in rls-sources */
	if (parse_service(data, dsize, &service) != 0) {
		ERROR_LOG("Parsing problems!\n");
		if (service) free_service(service);
		if (data) {
			cds_free(data);
		}
		return RES_XCAP_PARSE_ERR;
	}
/*	DEBUG_LOG("%.*s\n", dsize, data);*/
	if (data) cds_free(data);
	
	if (!service) {
		DEBUG_LOG("Empty service!\n");
		return RES_XCAP_QUERY_ERR;
	}

	/* verify the package */
	if (verify_package(service, package) != 0) {
		free_service(service);
		return RES_BAD_EVENT_PACKAGE_ERR;
	}
	
	/* create flat document */
	res = create_flat_list(service, xcap_params, dst);
	if (res != RES_OK) {
		ERROR_LOG("Flat list creation error\n");
		free_service(service);
		free_flat_list(*dst);
		*dst = NULL;
		return res;
	}
	free_service(service);
	
	return RES_OK;
}

int get_rls_from_full_doc(const str_t *uri, 
		/* const str_t *filename,  */
		xcap_query_params_t *xcap_params, 
		const str_t *package, flat_list_t **dst)
{
	char *data = NULL;
	int dsize = 0;
	rls_services_t *rls = NULL;
	service_t *service = NULL;
	str_t curi;
	int res;
	char *xcap_uri = NULL;
	str_t *filename = NULL;

	if (!dst) return RES_INTERNAL_ERR;
	

	/* get basic document */
	xcap_uri = xcap_uri_for_global_document(xcap_doc_rls_services, 
			filename, xcap_params);
	if (!xcap_uri) {
		ERROR_LOG("can't get XCAP uri\n");
		return -1;
	}
	
	res = xcap_query(xcap_uri, xcap_params, &data, &dsize);
	if (res != 0) {
		ERROR_LOG("XCAP problems for uri \'%s\'\n", xcap_uri);
		if (data) {
			cds_free(data);
		}
		cds_free(xcap_uri);
		return RES_XCAP_QUERY_ERR;
	}
	cds_free(xcap_uri);
	
	/* parse document as a service element in rls-sources */
	if (parse_rls_services_xml(data, dsize, &rls) != 0) {
		ERROR_LOG("Parsing problems!\n");
		if (rls) free_rls_services(rls);
		if (data) {
			cds_free(data);
		}
		return RES_XCAP_PARSE_ERR;
	}
/*	DEBUG_LOG("%.*s\n", dsize, data);*/
	if (data) cds_free(data);

	/* try to find given service according to uri */
	canonicalize_uri(uri, &curi);
	service = find_service(rls, &curi); 
	if (!service) DEBUG_LOG("Service %.*s not found!\n", FMT_STR(curi));
	str_free_content(&curi);
	
	if (!service) {
		if (rls) free_rls_services(rls);
		return RES_XCAP_QUERY_ERR;
	}

	/* verify the package */
	if (verify_package(service, package) != 0) {
		free_rls_services(rls);
		return RES_BAD_EVENT_PACKAGE_ERR;
	}
	
	/* create flat document */
	res = create_flat_list(service, xcap_params, dst);
	if (res != RES_OK) {
		ERROR_LOG("Flat list creation error\n");
		free_rls_services(rls);
		free_flat_list(*dst);
		*dst = NULL;
		return res;
	}
	free_rls_services(rls);
	
	return RES_OK;
}

static list_t *find_list(list_t *root, const char *name)
{
	list_content_t *c;
	
	if (!root) return root;
	if (!name) return root;
	if (!*name) return root; /* empty name = whole doc */

	c = root->content;
	while (c) {
		if (c->type == lct_list) {
			if (c->u.list) {
				if (strcmp(name, c->u.list->name) == 0) 
					return c->u.list;
			}
		}
		c = SEQUENCE_NEXT(c);
	}

	ERROR_LOG("list \'%s\' not found\n", name);
	
	return NULL;
}

/* catches and processes user's resource list as rls-services document */
int get_resource_list_from_full_doc(const str_t *user, 
		const str_t *filename, 
		xcap_query_params_t *xcap_params, 
		const char *list_name, flat_list_t **dst)
{
	char *data = NULL;
	int dsize = 0;
	service_t *service = NULL; 
	list_t *list = NULL, *right = NULL;
	int res;
	char *uri = NULL;

	if (!dst) return RES_INTERNAL_ERR;
	
	/* get basic document */
	uri = xcap_uri_for_users_document(xcap_doc_resource_lists,
			user, filename, xcap_params);
	if (!uri) {
		ERROR_LOG("can't get XCAP uri\n");
		return -1;
	}
	DEBUG_LOG("XCAP uri \'%s\'\n", uri);
	res = xcap_query(uri, xcap_params, &data, &dsize);
	if (res != 0) {
		ERROR_LOG("XCAP problems for uri \'%s\'\n", uri);
		if (data) {
			cds_free(data);
		}
		cds_free(uri);
		return RES_XCAP_QUERY_ERR;
	}
	cds_free(uri);
	
	/* parse document as a list element in resource-lists */
	if (parse_as_list_content_xml(data, dsize, &list) != 0) {
		ERROR_LOG("Parsing problems!\n");
		if (list) free_list(list);
		if (data) {
			cds_free(data);
		}
		return RES_XCAP_PARSE_ERR;
	}
/*	DEBUG_LOG("%.*s\n", dsize, data);*/
	if (data) cds_free(data);

	/* rs -> list */
	
	if (!list) {
		ERROR_LOG("Empty resource list!\n");
		*dst = NULL;
		return 0; /* this is not error! */
		/* return RES_INTERNAL_ERR; */
	}

	/* search for right list element */
	right = find_list(list, list_name);
		
	service = (service_t*)cds_malloc(sizeof(*service));
	if (!service) {
		ERROR_LOG("Can't allocate memory!\n");
		return RES_MEMORY_ERR;
	}
	memset(service, 0, sizeof(*service));
	service->content_type = stc_list;
	service->content.list = right;
	/*service->uri = ??? */

	/* create flat document */
	res = create_flat_list(service, xcap_params, dst);

	service->content.list = list; /* free whole document not only "right" list */
	free_service(service);
	
	if (res != RES_OK) {
		ERROR_LOG("Flat list creation error\n");
		free_flat_list(*dst);
		*dst = NULL;
		return res;
	}
	
	return RES_OK;
}
