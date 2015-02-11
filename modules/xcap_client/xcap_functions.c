/*
 * xcap_client module - XCAP client for Kamailio
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <curl/curl.h>
#include "../../mem/mem.h"
#include "../../lib/srdb1/db.h"
#include "xcap_functions.h"
#include "xcap_client.h"
#include "../presence/hash.h"


#define ETAG_HDR          "Etag: "
#define ETAG_HDR_LEN      strlen("Etag: ")

size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream);
char* get_xcap_path(xcap_get_req_t req);

int bind_xcap(xcap_api_t* api)
{
	if (!api) 
	{
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	api->get_elem= xcapGetElem;
	api->int_node_sel= xcapInitNodeSel;
	api->add_step= xcapNodeSelAddStep;
	api->add_terminal= xcapNodeSelAddTerminal;
	api->free_node_sel= xcapFreeNodeSel;
	api->register_xcb= register_xcapcb;
	api->getNewDoc= xcapGetNewDoc;
	
	return 0;
}

void xcapFreeNodeSel(xcap_node_sel_t* node)
{
	step_t* s, *p;
	ns_list_t* n, *m;

	s= node->steps;
	while(s)
	{
		p= s;
		s= s->next;
		pkg_free(p->val.s);
		pkg_free(p);
	}

	n= node->ns_list;
	while(n)
	{
		m= n;
		n= n->next;
		pkg_free(m->value.s);
		pkg_free(m);
	}

	pkg_free(node);

}

xcap_node_sel_t* xcapInitNodeSel(void)
{
	xcap_node_sel_t* nsel= NULL;

	nsel= (xcap_node_sel_t*)pkg_malloc(sizeof(xcap_node_sel_t));
	if(nsel== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(nsel, 0, sizeof(xcap_node_sel_t));
	nsel->steps= (step_t*)pkg_malloc(sizeof(step_t));
	if(nsel->steps== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(nsel->steps, 0, sizeof(step_t));
	nsel->last_step= nsel->steps;

	nsel->ns_list= (ns_list_t*)pkg_malloc(sizeof(ns_list_t));
	if(nsel->ns_list== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(nsel->ns_list, 0, sizeof(ns_list_t));
	nsel->last_ns= nsel->ns_list;

	return nsel;

error:
	if(nsel)
	{
		if(nsel->steps)
			pkg_free(nsel->steps);
		if(nsel->ns_list)
			pkg_free(nsel->ns_list);
		pkg_free(nsel);
	}

	return NULL;
}

xcap_node_sel_t* xcapNodeSelAddStep(xcap_node_sel_t* curr_sel, str* name,
		str* namespace, int pos, attr_test_t*  attr_test, str* extra_sel)
{
	int size= 0;
	str new_step= {NULL, 0};
	step_t* s= NULL;
	char ns_card= 'a';
	ns_list_t* ns= NULL;

	if(name)
		size+= name->len;
	else
		size+= 1;

	if(namespace)
		size+= 2;
	if(pos> 0)
		size+= 7;
	if(attr_test)
		size+= 2+ attr_test->name.len+ attr_test->value.len;
	if(extra_sel)
		size+= 2+ extra_sel->len;
	
	new_step.s= (char*)pkg_malloc(size* sizeof(char));
	if(new_step.s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	if(name)
	{
		if(namespace)
		{
			ns_card= curr_sel->ns_no+ 'a';
			curr_sel->ns_no++;

			if(ns_card> 'z')
			{
				LM_ERR("Insuficient name cards for namespaces\n");
				goto error;
			}
			new_step.len= sprintf(new_step.s, "%c:", ns_card);		
		}
		memcpy(new_step.s+new_step.len, name->s, name->len);
		new_step.len+= name->len;
	}
	else
		memcpy(new_step.s+new_step.len, "*", 1);

	if(attr_test)
	{
		new_step.len+= sprintf(new_step.s+ new_step.len, "[%.*s=%.*s]", attr_test->name.len,
				attr_test->name.s, attr_test->value.len, attr_test->value.s);
	}
	if(pos> 0)
		new_step.len+= sprintf(new_step.s+ new_step.len, "[%d]", pos);

	if(extra_sel)
	{
		memcpy(new_step.s+ new_step.len, extra_sel->s, extra_sel->len);
		new_step.len= extra_sel->len;
	}

	s= (step_t*)pkg_malloc(sizeof(step_t));
	if(s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	s->val= new_step;
	s->next= NULL;

	curr_sel->last_step->next= s;
	curr_sel->last_step= s;

	/* add the namespace binding if present */
	if(namespace)
	{
		ns= (ns_list_t*)pkg_malloc(sizeof(ns_list_t));
		if(ns== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		ns->name= ns_card;
		ns->value.s= (char*)pkg_malloc(namespace->len* sizeof(char));
		if(ns->value.s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(ns->value.s, namespace->s, namespace->len);
		ns->value.len= namespace->len;

		curr_sel->last_ns->next= ns;
		curr_sel->last_ns= ns;
	}

	curr_sel->size+= 1+ new_step.len;
	if(namespace->len)
	{
		curr_sel->size+= namespace->len+ 3;
	}
	
	return curr_sel;

error:
	if(new_step.s)
		pkg_free(new_step.s);
	if(s)
		pkg_free(s);
	if(ns)
	{
		if(ns->value.s)
			pkg_free(ns->value.s);
		pkg_free(ns);
	}

	return NULL;
}

xcap_node_sel_t* xcapNodeSelAddTerminal(xcap_node_sel_t* curr_sel, 
		char* attr_sel, char* namespace_sel, char* extra_sel )
{

	return NULL;
}

char* get_node_selector(xcap_node_sel_t* node_sel)
{
	char* buf= NULL;
	step_t* s;
	int len= 0;
	ns_list_t* ns_elem;

	buf= (char*)pkg_malloc((node_sel->size+ 10)* sizeof(char));
	if(buf== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	s= node_sel->steps->next;

	while(1)
	{
		memcpy(buf+ len, s->val.s, s->val.len);
		len+= s->val.len;
		s= s->next;
		if(s)
			buf[len++]= '/';
		else
			break;
	}
	ns_elem= node_sel->ns_list;

	if(ns_elem)
		buf[len++]= '?';
	
	while(ns_elem)
	{
		len+= sprintf(buf+ len, "xmlns(%c=%.*s)", ns_elem->name,
				ns_elem->value.len, ns_elem->value.s);
		ns_elem= ns_elem->next;
	}
	
	buf[len]= '\0';

	return buf;

error:
	return NULL;
}

char* xcapGetNewDoc(xcap_get_req_t req, str user, str domain)
{
	char* etag= NULL;
	char* doc= NULL;
	db_key_t query_cols[9];
	db_val_t query_vals[9];
	int n_query_cols = 0;
	char* path= NULL;

	path= get_xcap_path(req);
	if(path== NULL)
	{
		LM_ERR("while constructing xcap path\n");
		return NULL;
	}
	/* send HTTP request */
	doc= send_http_get(path, req.port, NULL, 0, &etag);
	if(doc== NULL)
	{
		LM_DBG("the searched document was not found\n");
		goto done;
	}

	if(etag== NULL)
	{
		LM_ERR("no etag found\n");
		pkg_free(doc); 
		doc= NULL;
		goto done;
	}
	/* insert in xcap table*/
	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = user;
	n_query_cols++;
	
	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = domain;
	n_query_cols++;
	
	query_cols[n_query_cols] = &str_doc_type_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= req.doc_sel.doc_type;
	n_query_cols++;

	query_cols[n_query_cols] = &str_doc_col;
	query_vals[n_query_cols].type = DB1_STRING;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.string_val= doc;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_vals[n_query_cols].type = DB1_STRING;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.string_val= etag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_source_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= XCAP_CL_MOD;
	n_query_cols++;

	query_cols[n_query_cols] = &str_doc_uri_col;
	query_vals[n_query_cols].type = DB1_STRING;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.string_val= path;
	n_query_cols++;
	
	query_cols[n_query_cols] = &str_port_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= req.port;
	n_query_cols++;

	if (xcap_dbf.use_table(xcap_db, &xcap_db_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcap_db_table.len, xcap_db_table.s);
		goto done;
	}
	
	if(xcap_dbf.insert(xcap_db, query_cols, query_vals, n_query_cols)< 0)
	{
		LM_ERR("in sql insert\n");
		goto done;
	}

done:
	pkg_free(path);
	return doc;
}

char* get_xcap_path(xcap_get_req_t req)
{
	int len= 0, size;
	char* path= NULL;
	char* node_selector= NULL;

	len= (strlen(req.xcap_root)+ 1+ req.doc_sel.auid.len+ 5+
			req.doc_sel.xid.len+ req.doc_sel.filename.len+ 50)* sizeof(char);
	
	if(req.node_sel)
		len+= req.node_sel->size;

	path= (char*)pkg_malloc(len);
	if(path== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	if(req.node_sel)
	{
		node_selector= get_node_selector(req.node_sel);
		if(node_selector== NULL)
		{
			LM_ERR("while constructing node selector\n");
			goto error;
		}
	}
	
	size= sprintf(path, "%s/%.*s/", req.xcap_root, req.doc_sel.auid.len,
			req.doc_sel.auid.s);

	if(req.doc_sel.type==USERS_TYPE)
		size+= sprintf(path+ size, "%s/%.*s/", "users", req.doc_sel.xid.len,
				req.doc_sel.xid.s);
	else
		size+= sprintf(path+ size, "%s/", "global");
	size+= sprintf(path+ size, "%.*s", req.doc_sel.filename.len,
			req.doc_sel.filename.s);
	
	if(node_selector)
	{
		size+= sprintf(path+ size, "/~~%s", node_selector);
	}

	if(size> len)
	{
		LM_ERR("buffer size overflow\n");
		goto error;
	}
	pkg_free(node_selector);

	return path;
	
error:
	if(path)
		pkg_free(path);
	if(node_selector)
		pkg_free(node_selector);
	return NULL;
}

/* xcap_root must be a NULL terminated string */

char* xcapGetElem(xcap_get_req_t req, char** etag)
{
	char* path= NULL;
	char* stream= NULL;
	
	path= get_xcap_path(req);
	if(path== NULL)
	{
		LM_ERR("while constructing xcap path\n");
		return NULL;
	}

	stream= send_http_get(path, req.port, req.etag, req.match_type, etag);
	if(stream== NULL)
	{
		LM_DBG("the serched element was not found\n");
	}
	
	if(etag== NULL)
	{
		LM_ERR("no etag found\n");
		pkg_free(stream);
		stream= NULL;
	}

	if(path)
		pkg_free(path);
	
	return stream;
}

size_t get_xcap_etag( void *ptr, size_t size, size_t nmemb, void *stream)
{
	int len= 0;
	char* etag= NULL;

	if(strncasecmp(ptr, ETAG_HDR, ETAG_HDR_LEN)== 0)
	{
		len= size* nmemb- ETAG_HDR_LEN;
		etag= (char*)pkg_malloc((len+ 1)* sizeof(char));
		if(etag== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(etag, ptr+ETAG_HDR_LEN, len);
		etag[len]= '\0';
		*((char**)stream)= etag;
	}
	return len;

error:
	return -1;
}

char* send_http_get(char* path, unsigned int xcap_port, char* match_etag,
		int match_type, char** etag)
{
	int len;
	char* stream= NULL;
	CURLcode ret_code;
	CURL* curl_handle= NULL;
	static char buf[128];
	char* match_header= NULL;
	*etag= NULL;
	
	if(match_etag)
	{
		char* hdr_name= NULL;
		
		memset(buf, 0, 128* sizeof(char));
		match_header= buf;
		
		hdr_name= (match_type==IF_MATCH)?"If-Match":"If-None-Match"; 
		
		len=sprintf(match_header, "%s: %s\n", hdr_name, match_etag);
		
		match_header[len]= '\0';	
	}

	curl_handle = curl_easy_init();
	
	curl_easy_setopt(curl_handle, CURLOPT_URL, path);
	
	curl_easy_setopt(curl_handle, CURLOPT_PORT, xcap_port);

	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);	

	curl_easy_setopt(curl_handle,  CURLOPT_STDERR, stdout);	
	
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_function);
	
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &stream);

	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, get_xcap_etag);
	
	curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, &etag);

	if(match_header)
		curl_easy_setopt(curl_handle, CURLOPT_HEADER, (long)match_header);

	/* non-2xx => error */
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);

	ret_code= curl_easy_perform(curl_handle );
	
	if( ret_code== CURLE_WRITE_ERROR)
	{
		LM_ERR("while performing curl option\n");
		if(stream)
			pkg_free(stream);
		stream= NULL;
		return NULL;
	}

	curl_global_cleanup();
	return stream;
}

size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream)
{
	/* allocate memory and copy */
	char* data;

	data= (char*)pkg_malloc(size* nmemb);
	if(data== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	memcpy(data, (char*)ptr, size* nmemb);
	
	*((char**) stream)= data;

	return size* nmemb;

error:
	return CURLE_WRITE_ERROR;
}
