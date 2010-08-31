/*
 * $Id$
 *
 * xcap_server module - builtin XCAP server
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../parser/parse_param.h"
#include "../../modules_k/xcap_client/xcap_callbacks.h"

#include "xcap_misc.h"

static param_t *_xcaps_xpath_ns_root = NULL;

int xcap_parse_uri(str *huri, str *xroot, xcap_uri_t *xuri)
{
	str s;
	char *p;
	int i;

	if(huri==NULL || xuri==NULL || huri->s==NULL)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(huri->len>XCAP_MAX_URI_SIZE)
	{
		LM_ERR("http uri too long\n");
		return -1;
	}

	memset(xuri, 0, sizeof(xcap_uri_t));

	/* copy and url decode */
	for(p=huri->s, i=0; p<huri->s+huri->len; p++)
	{
		if(*p=='%') {
			if(p > huri->s+huri->len-2)
			{
				LM_ERR("invalid http uri - hexa value too short\n");
				return -1;
			}
			p++;
			if(*p>='0' && *p<='9')
				xuri->buf[i] = (*p - '0') << 4;
			else if(*p>='a'&&*p<='f')
				xuri->buf[i] = (*p-'a'+10) << 4;
			else if(*p>='A'&&*p<='F')
				xuri->buf[i] = (*p-'A'+10) << 4;
			else return -1;
			p++;
			if(*p>='0'&&*p<='9')
				xuri->buf[i] += *p-'0';
			else if(*p>='a'&&*p<='f')
				xuri->buf[i] += *p-'a'+10;
			else if(*p>='A'&&*p<='F')
				xuri->buf[i] += *p-'A'+10;
			else return -1;
		} else {
			xuri->buf[i] = *p;
		}
		i++;
	}
	xuri->uri.s = xuri->buf;
	xuri->uri.len = i;
	xuri->uri.s[xuri->uri.len] = '\0';

	xuri->nss = strstr(xuri->uri.s, XCAP_NSS);

	xuri->root.s = xuri->uri.s;
	s = xuri->uri;

	if(xroot!=NULL)
	{
		if(xroot->len >= xuri->uri.len)
		{
			LM_ERR("invalid http uri - shorter than xcap-root\n");
			return -1;
		}
		if(strncmp(xuri->uri.s, xroot->s, xroot->len)!=0)
		{
			LM_ERR("missing xcap-root in [%.*s]\n", xuri->uri.len,
					xuri->uri.s);
			return -1;
		}

		s.s   = xuri->uri.s + xroot->len;
		s.len = xuri->uri.len - xroot->len;
		xuri->root.len = xroot->len;
	}
	if(*s.s == '/')
	{
		 s.s++;
		 s.len--;
	}

	/* auid */
	xuri->auid.s = s.s;
	if(s.len>11 && strncmp(s.s, "pres-rules/", 11)==0)
	{
		LM_DBG("matched pres-rules\n");
		xuri->type = PRES_RULES;
		xuri->auid.len = 10;
	} else if(s.len>13 && strncmp(s.s, "rls-services/", 13)==0) {
		LM_DBG("matched rls-services\n");
		xuri->type = RLS_SERVICE;
		xuri->auid.len = 12;
	} else if(s.len>18 && strncmp(s.s, "pidf-manipulation/", 18)==0) {
		LM_DBG("matched pidf-manipulation\n");
		xuri->type = PIDF_MANIPULATION;
		xuri->auid.len = 17;
	} else if(s.len>15 && strncmp(s.s, "resource-lists/", 15)==0) {
		LM_DBG("matched resource-lists\n");
		xuri->type = RESOURCE_LIST;
		xuri->auid.len = 14;
	} else {
		LM_ERR("unsupported auid in [%.*s]\n", xuri->uri.len,
				xuri->uri.s);
		return -1;
	}

	s.s   += xuri->auid.len + 1;
	s.len -= xuri->auid.len + 1;

	/* tree: users or global */
	xuri->tree.s = s.s;
	if(s.len>6 && strncmp(s.s, "users/", 6)==0) {
		LM_DBG("matched users\n");
		xuri->tree.len = 5;
	} else if(s.len>7 && strncmp(s.s, "global/", 7)==0) {
		LM_DBG("matched global\n");
		xuri->tree.len = 6;
	} else {
		LM_ERR("unsupported sub-tree in [%.*s]\n", xuri->uri.len,
				xuri->uri.s);
		return -1;
	}

	s.s   += xuri->tree.len + 1;
	s.len -= xuri->tree.len + 1;

	/* xuid */
	if(xuri->tree.s[0]=='u') {
		xuri->xuid.s = s.s;
		p = strchr(s.s, '/');
		if(p==NULL) {
			LM_ERR("no xuid in [%.*s]\n", xuri->uri.len,
					xuri->uri.s);
			return -1;
		}
		xuri->xuid.len = p - xuri->xuid.s;

		s.s   += xuri->xuid.len + 1;
		s.len -= xuri->xuid.len + 1;
	}

	/* file */
	xuri->file.s = s.s;
	if(xuri->nss==NULL) {
		/* without node selector in http uri */
		if(s.s[s.len-1]=='/')
			xuri->file.len = s.len - 1;
		else
			xuri->file.len = s.len;
	} else {
		/* with node selector in http uri */
		if(xuri->nss <= s.s) {
			LM_ERR("no file in [%.*s]\n", xuri->uri.len,
				xuri->uri.s);
			return -1;
		}
		xuri->file.len = xuri->nss - s.s;
		if(xuri->file.s[xuri->file.len-1]=='/')
			xuri->file.len--;
	}
	/* doc: aboslute and relative */
	xuri->adoc.s   = xuri->uri.s;
	xuri->adoc.len = xuri->file.s + xuri->file.len - xuri->adoc.s;
	xuri->rdoc.s   = xuri->auid.s;
	xuri->rdoc.len = xuri->file.s + xuri->file.len - xuri->rdoc.s;

	/* node */
	if(xuri->nss!=NULL) {
		xuri->node.s   = xuri->nss + 2;
		xuri->node.len = xuri->uri.s + xuri->uri.len - xuri->node.s;
	}

	LM_DBG("----- uri: [%.*s]\n", xuri->uri.len, xuri->uri.s);
	LM_DBG("----- root: [%.*s]\n", xuri->root.len, xuri->root.s);
	LM_DBG("----- auid: [%.*s] (%d)\n", xuri->auid.len, xuri->auid.s,
			xuri->type);
	LM_DBG("----- tree: [%.*s]\n", xuri->tree.len, xuri->tree.s);
	if(xuri->tree.s[0]=='u')
		LM_DBG("----- xuid: [%.*s]\n", xuri->xuid.len, xuri->xuid.s);
	LM_DBG("----- file: [%.*s]\n", xuri->file.len, xuri->file.s);
	LM_DBG("----- adoc: [%.*s]\n", xuri->adoc.len, xuri->adoc.s);
	LM_DBG("----- rdoc: [%.*s]\n", xuri->rdoc.len, xuri->rdoc.s);
	if(xuri->nss!=NULL)
		LM_DBG("----- node: [%.*s]\n", xuri->node.len, xuri->node.s);
	return 0;
}

