/*
 * $Id$
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

#include "../../parser/parse_uri.h"
#include "userdata_parser.h"
#include "../../parser/parse_hname2.h"

int ctxtInit=0;							/**< the XML context		*/
static xmlDtdPtr dtd;
static xmlValidCtxtPtr dtdCtxt;
static xmlSchemaValidCtxtPtr xsdCtxt=0;	/**< Schema Validating context */
static xmlSchemaPtr xsd;

/**
 *	Duplicate a string into shm and trim leading&trailing spaces and surrounding quotes.
 * @param dest - destination
 * @param src - source
 */
void space_quotes_trim_dup(str *dest,char * src) {
	int i = 0;
	//right space trim
	if (src == NULL) return ;
	dest->len = strlen(src);
	i = dest->len - 1;
	while((src[i] == ' '||src[i]=='\t') && i > 0) {
		dest->len--;
		i--;
	}
	//left space trim
	i = 0;
	while((src[i] == ' '||src[i]=='\t') && i<dest->len)
		i++;

	while(i<dest->len &&(src[i]=='\"'&&src[dest->len-1]=='\"')){
		i++;
		if (i<dest->len) dest->len--;
	}

	dest->len -= i;
	if (dest->len<=0) return;
	dest->s = shm_malloc(dest->len);
	memcpy(dest->s, src+i , dest->len);
}

/**
 * Converts strings to integer values.
 * - False -> 0
 * - True  -> 1
 * @param x - input value
 * @returns int value
 */
static inline char ifc_tBool2char(xmlChar *x)
{
	int r=0;	
	while(x[r]){
		switch(x[r]){
			case '0': return 0;
			case '1': return 1;
			case 't': case 'T': return 1;
			case 'f': case 'F': return 0;
		}
		r++;
	}
	return 0;
}

/**
 * Converts strings to integer values.
 * - SESSION_CONTINUED   -> 0
 * - SESSION_TERMINATED  -> 1
 * @param x - input value
 * @returns int value
 */
static inline char ifc_tDefaultHandling2char(xmlChar *x)
{
	char r;	
	r = strtol((char*)x, (char **)NULL, 10);
	if (errno==EINVAL){
		while(x[0]){
			if (x[0]=='c'||x[0]=='C') return 0;//SESSION_CONTINUED
			if (x[0]=='r'||x[0]=='R') return 1;//SESSION_TERMINATED
			x++;
		}
		return 0;
	} 
	else return (char)r; 
}

/**
 * Converts strings to integer values.
 * - ORIGINATING_SESSION      -> 0
 * - TERMINATING_REGISTERED   -> 1
 * - TERMINATING_UNREGISTERED -> 2
 * @param x - input value
 * @returns int value
 */
static inline char ifc_tDirectionOfRequest2char(xmlChar *x)
{
	int r;	
	r = strtol((char*)x, (char **)NULL, 10);
	if (errno==EINVAL){
		while(x[0]){
			if (x[0]=='o'||x[0]=='O') return 0;//ORIGINATING_SESSION
			if (x[0]=='s'||x[0]=='S') return 1;//TERMINATING_REGISTERED
			if (x[0]=='u'||x[0]=='U') return 2;//TERMINATING_UNREGISTERED
			x++;
		}
		return 0;
	} 
	else return (char)r; 
}

/**
 * Converts strings to integer values.
 * - REGISTERED     -> 0
 * - UNREGISTERED   -> 1
 * - error (any)	 -> -1
 * @param x - input value
 * @returns int value
 */
static inline char ifc_tProfilePartIndicator2char(xmlChar *x)
{
	int r;	
	if (x==0||x[0]==0) return -1;
	r = strtol((char*)x, (char **)NULL, 10);
	if (errno==EINVAL){
		while(x[0]){
			if (x[0]=='r'||x[0]=='R') return 0;//REGISTERED
			if (x[0]=='u'||x[0]=='U') return 1;//UNREGISTERED
			x++;
		}
		return 0;
	} 
	else return (char)r; 
}

/**
 * Duplicate a string into shm and trim leading&trailing spaces.
 * @param dest - destination
 * @param src - source
 */
