/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "config.h"


#include <libxml/parser.h>
#include <stdio.h>
#include <string.h>

extern int errno;
	
static xmlValidCtxt	cvp;	/**< XML Validation context */

/**
 * Initializes the libxml parser
 * @returns 1 always
 */
static inline int parser_init()
{
	cvp.userData = (void*)stderr;
	cvp.error = (xmlValidityErrorFunc) fprintf;
	cvp.warning = (xmlValidityWarningFunc) fprintf;
	return 1;
}

/**
 * Destroys the parser 
 */
static inline void parser_destroy()
{
	xmlCleanupParser();
}

/**
 * Trim the quotes from a string and duplicate it.
 * @param dest - destination for the untrimmed and duplicated string
 * @param src - source string
 */
static inline void quote_trim_dup(str *dest, char *src)
{
	int i=0;
	dest->s=0;
	dest->len=0;
	if (!src) return;
	dest->len = strlen(src);
	if (src[0]=='\"') {i++;dest->len--;}
	if (src[dest->len-1]=='\"') {dest->len--;}

	dest->s = shm_malloc(dest->len+1);
	if (!dest->s) {
		LOG_NO_MEM("shm",dest->len);
		dest->len=0;
		return;
	}
	memcpy(dest->s,src+i,dest->len);
	dest->s[dest->len]=0;
}

/**
 * Parse the cdp configuration from file to xml
 * @param filename
 * @return the xmlDocPtr or null on error
 */
xmlDocPtr parse_dp_config_file(char* filename)
{
	FILE *f=0;
	xmlDocPtr doc;

	parser_init();

	if (!filename){
		LM_ERR("ERROR:parse_dp_config_file(): filename parameter is null\n");
		goto error;
	}
	f = fopen(filename,"r");
	if (!f){
		LM_ERR("ERROR:parse_dp_config_file(): Error opening <%s> file > %s\n",filename,strerror(errno));
		goto error;
	}
	fclose(f);
	
	doc = xmlParseFile(filename);
	if (!doc){
		LM_ERR("parse_dp_config_file():  This is not a valid XML file <%s>\n",
			filename);
		goto error;
	}
	
	return doc;
error:
	return 0;		
}

/**
 * Parse the cdp configuration from str to xml
 * @param filename
 * @return the xmlDocPtr or null on error
 */
xmlDocPtr parse_dp_config_str(str config_str)
{
	xmlDocPtr doc;
	
	char c = config_str.s[config_str.len];
	if (!config_str.len){
		LM_ERR("ERROR:parse_dp_config_str(): empty string\n");
		goto error;
	}
	parser_init();

	config_str.s[config_str.len] = 0;
	doc = xmlParseDoc((xmlChar*)config_str.s);
	config_str.s[config_str.len] = c;

	if (!doc){
		LM_ERR("parse_dp_config_file():  This is not a valid XML string <%.*s>\n",
			config_str.len,config_str.s);
		goto error;
	}
	
	return  doc;
error:
	return 0;		
}

/**
 * Parses a DiameterPeer configuration file.
 * @param filename - path to the file
 * @returns the dp_config* structure containing the parsed configuration  
 */