int xcaps_xpath_get(str *inbuf, str *xpaths, str *outbuf)
{
	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL; 
	xmlXPathObjectPtr xpathObj = NULL; 
	xmlNodeSetPtr nodes;
	xmlChar *keyword;
	xmlBufferPtr psBuf;
	int size;
	int i;
	char *p;
	char *end;
	char *pos;

	doc = xmlParseMemory(inbuf->s, inbuf->len);
	if(doc == NULL)
		return -1;

	xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL)
	{
		LM_ERR("unable to create new XPath context\n");
		goto error;
	}
	
	/* Evaluate xpath expression */
	// xcaps_xpath_register_ns(xpathCtx);
	xpathObj = xmlXPathEvalExpression(
					(const xmlChar*)xpaths->s, xpathCtx);
	if(xpathObj == NULL)
	{
		LM_ERR("unable to evaluate xpath expression [%s]\n", xpaths->s);
		goto error;
	}
    nodes = xpathObj->nodesetval;
	if(nodes==NULL)
	{
		outbuf->len = 0;
		outbuf->s[outbuf->len] = '\0';
		goto done;
	}
	size = nodes->nodeNr;
    p = outbuf->s;
	end = outbuf->s + outbuf->len;
	for(i = 0; i < size; ++i)
	{
		if(nodes->nodeTab[i]==NULL)
			continue;
		if(i!=0)
		{
			if(p>=end)
			{
				LM_ERR("output buffer overflow\n");
				goto error;
			}
			*p = ',';
			p++;
		}
		if(nodes->nodeTab[i]->type == XML_ATTRIBUTE_NODE)
		{
			keyword = xmlNodeListGetString(doc,
				nodes->nodeTab[i]->children, 0);
			if(keyword != NULL)
			{
				pos = p + strlen((char*)keyword);
				if(pos>=end)
				{
					LM_ERR("output buffer overflow\n");
					goto error;
				}
				strcpy(p, (char*)keyword);
				p = pos;
				xmlFree(keyword);
				keyword = NULL;
			}
		} else {
			if(nodes->nodeTab[i]->content!=NULL)
			{
				pos = p + strlen((char*)nodes->nodeTab[i]->content);
				if(pos>=end)
				{
					LM_ERR("output buffer overflow\n");
					goto error;
				}
				strcpy(p, (char*)nodes->nodeTab[i]->content);
				p = pos;
			} else {
				psBuf = xmlBufferCreate();
				if(psBuf != NULL && xmlNodeDump(psBuf, doc,
						nodes->nodeTab[i], 0, 0)>0)
				{
					pos = p + strlen((char*)xmlBufferContent(psBuf));
					if(pos>=end)
					{
						LM_ERR("output buffer overflow\n");
						goto error;
					}
					strcpy(p, (char*)xmlBufferContent(psBuf));
					p = pos;
				}
				if(psBuf != NULL) xmlBufferFree(psBuf);
				psBuf = NULL;
			}
		}
	}
	outbuf->len = p - outbuf->s;
	outbuf->s[outbuf->len] = '\0';

