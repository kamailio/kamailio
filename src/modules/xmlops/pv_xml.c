/**
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "../../mem/mem.h"
#include "../../parser/parse_param.h"
#include "../../hashes.h"
#include "../../dprint.h"

#include "pv_xml.h"

int pv_xml_buf_size = 4095;

typedef struct _pv_xml {
	str docname;
	unsigned int docid;
	str inbuf;
	str outbuf;
	int updated;
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx; 
    xmlXPathObjectPtr xpathObj; 
	struct _pv_xml *next;
} pv_xml_t;

typedef struct _pv_xml_spec {
	str docname;
	pv_xml_t *xdoc;
	int type;
	pv_elem_t *pve;
} pv_xml_spec_t;

pv_xml_t *_pv_xml_root = NULL;

param_t *_pv_xml_ns_root = NULL;

pv_xml_t *pv_xml_get_struct(str *name)
{
	unsigned int docid;
	pv_xml_t *it;

	docid = get_hash1_raw(name->s, name->len);
	it = _pv_xml_root;

	while(it!=NULL)
	{
		if(docid == it->docid && name->len==it->docname.len 
				&& strncmp(name->s, it->docname.s, name->len)==0)
		{
			LM_DBG("doc found [%.*s]\n", name->len, name->s);
			return it;
		}
		it = it->next;
	}

	it = (pv_xml_t*)pkg_malloc(sizeof(pv_xml_t)+2*(pv_xml_buf_size+1));
	if(it==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(it, 0, sizeof(pv_xml_t)+2*(pv_xml_buf_size+1));

	it->docid = docid;
	it->docname = *name;
	it->inbuf.s = (char*)it + sizeof(pv_xml_t);
	it->outbuf.s = it->inbuf.s + pv_xml_buf_size+1;

	it->next = _pv_xml_root;
	_pv_xml_root = it;
	return it;
}

int pv_xpath_nodes_eval(pv_xml_t *xdoc)
{
    int size;
    int i;
	xmlNodeSetPtr nodes;
	char *p;
	xmlChar *keyword;
	xmlBufferPtr psBuf;

	if(xdoc==NULL || xdoc->doc==NULL || xdoc->xpathCtx==NULL
			|| xdoc->xpathObj==NULL)
		return -1;

    nodes = xdoc->xpathObj->nodesetval;
	if(nodes==NULL)
	{
		xdoc->outbuf.len = 0;
		xdoc->outbuf.s[xdoc->outbuf.len] = '\0';
		return 0;
	}
	size = nodes->nodeNr;
    p = xdoc->outbuf.s;
	for(i = 0; i < size; ++i)
	{
		if(nodes->nodeTab[i]==NULL)
			continue;
		if(i!=0)
		{
			*p = ',';
			p++;
		}
		if(nodes->nodeTab[i]->type == XML_ATTRIBUTE_NODE)
		{
			keyword = xmlNodeListGetString(xdoc->doc,
				nodes->nodeTab[i]->children, 0);
			if(keyword != NULL)
			{
				strcpy(p, (char*)keyword);
				p += strlen((char*)keyword);
				xmlFree(keyword);
				keyword = NULL;
			}
		} else {
			if(nodes->nodeTab[i]->content!=NULL)
			{
				strcpy(p, (char*)nodes->nodeTab[i]->content);
				p += strlen((char*)nodes->nodeTab[i]->content);
			} else {
				psBuf = xmlBufferCreate();
				if(psBuf != NULL && xmlNodeDump(psBuf, xdoc->doc,
						nodes->nodeTab[i], 0, 0)>0)
				{
					strcpy(p, (char*)xmlBufferContent(psBuf));
					p += strlen((char*)xmlBufferContent(psBuf));
				}
				if(psBuf != NULL) xmlBufferFree(psBuf);
				psBuf = NULL;
			}
		}
	}
	xdoc->outbuf.len = p - xdoc->outbuf.s;
	xdoc->outbuf.s[xdoc->outbuf.len] = '\0';
	return 0;
}

int pv_xpath_nodes_update(pv_xml_t *xdoc, str *val)
{
	xmlNodeSetPtr nodes;
	const xmlChar* value;
    int size;
    int i;
    
	if(xdoc==NULL || xdoc->doc==NULL || xdoc->xpathCtx==NULL
			|| xdoc->xpathObj==NULL || val==NULL)
		return -1;
	if(val->len>pv_xml_buf_size)
	{
		LM_ERR("internal buffer overflow - %d\n", val->len);
		return -1;
	}
    nodes = xdoc->xpathObj->nodesetval;
	if(nodes==NULL)
		return 0;
    size = nodes->nodeNr;

	value = (const xmlChar*)xdoc->outbuf.s;
	memcpy(xdoc->outbuf.s, val->s, val->len);
	xdoc->outbuf.s[val->len] = '\0';
	xdoc->outbuf.len = val->len;
    
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
	
		xmlNodeSetContent(nodes->nodeTab[i], value);
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
	xdoc->outbuf.s[0] = '\0';
	xdoc->outbuf.len = 0;
	return 0;
}

void pv_xml_register_ns(xmlXPathContextPtr xpathCtx)
{
	param_t *ns;
	ns = _pv_xml_ns_root;
	while(ns) {
		xmlXPathRegisterNs(xpathCtx, (xmlChar*)ns->name.s,
				(xmlChar*)ns->body.s);
		ns = ns->next;
	}
}

int pv_get_xml(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	pv_xml_spec_t *pxs = NULL;
	str xpaths;
	int size = 0;
	xmlChar *xmem = NULL;

	pxs = (pv_xml_spec_t*)param->pvn.u.dname;
	if(pxs->xdoc==NULL)
		return -1;

	switch(pxs->type) {
		case 0:
			/* get document */
			if(pxs->xdoc->inbuf.len<=0)
				return pv_get_null(msg, param, res);
			if(pxs->xdoc->doc == NULL || pxs->xdoc->updated == 0)
				return pv_get_strval(msg, param, res, &pxs->xdoc->inbuf);
			xmlDocDumpMemory(pxs->xdoc->doc, &xmem, &size);
			if(xmem!=NULL)
			{
				if(size>pv_xml_buf_size)
				{
					xmlFree(xmem);
					return pv_get_null(msg, param, res);
				}
				memcpy(pxs->xdoc->outbuf.s, xmem, size);
				pxs->xdoc->outbuf.s[size] = '\0';
				pxs->xdoc->outbuf.len = size;
				xmlFree(xmem);
				return pv_get_strval(msg, param, res, &pxs->xdoc->outbuf);
			}
			return pv_get_null(msg, param, res);
		break;
		case 1:
			/* get xpath element */
			if(pxs->xdoc->doc == NULL)
			{
				if(pxs->xdoc->inbuf.len<=0)
					return pv_get_null(msg, param, res);
				pxs->xdoc->doc = xmlParseMemory(pxs->xdoc->inbuf.s,
						pxs->xdoc->inbuf.len);
				if(pxs->xdoc->doc == NULL)
					return pv_get_null(msg, param, res);
			}
			if(pxs->xdoc->xpathCtx == NULL)
			{
				pxs->xdoc->xpathCtx = xmlXPathNewContext(pxs->xdoc->doc);
				if(pxs->xdoc->xpathCtx == NULL)
				{
					LM_ERR("unable to create new XPath context\n");
					xmlFreeDoc(pxs->xdoc->doc);
					pxs->xdoc->doc = NULL;
					return pv_get_null(msg, param, res);
				}
			}
			if(pv_printf_s(msg, pxs->pve, &xpaths)!=0)
			{
				LM_ERR("cannot get xpath string\n");
				return pv_get_null(msg, param, res);
			}
			
			/* Evaluate xpath expression */
			pv_xml_register_ns(pxs->xdoc->xpathCtx);
			pxs->xdoc->xpathObj = xmlXPathEvalExpression(
					(const xmlChar*)xpaths.s, pxs->xdoc->xpathCtx);
			if(pxs->xdoc->xpathObj == NULL)
			{
				LM_ERR("unable to evaluate xpath expression [%s/%d]\n",
						xpaths.s, xpaths.len);
				xmlXPathFreeContext(pxs->xdoc->xpathCtx); 
				xmlFreeDoc(pxs->xdoc->doc); 
				pxs->xdoc->xpathCtx = NULL; 
				pxs->xdoc->doc = NULL; 
				return pv_get_null(msg, param, res);
			}
			/* Print results */
			if(pv_xpath_nodes_eval(pxs->xdoc)<0)
			{
				xmlXPathFreeObject(pxs->xdoc->xpathObj);
				xmlXPathFreeContext(pxs->xdoc->xpathCtx); 
				xmlFreeDoc(pxs->xdoc->doc);
				pxs->xdoc->xpathObj = NULL;
				pxs->xdoc->xpathCtx = NULL; 
				pxs->xdoc->doc = NULL; 
				return pv_get_null(msg, param, res);
			}
			xmlXPathFreeObject(pxs->xdoc->xpathObj);
			pxs->xdoc->xpathObj = NULL;
			return pv_get_strval(msg, param, res, &pxs->xdoc->outbuf);
		break;
		default:
			return pv_get_null(msg, param, res);
	}
	return pv_get_null(msg, param, res);
}

