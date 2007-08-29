/*
 * $Id: xcap_functions.c 2230 2007-06-06 07:13:20Z anca_vamanu $
 *
 * xcap_client module - XCAP client for openser
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-08-20  initial version (anca)
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
#include "xcap_functions.h"
#include "xcap_client.h"

size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream);

int bind_xcap(xcap_api_t* api)
{
	if (!api) {
		LOG(L_ERR, "NOTIFIER:bind_notifier: Invalid parameter value\n");
		return -1;
	}
	api->get_elem= xcapGetElem;
	api->int_node_sel= xcapInitNodeSel;
	api->add_step= xcapNodeSelAddStep;
	api->add_terminal= xcapNodeSelAddTerminal;
	api->free_node_sel= xcapFreeNodeSel;
	api->register_xcb= register_xcapcb;

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
		pkg_free(n->value.s);
		pkg_free(n);
	}

	pkg_free(node);

}

xcap_node_sel_t* xcapInitNodeSel(void)
{
	xcap_node_sel_t* nsel= NULL;

	nsel= (xcap_node_sel_t*)pkg_malloc(sizeof(xcap_node_sel_t));
	if(nsel== NULL)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapInitNodeSel: ERROR No more memory\n");
		goto error;
	}
	memset(nsel, 0, sizeof(xcap_node_sel_t));
	nsel->steps= (step_t*)pkg_malloc(sizeof(step_t));
	if(nsel->steps== NULL)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapInitNodeSel: ERROR No more memory\n");
		goto error;
	}
	memset(nsel->steps, 0, sizeof(step_t));
	nsel->last_step= nsel->steps;

	nsel->ns_list= (ns_list_t*)pkg_malloc(sizeof(ns_list_t));
	if(nsel->ns_list== NULL)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapInitNodeSel: ERROR No more memory\n");
		goto error;
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
		LOG(L_ERR, "XCAP_CLIENT: xcapNodeSelAddStep: ERROR No more memory\n");
		goto error;
	}
	if(name)
	{
		if(namespace)
		{
			ns_card= curr_sel->ns_no+ 'a';
			curr_sel->ns_no++;

			if(ns_card> 'z')
			{
				LOG(L_ERR, "XCAP_CLIENT: xcapNodeSelAddStep: ERROR Insuficient name cards"
						" for namespaces\n");
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
		LOG(L_ERR, "XCAP_CLIENT: xcapNodeSelAddStep: ERROR No more memory\n");
		goto error;
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
			LOG(L_ERR, "XCAP_CLIENT: xcapNodeSelAddStep: ERROR No more memory");
			goto error;
		}
		ns->name= ns_card;
		ns->value.s= (char*)pkg_malloc(namespace->len* sizeof(char));
		if(ns->value.s== NULL)
		{
			LOG(L_ERR, "XCAP_CLIENT: xcapNodeSelAddStep: ERROR No more memory");
			goto error;
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
		LOG(L_ERR, "XCAP_client: get_node_selector: ERROR No more memory\n");
		return NULL;
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

	return NULL;
}

/* for testing purposes only
 * function to write data at the xcap server */
int xcapWriteData(char* path, char* file_name)
{
	CURL* curl_handle= NULL;
	FILE* f= NULL;

	f= fopen(file_name, "rt");
	if(f== NULL)
	{
		LOG(L_ERR, "XCAP_client: xcapWriteData: ERROR while opening"
				" file %s for reading\n", file_name);
		return -1;
	}
	curl_handle = curl_easy_init();

	curl_easy_setopt(curl_handle, CURLOPT_URL, path);
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);	
	curl_easy_setopt(curl_handle,  CURLOPT_STDERR, stdout);	
	
	/* enable uploading */
	curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1) ;

	/* HTTP PUT please */
	curl_easy_setopt(curl_handle, CURLOPT_PUT, 1);

//	curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, read_function);
	
	curl_easy_setopt(curl_handle, CURLOPT_READDATA, f);
	
	curl_easy_perform(curl_handle);
	
	curl_global_cleanup();

	return 0;
}

/* xcap_root must be a NULL terminated string */

char* xcapGetElem(char* xcap_root, xcap_doc_sel_t* doc_sel, xcap_node_sel_t* node_sel)
{
	int len= 0, size;
	char* path= NULL;
	char* node_selector= NULL;
	char* stream= NULL;
		
	len= (strlen(xcap_root)+ 1+ doc_sel->auid.len+ 5+ doc_sel->xid.len+
		doc_sel->filename.len+ 50)* sizeof(char);
	
	if(node_sel)
		len+= node_sel->size;

	path= (char*)pkg_malloc(len);
	if(path== NULL)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapGetElem: ERROR NO more memory\n");
		goto error;
	}

	if(node_sel)
	{
		node_selector= get_node_selector(node_sel);
		if(node_selector== NULL)
		{
			LOG(L_ERR, "XCAP_CLIENT: xcapGetElem:ERROR while constructing"
					" node selector\n");
			goto error;
		}
	}
	
	size= sprintf(path, "%s/%.*s/", xcap_root,doc_sel->auid.len,doc_sel->auid.s);

	if(doc_sel->type==USERS_TYPE)
		size+= sprintf(path+ size, "%s/%.*s/", "users", doc_sel->xid.len, doc_sel->xid.s);
	else
		size+= sprintf(path+ size, "%s/", "global");
	size+= sprintf(path+ size, "%.*s", doc_sel->filename.len, doc_sel->filename.s);
	
	if(node_selector)
	{
		size+= sprintf(path+ size, "/~~%s", node_selector);
	}

	if(size> len)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapGetElem:ERROR buffer size overflow\n");
		goto error;
	}

	stream= send_http_get(path);
	if(stream== NULL)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapGetElem:ERROR sending xcap get request\n");
		goto error;
	}

error:		
	if(path)
		pkg_free(path);
	
	return stream;
}

char* send_http_get(char* path)
{
	char* stream= NULL;
	CURLcode ret_code;
	CURL* curl_handle= NULL;

	curl_handle = curl_easy_init();
	
	curl_easy_setopt(curl_handle, CURLOPT_URL, path);

	//	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);	

	curl_easy_setopt(curl_handle,  CURLOPT_STDERR, stdout);	
	
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_function);
	
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &stream);

	/* non-2xx => error */
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);

	ret_code= curl_easy_perform(curl_handle );
	
	if( ret_code== CURLE_WRITE_ERROR)
	{
		LOG(L_ERR, "XCAP_CLIENT: xcapGetElem:ERROR while performing"
				" curl option\n");
		if(stream)
			pkg_free(stream);
		stream= NULL;
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
		LOG(L_ERR, "XCAP_client: write_function: ERROR No more memory\n");
		return CURLE_WRITE_ERROR;
	}

	memcpy(data, (char*)ptr, size* nmemb);
	
	*((char**) stream)= data;

	return size* nmemb;
}