done:
	if(xpathObj!=NULL) xmlXPathFreeObject(xpathObj);
	if(xpathCtx!=NULL) xmlXPathFreeContext(xpathCtx); 
	if(doc!=NULL) xmlFreeDoc(doc);
	xpathObj = NULL;
	xpathCtx = NULL; 
	doc = NULL; 
	return 0;

error:
	if(xpathObj!=NULL) xmlXPathFreeObject(xpathObj);
	if(xpathCtx!=NULL) xmlXPathFreeContext(xpathCtx); 
	if(doc!=NULL) xmlFreeDoc(doc);
	xpathObj = NULL;
	xpathCtx = NULL; 
	doc = NULL; 
	outbuf->len = 0;
	outbuf->s[outbuf->len] = '\0';
	return -1;
}


int xcaps_xpath_set(str *inbuf, str *xpaths, str *val, str *outbuf)
{
	xmlDocPtr doc = NULL;
	xmlDocPtr newnode = NULL;
	xmlXPathContextPtr xpathCtx = NULL; 
	xmlXPathObjectPtr xpathObj = NULL; 
	xmlNodeSetPtr nodes;
	const xmlChar* value = NULL;
	xmlChar *xmem = NULL;
	xmlNodePtr parent = NULL;
	int size;
	int i;

	doc = xmlParseMemory(inbuf->s, inbuf->len);
	if(doc == NULL)
		return -1;

	if(val!=NULL)
		newnode = xmlParseMemory(val->s, val->len);

	outbuf->s   = NULL;
	outbuf->len = 0;

	xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL)
	{
		LM_ERR("unable to create new XPath context\n");
		goto error;
	}
	
	/* Evaluate xpath expression */
	xpathObj = xmlXPathEvalExpression(
					(const xmlChar*)xpaths->s, xpathCtx);
	if(xpathObj == NULL)
	{
		LM_ERR("unable to evaluate xpath expression [%s]\n", xpaths->s);
		goto error;
	}
    nodes = xpathObj->nodesetval;
	if(nodes==NULL)
	{
		LM_DBG("no selection for xpath expression [%s]\n", xpaths->s);
		goto done;
	}
	size = nodes->nodeNr;
	if(val!=NULL)
		value = (const xmlChar*)val->s;
    
	/*
	 * NOTE: the nodes are processed in reverse order, i.e. reverse document
	 *       order because xmlNodeSetContent can actually free up descendant
	 *       of the node and such nodes may have been selected too ! Handling
	 *       in reverse order ensure that descendant are accessed first, before
	 *       they get removed. Mixing XPath and modifications on a tree must be
	 *       done carefully !
	 */
	for(i = size - 1; i >= 0; i--) {
		if(nodes->nodeTab[i]==NULL)
			continue;
	
		if(nodes->nodeTab[i]->type==XML_ELEMENT_NODE)
		{
			parent = nodes->nodeTab[i]->parent;
			xmlUnlinkNode(nodes->nodeTab[i]);
			if(val!=NULL && newnode!=NULL)
				xmlAddChild(parent, xmlCopyNode(newnode->children, 1));
		} else {
			if(val!=NULL)
				xmlNodeSetContent(nodes->nodeTab[i], value);
			else
				xmlNodeSetContent(nodes->nodeTab[i], (const xmlChar*)"");
		}
		/*
		 * All the elements returned by an XPath query are pointers to
		 * elements from the tree *except* namespace nodes where the XPath
		 * semantic is different from the implementation in libxml2 tree.
		 * As a result when a returned node set is freed when
		 * xmlXPathFreeObject() is called, that routine must check the
		 * element type. But node from the returned set may have been removed
		 * by xmlNodeSetContent() resulting in access to freed data.
		 * This can be exercised by running
		 *       valgrind xpath2 test3.xml '//discarded' discarded
		 * There is 2 ways around it:
		 *   - make a copy of the pointers to the nodes from the result set 
		 *     then call xmlXPathFreeObject() and then modify the nodes
		 * or
		 *   - remove the reference to the modified nodes from the node set
		 *     as they are processed, if they are not namespace nodes.
		 */
		if (nodes->nodeTab[i]->type != XML_NAMESPACE_DECL)
			nodes->nodeTab[i] = NULL;
	}

	xmlDocDumpMemory(doc, &xmem, &size);
	if(xmem==NULL)
	{
		LM_ERR("error printing output\n");
		goto error;
	}

	if(size<=0)
	{
		LM_ERR("invalid output size\n");
		xmlFree(xmem);
		goto error;
	}
	outbuf->s = (char*)pkg_malloc(size+1);
	if(size<=0)
	{
		LM_ERR("no pkg for output\n");
		xmlFree(xmem);
		goto error;
	}
	memcpy(outbuf->s, xmem, size);
	outbuf->s[size] = '\0';
	outbuf->len = size;
	xmlFree(xmem);