static inline void space_trim_dup(str *dest, char *src)
{
    	int i;
	dest->s=0;
	dest->len=0;
	if (!src) return;
	dest->len = strlen(src);
	i = dest->len-1;
	while((src[i]==' '||src[i]=='\t') && i>0) 
		i--;
	i=0;
	while((src[i]==' '||src[i]=='\t') && i<dest->len)
		i++;
	dest->len -= i;
        
	dest->s = shm_malloc(dest->len);
	if (!dest->s) {
		LM_ERR("Out of memory allocating %d bytes\n",dest->len);
		dest->len=0;
		return;
	}
	memcpy(dest->s,src+i,dest->len);
}

/**
 *	Parse a Application Server.
 * @param doc - the XML document
 * @param node - the current node
 * @param as - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_application_server(xmlDocPtr doc,xmlNodePtr node,ims_application_server *as)
{
	xmlNodePtr child;
	xmlChar *x;
	as->server_name.s=NULL;as->server_name.len=0;
	as->default_handling=IFC_NO_DEFAULT_HANDLING;
	as->service_info.s=NULL;as->service_info.len=0;

	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'S':case 's':	{//ServerName / ServiceInfo
					switch (child->name[4]) {
						case 'E':case 'e':  //ServerName
							x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                                                        space_trim_dup(&(as->server_name),(char*)x);
							xmlFree((char*)x);
							break;
						case 'I':case 'i':  //ServiceInfo
							x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                                                        space_trim_dup(&(as->service_info),(char*)x);
							xmlFree(x);
							break;
					}
					break;
				}
				case 'D':case 'd': //DefaultHandling
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					as->default_handling=ifc_tDefaultHandling2char(x);
					xmlFree(x);
					break;
			}
	return 1;
}


/**
 *	Parse SPT for SIP Header.
 * @param doc - the XML document
 * @param node - the current node
 * @param sh - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_sip_header(xmlDocPtr doc,xmlNodePtr node,ims_sip_header *sh)
{
	xmlNodePtr child;
	xmlChar *x;
	char c[256];
	int len;
	struct hdr_field hf;
	sh->header.s=NULL;sh->header.len=0;
	sh->content.s=NULL;sh->content.len=0;

	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'H':case 'h':	//Header
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					len = strlen((char*)x);		
					memcpy(c,x,len);
					c[len++]=':';
					c[len]=0;
					space_trim_dup(&(sh->header),(char*)x);
					parse_hname2(c,c+(len<4?4:len),&hf);
					sh->type=(short)hf.type;
					//LOG(L_CRIT,"[%.*s(%d)]\n",sh->header.len,sh->header.s,sh->type);
					xmlFree(x);
					break;
				case 'C':case 'c':	//Content
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					space_quotes_trim_dup(&(sh->content),(char*)x);
					xmlFree(x);
					break;
			}
	return 1;
}

/**
 *	Parse SPT for Session Description.
 * @param doc - the XML document
 * @param node - the current node
 * @param sd - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_session_desc(xmlDocPtr doc,xmlNodePtr node,ims_session_desc *sd)
{
	xmlNodePtr child;
	xmlChar *x;
	sd->line.s=NULL;sd->line.len=0;
	sd->content.s=NULL;sd->content.len=0;

	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'L':case 'l':	//Line
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					space_trim_dup(&(sd->line),(char*)x);
					xmlFree(x);
					break;
				case 'C':case 'c':	//Content
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					space_quotes_trim_dup(&(sd->content),(char*)x);
					xmlFree(x);
					break;
			}
	return 1;
}

/**
 *	Parse a Service Point Trigger Extension (RegistrationType).
 * @param doc - the XML document
 * @param node - the current node
 * @param spt - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_spt_extension(xmlDocPtr doc,xmlNodePtr node,ims_spt *spt)
{
	xmlNodePtr child;
	xmlChar *x;

	for(child=node->children ; child ; child=child->next) {
		if (child->type==XML_ELEMENT_NODE && (child->name[0]=='R' || child->name[0]=='r')) {
			x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			switch(atoi((char*)x)) {
				case 0:
					spt->registration_type |= IFC_INITIAL_REGISTRATION;
					break;
				case 1:
					spt->registration_type |= IFC_RE_REGISTRATION;
					break;
				case 2:
					spt->registration_type |= IFC_DE_REGISTRATION;
					break;
			}								
			xmlFree(x);
		}
	}
//	LOG(L_CRIT,"INFO:"M_NAME":parse_spt_extension: spt->registration_type=%d\n",spt->registration_type);
	return 1;			
}

/**
 *	Parse a Service Point Trigger.
 * @param doc - the XML document
 * @param node - the current node
 * @param spt_to - structure to fill
 * @param spt_cnt - structure to fill with the spt count
 * @returns 1 on success, 0 on failure
 */