int pv_set_xml(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	pv_xml_spec_t *pxs = NULL;
	str xpaths;

	pxs = (pv_xml_spec_t*)param->pvn.u.dname;
	if(pxs->xdoc==NULL)
		return -1;
	if(!(val->flags&PV_VAL_STR))
		return -1;

	switch(pxs->type) {
		case 0:
			/* set document */
			if(pxs->xdoc->doc!=NULL)
			{
				if(pxs->xdoc->xpathCtx!=NULL)
				{
					xmlXPathFreeContext(pxs->xdoc->xpathCtx);
					pxs->xdoc->xpathCtx = NULL;
				}
				xmlFreeDoc(pxs->xdoc->doc);
				pxs->xdoc->doc = NULL;
			}
			if(val->rs.len>pv_xml_buf_size)
			{
				LM_ERR("local buffer overflow - %d\n", val->rs.len);
				return -1;
			}
			memcpy(pxs->xdoc->inbuf.s, val->rs.s, val->rs.len);
			pxs->xdoc->inbuf.s[val->rs.len] = '\0';
			pxs->xdoc->inbuf.len = val->rs.len;
			pxs->xdoc->updated = 0;
			return 0;
		break;
		case 1:
			/* set xpath element */
			if(pxs->xdoc->doc == NULL)
			{
				if(pxs->xdoc->inbuf.len<=0)
					return -1;
				pxs->xdoc->doc = xmlParseMemory(pxs->xdoc->inbuf.s,
						pxs->xdoc->inbuf.len);
				if(pxs->xdoc->doc == NULL)
					return -1;
			}
			if(pxs->xdoc->xpathCtx == NULL)
			{
				pxs->xdoc->xpathCtx = xmlXPathNewContext(pxs->xdoc->doc);
				if(pxs->xdoc->xpathCtx == NULL)
				{
					LM_ERR("unable to create new XPath context\n");
					xmlFreeDoc(pxs->xdoc->doc);
					pxs->xdoc->doc = NULL;
					return -1;
				}
			}
			if(pv_printf_s(msg, pxs->pve, &xpaths)!=0)
			{
				LM_ERR("cannot get xpath string\n");
				return -1;
			}
			
			/* Evaluate xpath expression */
			pxs->xdoc->xpathObj = xmlXPathEvalExpression(
					(const xmlChar*)xpaths.s, pxs->xdoc->xpathCtx);
			if(pxs->xdoc->xpathObj == NULL)
			{
				LM_ERR("unable to evaluate xpath expression [%s]\n", xpaths.s);
				xmlXPathFreeContext(pxs->xdoc->xpathCtx); 
				xmlFreeDoc(pxs->xdoc->doc); 
				pxs->xdoc->xpathCtx = NULL; 
				pxs->xdoc->doc = NULL; 
				return -1;
			}
			/* Set value */
			if(pv_xpath_nodes_update(pxs->xdoc, &val->rs)<0)
			{
				LM_ERR("unable to update xpath [%s] - [%.*s]\n", xpaths.s,
						val->rs.len, val->rs.s);
				xmlXPathFreeObject(pxs->xdoc->xpathObj);
				xmlXPathFreeContext(pxs->xdoc->xpathCtx); 
				xmlFreeDoc(pxs->xdoc->doc); 
				pxs->xdoc->xpathObj = NULL;
				pxs->xdoc->xpathCtx = NULL; 
				pxs->xdoc->doc = NULL; 
				return -1;
			}
			pxs->xdoc->updated = 1;
			xmlXPathFreeObject(pxs->xdoc->xpathObj);
			pxs->xdoc->xpathObj = NULL;
			return 0;
		break;
		default:
			return -1;
	}

	return 0;
}