dp_config* parse_dp_config(xmlDocPtr doc)
{
	dp_config *x=0;
	xmlNodePtr root=0,child=0,nephew=0;
	xmlChar *xc=0;
	int k;
	routing_entry *re,*rei;
	routing_realm *rr,*rri;
	
	if (!doc)
		goto error;
		
	x = new_dp_config();

	root = xmlDocGetRootElement(doc);
	if (!root){
		LM_ERR("parse_dp_config():  Empty XML \n");
		goto error;
	}

	k = xmlStrlen(root->name);
	if (k>12) k = 12;
	if (strncasecmp((char*)root->name,"DiameterPeer",k)!=0){
		LM_ERR("parse_dp_config(): XML Root is not <DiameterPeer>\n");
		goto error;
	}

	xc = xmlGetProp(root,(xmlChar*)"FQDN");
	if (xc){
		quote_trim_dup(&(x->fqdn),(char*)xc);
		quote_trim_dup(&(x->identity),(char*)xc);
		xmlFree(xc);
	}
	
	xc = xmlGetProp(root,(xmlChar*)"Realm");
	if (xc){
		quote_trim_dup(&(x->realm),(char*)xc);
		xmlFree(xc);
	}
	
	xc = xmlGetProp(root,(xmlChar*)"Vendor_Id");
	if (xc) x->vendor_id = atoi((char*)xc);
	else x->vendor_id = 0;

	xc = xmlGetProp(root,(xmlChar*)"Product_Name");
	if (xc){
		quote_trim_dup(&(x->product_name),(char*)xc);
		xmlFree(xc);
	}
	
	xc = xmlGetProp(root,(xmlChar*)"AcceptUnknownPeers");
	if (xc) {x->accept_unknown_peers = atoi((char*)xc);xmlFree(xc);}
	else x->accept_unknown_peers = 1;
	
	xc = xmlGetProp(root,(xmlChar*)"DropUnknownOnDisconnect");
	if (xc) {x->drop_unknown_peers = atoi((char*)xc);xmlFree(xc);}
	else x->drop_unknown_peers = 1;
	
	xc = xmlGetProp(root,(xmlChar*)"Tc");
	if (xc) {x->tc = atoi((char*)xc);xmlFree(xc);}
	else x->tc = 30;

	xc = xmlGetProp(root,(xmlChar*)"Workers");
	if (xc) {x->workers = atoi((char*)xc);xmlFree(xc);}
	else x->workers = 4;

	xc = xmlGetProp(root,(xmlChar*)"QueueLength");
	if (xc) {x->queue_length = atoi((char*)xc);xmlFree(xc);}
	else x->queue_length = 32;

	xc = xmlGetProp(root,(xmlChar*)"ConnectTimeout");
	if (xc) {x->connect_timeout= atoi((char*)xc);xmlFree(xc);}
	else x->connect_timeout = 5;

	xc = xmlGetProp(root,(xmlChar*)"TransactionTimeout");
	if (xc) {x->transaction_timeout = atoi((char*)xc);xmlFree(xc);}
	else x->transaction_timeout = 5;
	
	xc = xmlGetProp(root,(xmlChar*)"SessionsHashSize");
	if (xc) {x->sessions_hash_size = atoi((char*)xc);xmlFree(xc);}
	else x->sessions_hash_size = 128;

	xc = xmlGetProp(root,(xmlChar*)"DefaultAuthSessionTimeout");
	if (xc) {x->default_auth_session_timeout = atoi((char*)xc);xmlFree(xc);}
	else x->default_auth_session_timeout = 60;

	xc = xmlGetProp(root,(xmlChar*)"MaxAuthSessionTimeout");
	if (xc) {x->max_auth_session_timeout = atoi((char*)xc);xmlFree(xc);}
	else x->max_auth_session_timeout = 300;
	
	for(child = root->children; child; child = child->next)
		if (child->type == XML_ELEMENT_NODE)
	{
		if (xmlStrlen(child->name)==4 && strncasecmp((char*)child->name,"Peer",4)==0){
			//PEER
			x->peers_cnt++;		
		}
		else if (xmlStrlen(child->name)==8 && strncasecmp((char*)child->name,"Acceptor",8)==0){
			//Acceptor
			x->acceptors_cnt++;		
		}
		else if (xmlStrlen(child->name)==4 && (strncasecmp((char*)child->name,"Auth",4)==0||
			strncasecmp((char*)child->name,"Acct",4)==0)){
			//Application
			x->applications_cnt++;		
		}	
		else if (xmlStrlen(child->name)==15 && (strncasecmp((char*)child->name,"SupportedVendor",15)==0)){
			//SupportedVendor
			x->supported_vendors_cnt++;		
		}	
	}
	x->peers = shm_malloc(x->peers_cnt*sizeof(peer_config));
	if (!x->peers){
		LOG_NO_MEM("shm",x->peers_cnt*sizeof(peer_config));
		goto error;
	}
	memset(x->peers,0,x->peers_cnt*sizeof(peer_config));
	x->peers_cnt=0;
	x->acceptors = shm_malloc(x->acceptors_cnt*sizeof(acceptor_config));
	if (!x->acceptors){
		LOG_NO_MEM("shm",x->acceptors_cnt*sizeof(acceptor_config));
		goto error;
	}
	memset(x->acceptors,0,x->acceptors_cnt*sizeof(acceptor_config));
	x->acceptors_cnt=0;
	x->applications = shm_malloc(x->applications_cnt*sizeof(app_config));
	if (!x->applications){
		LOG_NO_MEM("shm",x->applications_cnt*sizeof(app_config));
		goto error;
	}
	memset(x->applications,0,x->applications_cnt*sizeof(app_config));
	x->applications_cnt=0;

	x->supported_vendors = shm_malloc(x->supported_vendors_cnt*sizeof(int));
	if (!x->supported_vendors){
		LOG_NO_MEM("shm",x->supported_vendors_cnt*sizeof(int));
		goto error;
	}
	memset(x->supported_vendors,0,x->supported_vendors_cnt*sizeof(int));
	x->supported_vendors_cnt=0;

	for(child = root->children; child; child = child->next)
		if (child->type == XML_ELEMENT_NODE)
	{
		if (xmlStrlen(child->name)==4 && strncasecmp((char*)child->name,"Peer",4)==0){
			//PEER
			xc = xmlGetProp(child,(xmlChar*)"FQDN");
			if (xc){
				quote_trim_dup(&(x->peers[x->peers_cnt].fqdn),(char*)xc);
				xmlFree(xc);
			}
			xc = xmlGetProp(child,(xmlChar*)"Realm");
			if (xc){
				quote_trim_dup(&(x->peers[x->peers_cnt].realm),(char*)xc);			
				xmlFree(xc);
			}
			xc = xmlGetProp(child,(xmlChar*)"port");
			if (xc){
				x->peers[x->peers_cnt].port = atoi((char*)xc);
				xmlFree(xc);
			}
			xc = xmlGetProp(child,(xmlChar*)"src_addr");
			if (xc){
				quote_trim_dup(&(x->peers[x->peers_cnt].src_addr),(char*)xc);
				xmlFree(xc);
			}
			x->peers_cnt++;
		}
		else if (xmlStrlen(child->name)==8 && strncasecmp((char*)child->name,"Acceptor",8)==0){
			//Acceptor
			xc = xmlGetProp(child,(xmlChar*)"bind");			
			if (xc){
				quote_trim_dup(&(x->acceptors[x->acceptors_cnt].bind),(char*)xc);			
				xmlFree(xc);
			}
			xc = xmlGetProp(child,(xmlChar*)"port");
			if (xc){
				x->acceptors[x->acceptors_cnt].port = atoi((char*)xc);						
				xmlFree(xc);
			}
			x->acceptors_cnt++;		
		}
		else if (xmlStrlen(child->name)==4 && (strncasecmp((char*)child->name,"Auth",4)==0||
			strncasecmp((char*)child->name,"Acct",4)==0)){
			//Application
			xc = xmlGetProp(child,(xmlChar*)"id");	
			if (xc){
				x->applications[x->applications_cnt].id = atoi((char*)xc);						
				xmlFree(xc);
			}
			xc = xmlGetProp(child,(xmlChar*)"vendor");
			if (xc){
				x->applications[x->applications_cnt].vendor = atoi((char*)xc);						
				xmlFree(xc);
			}
			if (child->name[1]=='u'||child->name[1]=='U')
				x->applications[x->applications_cnt].type = DP_AUTHORIZATION;						
			else
				x->applications[x->applications_cnt].type = DP_ACCOUNTING;										
			x->applications_cnt++;		
		}	
		else if (xmlStrlen(child->name)==15 && (strncasecmp((char*)child->name,"SupportedVendor",15)==0)){
			//SupportedVendor
			xc = xmlGetProp(child,(xmlChar*)"vendor");
			if (xc){
				x->supported_vendors[x->supported_vendors_cnt] = atoi((char*)xc);						
				xmlFree(xc);
			}
			x->supported_vendors_cnt++;		
		}	
		else if (xmlStrlen(child->name)==12 && (strncasecmp((char*)child->name,"DefaultRoute",12)==0)){
			if (!x->r_table) {
				x->r_table = shm_malloc(sizeof(routing_table));
				memset(x->r_table,0,sizeof(routing_table));
			}
			re = new_routing_entry();
			if (re){			
				xc = xmlGetProp(child,(xmlChar*)"FQDN");
				if (xc){
					quote_trim_dup(&(re->fqdn),(char*)xc);			
					xmlFree(xc);
				}
				xc = xmlGetProp(child,(xmlChar*)"metric");			
				if (xc){
					re->metric = atoi((char*)xc);			
					xmlFree(xc);
				}
				
				/* add it the list in ascending order */
				if (! x->r_table->routes || re->metric <= x->r_table->routes->metric){
					re->next = x->r_table->routes;
					x->r_table->routes = re;
				}else{
					for(rei=x->r_table->routes;rei;rei=rei->next)
						if (!rei->next){
							rei->next = re;
							break;						
						}else{
							if (re->metric <= rei->next->metric){
								re->next = rei->next;
								rei->next = re;
								break;
							}
						}				
				}
			}					
		}
		else if (xmlStrlen(child->name)==5 && (strncasecmp((char*)child->name,"Realm",5)==0)){
			if (!x->r_table) {
				x->r_table = shm_malloc(sizeof(routing_table));
				memset(x->r_table,0,sizeof(routing_table));
			}
			rr = new_routing_realm();
			if (rr){			
				xc = xmlGetProp(child,(xmlChar*)"name");
				quote_trim_dup(&(rr->realm),(char*)xc);			
				
				if (!x->r_table->realms) {				
					x->r_table->realms = rr;
				}else{				
					for(rri=x->r_table->realms;rri->next;rri=rri->next);
					rri->next = rr;				
				}			
				for(nephew = child->children; nephew; nephew = nephew->next)
					if (nephew->type == XML_ELEMENT_NODE){
						if (xmlStrlen(nephew->name)==5 && (strncasecmp((char*)nephew->name,"Route",5)==0))
						{
							re = new_routing_entry();
							if (re) {
								xc = xmlGetProp(nephew,(xmlChar*)"FQDN");
								if (xc){
									quote_trim_dup(&(re->fqdn),(char*)xc);	
									xmlFree(xc);
								}
								xc = xmlGetProp(nephew,(xmlChar*)"metric");
								if (xc){
									re->metric = atoi((char*)xc);			
									xmlFree(xc);
								}
								/* add it the list in ascending order */
								if (! rr->routes || re->metric <= rr->routes->metric){
									re->next = rr->routes;
									rr->routes = re;
								}else{
									for(rei=rr->routes;rei;rei=rei->next)
										if (!rei->next){
											rei->next = re;
											break;						
										}else{
											if (re->metric <= rei->next->metric){
												re->next = rei->next;
												rei->next = re;
												break;
											}
										}					
								}
							}
						}
					}
			}		
		}
	}
	
	if (doc) xmlFreeDoc(doc);	
	parser_destroy();
	return x;
error:
	if (doc) xmlFreeDoc(doc);
	parser_destroy();
	if (x) free_dp_config(x);
	return 0;	
}