static int parse_spt(xmlDocPtr doc,xmlNodePtr node,ims_spt *spt_to,unsigned short *spt_cnt)
{
	xmlNodePtr child,saved=0;
	xmlChar *x;

	ims_spt *spt,*spt2;
	int group;
	
	spt = spt_to + *spt_cnt;
	
	spt->condition_negated=0;
	spt->group=0;
	spt->type=IFC_UNKNOWN;
	spt->registration_type=0;

	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'C':case 'c': //ConditionNegated
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					spt->condition_negated=ifc_tBool2char(x);
					xmlFree(x);
					break;
				case 'G':case 'g': //Group
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					spt->group=atoi((char*)x);
					xmlFree(x);
					break;
				case 'R':case 'r': //RequestUri
					spt->type=IFC_REQUEST_URI;
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					space_trim_dup(&(spt->request_uri),(char*)x);
					xmlFree(x);
					break;
				case 'E':case 'e': //Extension
				    parse_spt_extension(doc,child,spt);
					break;
				case 'M':case 'm': //method
					spt->type=IFC_METHOD;
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					space_trim_dup(&(spt->method),(char*)x);
					xmlFree(x);
					break;
				case 'S':case 's': {//SIPHeader/SessionCase/SessionDescription
					switch(child->name[7]) {
						case 'E':case 'e'://SIP_HEADER
							spt->type=IFC_SIP_HEADER;
							parse_sip_header(doc,child,&(spt->sip_header));
							saved = child;
							break;
						case 'C':case 'c'://Session Case
							spt->type=IFC_SESSION_CASE;
							x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
							spt->session_case=ifc_tDirectionOfRequest2char(x);
							xmlFree(x);
							break;
						case 'D':case 'd'://Session Description
							spt->type=IFC_SESSION_DESC;
							parse_session_desc(doc,child,&(spt->session_desc));
							saved = child;
							break;
					}

				}
					break;
			}
	*spt_cnt=*spt_cnt+1;

	/* adding the other nodes for multiple groups */			
	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'G':case 'g': //Group
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					group=atoi((char*)x);
					xmlFree(x);
					if (group != spt->group){
						spt2 = spt_to + *spt_cnt;
						spt2->condition_negated = spt->condition_negated;
						spt2->group = group;
						spt2->type = spt->type;
						switch(spt2->type){
							case IFC_REQUEST_URI:
								spt2->request_uri.len = spt->request_uri.len;
								spt2->request_uri.s = shm_malloc(spt2->request_uri.len);
								if (!spt2->request_uri.s){
									LM_ERR("Out of memory allocating %d bytes\n",spt->request_uri.len);
									break;
								}
								memcpy(spt2->request_uri.s,spt->request_uri.s,spt->request_uri.len);
								break;
							case IFC_METHOD:
								spt2->method.len = spt->method.len;
								spt2->method.s = shm_malloc(spt2->method.len);
								if (!spt2->method.s){
									LM_ERR("Out of memory allocating %d bytes\n",spt->method.len);
									break;
								}
								memcpy(spt2->method.s,spt->method.s,spt->method.len);
								break;
							case IFC_SIP_HEADER:
								parse_sip_header(doc,saved,&(spt2->sip_header));
								break;
							case IFC_SESSION_CASE:
								spt2->session_case = spt->session_case;
								break;
							case IFC_SESSION_DESC:
								parse_session_desc(doc,saved,&(spt2->session_desc));
								break;								
						}
						spt2->registration_type = spt->registration_type;
						*spt_cnt = *spt_cnt+1;						
					}
					break;
			}
	return 1;			
}