done:
	if(xpathObj!=NULL) xmlXPathFreeObject(xpathObj);
	if(xpathCtx!=NULL) xmlXPathFreeContext(xpathCtx); 
	if(doc!=NULL) xmlFreeDoc(doc);
	if(newnode!=NULL) xmlFreeDoc(newnode);
	xpathObj = NULL;
	xpathCtx = NULL; 
	doc = NULL; 
	return 0;

error:
	if(xpathObj!=NULL) xmlXPathFreeObject(xpathObj);
	if(xpathCtx!=NULL) xmlXPathFreeContext(xpathCtx); 
	if(doc!=NULL) xmlFreeDoc(doc);
	if(newnode!=NULL) xmlFreeDoc(newnode);
	xpathObj = NULL;
	xpathCtx = NULL; 
	doc = NULL; 
	outbuf->s =   NULL;
	outbuf->len = 0;
	return -1;
}


void xcaps_xpath_register_ns(xmlXPathContextPtr xpathCtx)
{
	param_t *ns;
	ns = _xcaps_xpath_ns_root;
	while(ns) {
		xmlXPathRegisterNs(xpathCtx, (xmlChar*)ns->name.s,
				(xmlChar*)ns->body.s);
		ns = ns->next;
	}
}

int xcaps_xpath_ns_param(modparam_t type, void *val)
{
	char *p;
	param_t *ns;

	if(val==NULL)
		goto error;
	ns = (param_t*)pkg_malloc(sizeof(param_t));

	if(ns==NULL)
	{
		LM_ERR("no more pkg\n");
		goto error;
	}
	memset(ns, 0, sizeof(param_t));

	p = strchr((const char*)val, '=');
	if(p==NULL)
	{
		ns->name.s = "";
		ns->body.s = (char*)val;
		ns->body.len = strlen(ns->body.s);
	} else {
		*p = 0;
		p++;
		ns->name.s = (char*)val;
		ns->name.len = strlen(ns->name.s);
		ns->body.s = p;
		ns->body.len = strlen(ns->body.s);
	}
	ns->next = _xcaps_xpath_ns_root;
	_xcaps_xpath_ns_root = ns;
	return 0;
error:
	return -1;

}