int pv_parse_xml_name(pv_spec_p sp, str *in)
{
	pv_xml_spec_t *pxs = NULL;
	char *p;
	str pvs;

	if(in->s==NULL || in->len<=0)
		return -1;

	pxs = (pv_xml_spec_t*)pkg_malloc(sizeof(pv_xml_spec_t));
	if(pxs==NULL)
		return -1;

	memset(pxs, 0, sizeof(pv_xml_spec_t));

	p = in->s;

	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		goto error;
	pxs->docname.s = p;
	while(p < in->s + in->len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}
	if(p>in->s+in->len || *p=='\0')
		goto error;
	pxs->docname.len = p - pxs->docname.s;
	if(*p!='=')
	{
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!='=')
			goto error;
	}
	p++;
	if(*p!='>')
		goto error;
	p++;

	pvs.len = in->len - (int)(p - in->s);
	pvs.s = p;
	LM_DBG("xmldoc [%.*s] - key [%.*s]\n", pxs->docname.len, pxs->docname.s,
			pvs.len, pvs.s);
	if(pvs.len>=3 && strncmp(pvs.s, "doc", 3)==0) {
		pxs->type = 0;
	} else if(pvs.len>6 && strncmp(pvs.s, "xpath:", 6)==0) {
		pvs.s += 6;
		pvs.len -= 6;
		pxs->type = 1;
		LM_DBG("*** xpath expr [%.*s]\n", pvs.len, pvs.s);
		if(pv_parse_format(&pvs, &pxs->pve)<0 || pxs->pve==NULL)
		{
			LM_ERR("wrong xpath format [%.*s]\n", in->len, in->s);
			goto error;
		}
	} else {
		LM_ERR("unknown key type [%.*s]\n", in->len, in->s);
		goto error;
	}
	pxs->xdoc = pv_xml_get_struct(&pxs->docname);
	sp->pvp.pvn.u.dname = (void*)pxs;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;

error:
	if(pxs!=NULL)
		pkg_free(pxs);
	return -1;
}

int pv_xml_ns_param(modparam_t type, void *val)
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
	ns->next = _pv_xml_ns_root;
	_pv_xml_ns_root = ns;
	return 0;
error:
	return -1;

}