/**
 *	Parse a Trigger Point.
 * @param doc - the XML document
 * @param node - the current node
 * @param tp - structure to fill
 * @returns 1 on success, 0 on failure
 * \todo An effective sort for the priority
 */
static int parse_trigger_point(xmlDocPtr doc,xmlNodePtr node,ims_trigger_point *tp)
{
	xmlNodePtr child,child2;
	xmlChar *x;
	unsigned short spt_cnt=0;
	int i,j;
	ims_spt spttemp;
	tp->condition_type_cnf=IFC_DNF;//0
	tp->spt=NULL;
	tp->spt_cnt=0;

	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'C':case 'c': //ConditionTypeCNF
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					tp->condition_type_cnf=ifc_tBool2char(x);
					xmlFree(x);
					break;
				case 'S':case 's': //SPT - Service Point Trigger
					// COUNT all in another groups
					for(child2=child->children ; child2 ; child2=child2->next)
						if (child2->type==XML_ELEMENT_NODE)
							switch (child2->name[0]) {
								case 'G':case 'g':
									spt_cnt++;
							}
					break;
			}
	tp->spt = (ims_spt*) shm_malloc(sizeof(ims_spt)*spt_cnt);
	if (!tp->spt){
		LM_ERR("Out of memory allocating %lx bytes\n",sizeof(ims_spt)*spt_cnt);
		return 0;
	}
	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'S':case 's': //SPT - Service Point Trigger
					parse_spt(doc,child,tp->spt,&(tp->spt_cnt));
					/*i=0;
					while(i<tp->spt_cnt&&tp->spt[i].group<spttemp.group)
						i++;
					for(j=tp->spt_cnt-1;j>=i;j--)
						tp->spt[j+1]=tp->spt[j];
					tp->spt[i]=spttemp;
					tp->spt_cnt++;*/
			
					break;
			}
	
	j=1;
	while(j){
		j=0;
		for(i=0;i<tp->spt_cnt-1;i++)
			if (tp->spt[i].group > tp->spt[i+1].group){
				j=1;
				spttemp = tp->spt[i];
				tp->spt[i]=tp->spt[i+1];
				tp->spt[i+1]=spttemp;
			}			
	}
	return 1;
}

/**
 *	Parse a Filter Criteria.
 * @param doc - the XML document
 * @param node - the current node
 * @param fc - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_filter_criteria(xmlDocPtr doc,xmlNodePtr node,ims_filter_criteria *fc)
{
	xmlNodePtr child;
	xmlChar *x;
	char k;
	fc->priority=0;
	fc->trigger_point=NULL;
	fc->profile_part_indicator=NULL;
	//fc->apllication_server init mai tarziu
	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[3]) {
				case 'O':case 'o':	//Priority
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					fc->priority=atoi((char*)x);
					xmlFree(x);
					break;
				case 'G':case 'g':	//TriggerPoint
					fc->trigger_point=(ims_trigger_point*) shm_malloc(sizeof(ims_trigger_point));
					if (!fc->trigger_point){
						LM_ERR("Out of memory allocating %lx bytes\n",sizeof(ims_trigger_point));
						break;
					}
					if (!parse_trigger_point(doc,child,fc->trigger_point)){
						shm_free(fc->trigger_point);
						fc->trigger_point=0;
						return 0;
					}
					break;
				case 'L':case 'l':	//ApplicationServer
					parse_application_server(doc,child,&(fc->application_server));
					break;
				case 'F':case 'f':	//ProfilePartIndicator
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					k = ifc_tProfilePartIndicator2char(x);
					if (k<0) break;
					fc->profile_part_indicator=(char*)shm_malloc(sizeof(char));
					if (!fc->profile_part_indicator){
						LM_ERR("Out of memory allocating %lx bytes\n",sizeof(ims_trigger_point));
						break;
					}
					*fc->profile_part_indicator=k;
					xmlFree(x);
					break;
			}
	return 1;
}



/**
 * Parse the Public Identity.
 * @param doc - the XML document
 * @param root - the current node
 * @param pi - structure to fill
 * @returns 1 on success, 0 on failure , 2 if its a wildcardpsi
 */
static int parse_public_identity(xmlDocPtr doc, xmlNodePtr root, ims_public_identity *pi)
{
	xmlNodePtr child;
	xmlNodePtr grandson;
	xmlChar *x;
	int return_code=1;
	
	for(child=root->children;child;child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]){
				case 'I': case 'i':
					if (!pi->public_identity.len){
						x = xmlNodeListGetString(doc,child->xmlChildrenNode,1);
						space_trim_dup(&(pi->public_identity),(char*)x);
						xmlFree(x);
					}					
					break;
				case 'B': case 'b':
					x = xmlNodeListGetString(doc,child->xmlChildrenNode,1);
					pi->barring = ifc_tBool2char(x);
					xmlFree(x);
					break;
				//lets add something 
				case 'E' : case 'e':
					// that would be Extension
					// here i need to parse Identity Type 
					// if its two then  wildcardedpsi
					// and then extension!!!
					// I have to check how you parse this shit

					for(grandson=child->children;grandson;grandson=grandson->next)
					{
												
						if (grandson->type==XML_ELEMENT_NODE)
						{
							switch (grandson->name[0]) {
								case 'I' : case 'i':
									//identity type 0 public identity 1 distinct psi 2 wildcard psi
									//x = xmlNodeListGetString(doc,grandson->xmlChildrenNode,1);
									// i need to compare x with 2, but i have to trim leading 
									// space characters or tabs			
									//xmlFree(x);
									break;
								case 'W' : case 'w':
									//wildcardpsi
									if(!scscf_support_wildcardPSI) {
										LOG(L_ERR,"Configured without support for Wildcard PSI and got one from HSS\n");
										LOG(L_ERR,"the identity will be stored but never be matched, please include the parameter to support wildcard PSI in the config file\n");
									}
									
									x = xmlNodeListGetString(doc,grandson->xmlChildrenNode,1);
									space_trim_dup(&(pi->wildcarded_psi),(char*)x);
									
									xmlFree(x);
									return_code=2;
									break;
								default :
									break;
							}
						}
					}
										
					break;				
			}

	return return_code;
}

/**
 *	Parse a Core Network Service Authorization.
 * @param doc - the XML document
 * @param node - the current node
 * @param cn - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_cn_service_auth(xmlDocPtr doc,xmlNodePtr node,ims_cn_service_auth *cn)
{
	xmlNodePtr child;
	xmlChar *x;
	cn->subscribed_media_profile_id=-1;
	for(child=node->children ; child ; child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]) {
				case 'S':case 's':	//BarringIndication
					x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
					cn->subscribed_media_profile_id=atoi((char*)x);
					xmlFree(x);
					return 1;
					break;

			}
	return 0;
}

/**
 *	Parse a Core Network Service Profile.
 * @param doc - the XML document
 * @param root - the current node
 * @param sp - structure to fill
 * @returns 1 on success, 0 on failure
 */
static int parse_service_profile(xmlDocPtr doc, xmlNodePtr root, ims_service_profile *sp) {
    xmlNodePtr child;
    xmlChar *x;
    unsigned short pi_cnt = 0, ifc_cnt = 0, sh_cnt = 0;
    int i, j;
    ims_filter_criteria fctemp;
    int returncode = 0;

    for (child = root->children; child; child = child->next)
        if (child->type == XML_ELEMENT_NODE) {
            LM_DBG("child name is [%s]\n", child->name);
            switch (child->name[0]) {
                case 'P': case 'p':
                    pi_cnt++;
                    break;
                case 'i':case 'I': //InitialFilterCriteria
                    ifc_cnt++;
                    break;
                case 'c':case 'C': //CoreNetworkServiceAuthorization
                    sp->cn_service_auth = (ims_cn_service_auth*) shm_malloc(
                            sizeof (ims_cn_service_auth));
                    break;
                case 's':case 'S': //SharedIFCSet
                    sh_cnt++;
                    break;

            }

        }

    sp->public_identities = shm_malloc(pi_cnt * sizeof (ims_public_identity));
    if (!sp->public_identities) {
        LM_ERR("Out of memory allocating %lx bytes\n", pi_cnt * sizeof (ims_public_identity));
        return 0;
    }
    memset(sp->public_identities, 0, pi_cnt * sizeof (ims_public_identity));
    
    sp->filter_criteria = (ims_filter_criteria*) shm_malloc(sizeof (ims_filter_criteria) * ifc_cnt);
    if (!sp->filter_criteria) {
        LM_ERR("Out of memory allocating %lx bytes\n", ifc_cnt * sizeof (ims_filter_criteria));
        return 0;
    }
    memset(sp->filter_criteria, 0, ifc_cnt * sizeof (ims_filter_criteria));
    
    sp->shared_ifc_set = (int*) shm_malloc(sizeof (int) *sh_cnt);
    if (!sp->shared_ifc_set) {
        LM_ERR("Out of memory allocating %lx bytes\n", sh_cnt * sizeof (int));
        return 0;
    }
    memset(sp->shared_ifc_set, 0, sh_cnt * sizeof (int));
    
    for (child = root->children; child; child = child->next)
        if (child->type == XML_ELEMENT_NODE)
            switch (child->name[0]) {
                case 'P': case 'p':
                    returncode = parse_public_identity(doc, child, &(sp->public_identities[sp->public_identities_cnt]));
                    if (returncode)
                        sp->public_identities_cnt++;
                    break;
                case 'I':case 'i': //InitialFilterCriteria
                    if (!parse_filter_criteria(doc, child, &(fctemp)))
                        break;
                    i = 0;
                    while (i < sp->filter_criteria_cnt && sp->filter_criteria[i].priority < fctemp.priority)
                        i++;
                    for (j = sp->filter_criteria_cnt - 1; j >= i; j--)
                        sp->filter_criteria[j + 1] = sp->filter_criteria[j];
                    sp->filter_criteria[i] = fctemp;
                    sp->filter_criteria_cnt++;
                    break;
                case 'C':case 'c': //CoreNetworkServiceAuthorization
                    if (!parse_cn_service_auth(doc, child, sp->cn_service_auth)) {
                        shm_free(sp->cn_service_auth);
                        sp->cn_service_auth = 0;
                    }
                    break;
                case 'S':case 's': //SharedIFCSet
                    x = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                    sp->shared_ifc_set[sp->shared_ifc_set_cnt++] = atoi((char*) x);
                    xmlFree(x);
                    break;
            }
    if (returncode == 2)
        return 2; // i need to know if there is a wildcardpsi hiding in the public_identity
    return 1;
}

/**
 *	Parse a IMS Subscription.
 * @param doc - the XML document
 * @param root - the current node
 * @returns the ims_subscription* on success or NULL on error
 */
static ims_subscription* parse_ims_subscription(xmlDocPtr doc, xmlNodePtr root)
{
	xmlNodePtr child;
	xmlChar *x;
	ims_subscription *s;
	unsigned short sp_cnt=0;
	int rc;
	
	if (!root) return 0;
	while(root->type!=XML_ELEMENT_NODE || strcasecmp((char*)root->name,"IMSSubscription")!=0){
		root = root->next;
	}
	if (!root) {
		LM_ERR("No IMSSubscription node found\n");
		return 0;
	}
	s = (ims_subscription*) shm_malloc(sizeof(ims_subscription));
	if (!s) {
		LM_ERR("Out of memory allocating %lx bytes\n",sizeof(ims_subscription));
		return 0;
	}
	memset(s,0,sizeof(ims_subscription));
	for(child=root->children;child;child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			switch (child->name[0]){
				case 'P':case 'p':  /* Private Identity */
					if (!s->private_identity.len){
						x = xmlNodeListGetString(doc,child->xmlChildrenNode,1);
						space_trim_dup(&(s->private_identity),(char*)x);
						xmlFree(x);
					}
					break;
				case 'S':case 's':	/* Service Profile */
					sp_cnt++;
					break;					
			}
	s->service_profiles = (ims_service_profile*) shm_malloc(sp_cnt * sizeof(ims_service_profile));
	if (!s->service_profiles) {
		LM_ERR("Out of memory allocating %lx bytes\n",sp_cnt*sizeof(ims_service_profile));
		return s;	
	}
	memset(s->service_profiles,0,sp_cnt * sizeof(ims_service_profile));
	for(child=root->children;child;child=child->next)
		if (child->type==XML_ELEMENT_NODE)
			if (child->name[0]=='S' || child->name[0]=='s')
			{
				rc=parse_service_profile(doc,child,&(s->service_profiles[s->service_profiles_cnt]));
				if (rc==2)
					s->wpsi=1;
				if (rc)
					s->service_profiles_cnt++;
			}				
	s->lock = lock_alloc();
	if (s->lock==0) {
		LM_ERR("Failed to allocate Lock for IMS Subscription\n");
		shm_free(s);
		return 0;
	}
	if (lock_init(s->lock)==0){
		LM_ERR("Failed to initialize Lock for IMS Subscription\n");
		lock_dealloc(s->lock);
		s->lock=0;
		shm_free(s);
		return 0;
	}
	return s;
}


/**
 * Parses the user data XML and copies data into a new ims_subscription structure.
 * @param xml - the input xml (NB must be null terminated)
 * @returns the ims_subscription* on success or NULL on error
 */
ims_subscription *parse_user_data(str xml)
{
	xmlDocPtr doc=0;
	xmlNodePtr root=0;
	ims_subscription *s = 0;
	if (!ctxtInit) parser_init(scscf_user_data_dtd,scscf_user_data_xsd);	
	doc=0;
	
	doc = xmlParseDoc((unsigned char *)xml.s);
	if (!doc){
		LM_ERR("This is not a valid XML <%.*s>\n", xml.len,xml.s);
		goto error;
	}

	if (dtdCtxt){
		if (xmlValidateDtd(dtdCtxt,doc,dtd)!=1){
			LM_ERR("Verification of XML against DTD failed <%.*s>\n", xml.len,xml.s);
			goto error;
		}
	}
	if (xsdCtxt){
		if (xmlSchemaValidateDoc(xsdCtxt,doc)!=0){
			LM_ERR("Verification of XML against XSD failed <%.*s>\n", xml.len,xml.s);
			goto error;
		}
	}

	root = xmlDocGetRootElement(doc);
	if (!root){
		LM_ERR("Empty XML <%.*s>\n",
			xml.len,xml.s);
		goto error;
	}
	s = parse_ims_subscription(doc,root);
	if (!s){
		LM_ERR("Error while loading into  ims subscription structure\n");
		goto error;		
	}
	xmlFreeDoc(doc);
	print_user_data(s);
	return s;
error:	
	if (doc) xmlFreeDoc(doc);
	return 0;	
}

/**
 * Initializes the libxml2 parser.
 * @param dtd_filename - path to the DTD or NULL if none
 * @param xsd_filename - path to the XSD or NULL if none
 * @returns 1 on success or 0 on error
 */
int parser_init(char *dtd_filename, char *xsd_filename)
{
	if (dtd_filename){
		dtd = xmlParseDTD(NULL,(unsigned char*)dtd_filename);
		if (!dtd){
			LM_ERR("unsuccesful DTD parsing from file <%s>\n",
				dtd_filename);
			return 0;
		}
		dtdCtxt = xmlNewValidCtxt();
		dtdCtxt->userData = (void*)stderr;
		dtdCtxt->error = (xmlValidityErrorFunc) fprintf;
		dtdCtxt->warning = (xmlValidityWarningFunc) fprintf;
	}
	if (xsd_filename){
		xmlSchemaParserCtxtPtr ctxt;
		ctxt = xmlSchemaNewParserCtxt(xsd_filename);
		if (!ctxt) {
			LM_ERR("unsuccesful XSD parsing from file <%s>\n",
				xsd_filename);
			return 0;
		}
		xmlSchemaSetParserErrors(ctxt,(xmlValidityErrorFunc) fprintf,(xmlValidityWarningFunc) fprintf,stderr);
		xsd = xmlSchemaParse(ctxt);
		xmlSchemaFreeParserCtxt(ctxt);		
		
		xsdCtxt = xmlSchemaNewValidCtxt(xsd);
		xmlSchemaSetValidErrors(xsdCtxt,(xmlValidityErrorFunc) fprintf,(xmlValidityWarningFunc) fprintf,stderr);
	}
	ctxtInit=1;
	return 1;
}


/**
 * Print the contents of an ims_subscription structure.
 * @param log_level - level to log on
 * @param s - the ims_subscription to be printed
 */
void print_user_data(ims_subscription *s) {
    int i, j, k;

    LM_DBG("IMSSubscription:\n");
    if (!s) return;
    
    LM_DBG("Private Identity: <%.*s>\n", s->private_identity.len, s->private_identity.s);
    for (i = 0; i < s->service_profiles_cnt; i++) {
        LM_DBG("\tService Profile:\n");
        for (j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
            LM_DBG("\t\tPublic Identity: Barring [%d] <%.*s> \n",
                    s->service_profiles[i].public_identities[j].barring,
                    s->service_profiles[i].public_identities[j].public_identity.len,
                    s->service_profiles[i].public_identities[j].public_identity.s);
        }
        for (j = 0; j < s->service_profiles[i].filter_criteria_cnt; j++) {
            LM_DBG("\t\tFilter Criteria: Priority [%d]ProfilePartInd [%d]\n",
                    s->service_profiles[i].filter_criteria[j].priority,
                    s->service_profiles[i].filter_criteria[j].profile_part_indicator ?
                    *(s->service_profiles[i].filter_criteria[j].profile_part_indicator) : -1);
            if (s->service_profiles[i].filter_criteria[j].trigger_point) {
                LM_DBG("\t\t\tTrigger Point: CNF [%c] %s\n",
                        s->service_profiles[i].filter_criteria[j].trigger_point->condition_type_cnf ? 'X' : ' ',
                        s->service_profiles[i].filter_criteria[j].trigger_point->condition_type_cnf ? "(_|_)&(_|_)" : "(_&_)|(_&_)"
                        );
                for (k = 0; k < s->service_profiles[i].filter_criteria[j].trigger_point->spt_cnt; k++) {
                    LM_DBG("\t\t\t\tSPT: Grp[%d] NOT[%c] RegType[%d]\n",
                            s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].group,
                            s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].condition_negated ? 'X' : ' ',
                            s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].registration_type
                            );
                    switch (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].type) {
                        case 1:
                            LM_DBG("\t\t\t\t\t Request-URI == <%.*s>\n",
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].request_uri.len,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].request_uri.s);
                            break;
                        case 2:
                            LM_DBG("\t\t\t\t\t Method == <%.*s>\n",
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].method.len,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].method.s);
                            break;
                        case 3:
                            LM_DBG("\t\t\t\t\t Hdr(%.*s(%d)) == <%.*s>\n",
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.header.len,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.header.s,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.type,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.content.len,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.content.s);
                            break;
                        case 4:
                            LM_DBG("\t\t\t\t\t SessionCase [%d]\n",
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_case);
                            break;
                        case 5:
                            LM_DBG("\t\t\t\t\t SDP(%.*s) == <%.*s>\n",
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.line.len,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.line.s,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.content.len,
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.content.s);
                            break;
                    }
                }
            }
            LM_DBG("\t\t\tAS: <%.*s> Handling [%d] SrvInfo: <%.*s>\n",
                    s->service_profiles[i].filter_criteria[j].application_server.server_name.len,
                    s->service_profiles[i].filter_criteria[j].application_server.server_name.s,
                    s->service_profiles[i].filter_criteria[j].application_server.default_handling,
                    s->service_profiles[i].filter_criteria[j].application_server.service_info.len,
                    s->service_profiles[i].filter_criteria[j].application_server.service_info.s);
        }
        if (s->service_profiles[i].cn_service_auth) {
            LM_DBG("\t\tCN Serv Auth: Subs Media Profile ID [%d]\n",
                    s->service_profiles[i].cn_service_auth->subscribed_media_profile_id);
        }
        for (j = 0; j < s->service_profiles[i].shared_ifc_set_cnt; j++) {
            LM_DBG("\t\tShared IFC Set: [%d]\n",
                    s->service_profiles[i].shared_ifc_set[j]);
        }
    }
}

str cscf_get_realm_from_ruri(struct sip_msg *msg) {
	str realm = { 0, 0 };
	if (!msg || msg->first_line.type != SIP_REQUEST) {
		LM_ERR("This is not a request!!!\n");
		return realm;
	}

	if (!msg->parsed_orig_ruri_ok)
		if (parse_orig_ruri(msg) < 0)
			return realm;

	realm = msg->parsed_orig_ruri.host;
	return realm;
}
