/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <gnome-xml/libxml/xmlmemory.h>
#include <gnome-xml/libxml/parser.h>
#include "CPL_tree.h"
#include "sub_list.h"
#include "../../dprint.h"



struct node *list = 0;


#define FOR_ALL_ATTR(_node,_attr) \
	for( (_attr)=(_node)->properties ; (_attr) ; (_attr)=(_attr)->next)



unsigned char encode_node_name(char *node_name)
{
	switch (node_name[0]) {
		case 'a':
		case 'A':
			switch (node_name[7]) {
				case 0:
					return ADDRESS_NODE;
				case '-':
					return ADDRESS_SWITCH_NODE;
				default:
					return ANCILLARY_NODE;
			}
		case 'B':
		case 'b':
			return BUSY_NODE;
		case 'c':
		case 'C':
			return CPL_NODE;
		case 'd':
		case 'D':
			return DEFAULT_NODE;
		case 'f':
		case 'F':
			return FAILURE_NODE;
		case 'i':
		case 'I':
			return INCOMING_NODE;
		case 'l':
		case 'L':
			switch (node_name[2]) {
				case 'g':
				case 'G':
					return LOG_NODE;
				case 'o':
				case 'O':
					return LOOKUP_NODE;
				case 'c':
				case 'C':
					return LOCATION_NODE;
				default:
					if (node_name[8])
						return LANGUAGE_SWITCH_NODE;
					else
						return LANGUAGE_NODE;
			}
		case 'm':
		case 'M':
			return MAIL_NODE;
		case 'n':
		case 'N':
			if (node_name[2]=='T' || node_name[2]=='t')
				return NOTFOUND_NODE;
			else
				return NOANSWER_NODE;
		case 'o':
		case 'O':
			if (node_name[1]=='t' || node_name[1]=='T')
				return OTHERWISE_NODE;
			else
				return OUTGOING_NODE;
		case 'p':
		case 'P':
			if (node_name[2]=='o' || node_name[2]=='O')
				return PROXY_NODE;
			if (node_name[8])
				return PRIORITY_SWITCH_NODE;
			else
				return PRIORITY_NODE;
		case 'r':
		case 'R':
			switch (node_name[2]) {
				case 'j':
				case 'J':
					return REJECT_NODE;
				case 'm':
				case 'M':
					return REMOVE_LOCATION_NODE;
				default:
					if (node_name[8])
						return REDIRECTION_NODE;
					else
						return REDIRECT_NODE;
			}
		case 's':
		case 'S':
			switch (node_name[3]) {
				case 0:
					return SUB_NODE;
				case 'c':
				case 'C':
					return SUCCESS_NODE;
				case 'a':
				case 'A':
					return SUBACTION_NODE;
				default:
					if (node_name[6])
						return STRING_SWITCH_NODE;
					else
						return STRING_NODE;
			}
		case 't':
		case 'T':
			if (node_name[4])
				return TIME_SWITCH_NODE;
			else
				return TIME_NODE;
	}
	return 0;
}




int encript_node_attr( xmlNodePtr node, unsigned char *node_ptr,
					unsigned int type, unsigned char *ptr,int *nr_of_attr)
{
	xmlAttrPtr    attr;
	char          *val;
	unsigned char *p;
	unsigned char *offset;
	int           nr_attr;
	int           foo;

	nr_attr = 0;
	p = ptr;

	switch (type) {
		case CPL_NODE:
		case INCOMING_NODE:
		case OUTGOING_NODE:
		case OTHERWISE_NODE:
		case LANGUAGE_SWITCH_NODE:
		case PRIORITY_SWITCH_NODE:
		case SUCCESS_NODE:
		case FAILURE_NODE:
		case NOTFOUND_NODE:
		case BUSY_NODE:
		case NOANSWER_NODE:
		case REDIRECTION_NODE:
		case DEFAULT_NODE:
		case ANCILLARY_NODE:
			break;
		/* enconding attributes and values fro ADDRESS-SWITCH node */
/*
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

		case ADDRESS_SWITCH_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'F': case 'f':
						*(p++) = FIELD_ATTR;
						if (val[0]=='D' || val[0]=='d')
							*(p++) = DESTINATION_VAL;
						else if (val[6]=='-')
							*(p++) = ORIGINAL_DESTINATION_VAL;
						else if (!val[6])
							*(p++) = ORIGIN_VAL;
						else goto error;
						break;
					case 'S': case 's':
						*(p++) = SUBFIELD_ATTR;
						switch (val[0]) {
							case 'a': case 'A':
								*(p++)=ADDRESS_TYPE_VAL;break;
							case 'u': case 'U':
								*(p++)=USER_VAL;break;
							case 'h': case 'H':
								*(p++)=HOST_VAL;break;
							case 'p': case 'P':
								*(p++)=PORT_VAL;break;
							case 't': case 'T':
								*(p++)=TEL_VAL;break;
							case 'd': case 'D':
								*(p++)=DISPLAY_VAL;break;
							default: goto error;
						}
						break;
					default: goto error;
				}
			}
			break;
		/* enconding attributes and values fro ADDRESS node */
		case ADDRESS_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch (attr->name[0]) {
					case 'i': case 'I':
						*(p++) = IS_ATTR; break;
					case 'c': case 'C':
						*(p++) = CONTAINS_ATTR; break;
					case 's': case 'S':
						*(p++) = SUBDOMAIN_OF_ATTR; break;
					default: goto error;
				}
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for STRING-SWITCH node */
		case STRING_SWITCH_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				if (attr->name[0]=='F' || attr->name[0]=='f') {
					*(p++) = FIELD_ATTR;
					switch (val[0]) {
						case 'S': case 's':
							*(p++) = SUBJECT_VAL; break;
						case 'O': case 'o':
							*(p++) = ORGANIZATION_VAL; break;
						case 'U': case 'u':
							*(p++) = USER_AGENT_VAL; break;
						case 'D': case 'd':
							*(p++) = DISPLAY_VAL; break;
						default: goto error;
					}
				} else goto error;
			}
			break;
		/* enconding attributes and values for STRING node */
		case STRING_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch (attr->name[0]) {
					case 'i': case 'I':
						*(p++) = IS_ATTR; break;
					case 'c': case 'C':
						*(p++) = CONTAINS_ATTR; break;
					default: goto error;
				}
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for LANGUAGE node */
		case LANGUAGE_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				if (attr->name[0]=='M' || attr->name[0]=='m') {
					*(p++) = MATCHES_ATTR;
				} else goto error;
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for TIME-SWITCH node */
		case TIME_SWITCH_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[2]) {
					case 'I': case 'i':
						*(p++) = TZID_ATTR;break;
					case 'U': case 'u':
						*(p++) = TZURL_ATTR;break;
					default: goto error;
				}
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for TIME node */
		case TIME_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch (attr->name[4]) {
					case 0:
						if (attr->name[0]=='F' || attr->name[0]=='f')
							*(p++) = FREQ_ATTR;
						else if (attr->name[0]=='W' || attr->name[0]=='w')
							*(p++) = WKST_ATTR;
						break;
					case 'a': case 'A':
						if (attr->name[0]=='D' || attr->name[0]=='d')
							*(p++) = DTSTART_ATTR;
						else if (attr->name[0]=='B' || attr->name[0]=='b')
							*(p++) = BYYEARDAY_ATTR;
						break;
					case 't': case 'T':
						if (attr->name[0]=='D' || attr->name[0]=='d')
							*(p++) = DURATION_ATTR;
						else if (attr->name[0]=='C' || attr->name[0]=='c')
							*(p++) = COUNT_ATTR;
						else if (attr->name[0]=='B' || attr->name[0]=='b')
							*(p++) = BYSETPOS_ATTR;
						break;
					case 'n': case 'N':
						if (!attr->name[0])
							*(p++) = BYMONTH_ATTR;
						else if (attr->name[0]=='D' || attr->name[0]=='d')
							*(p++) = BYMONTHDAY_ATTR;
						else if (attr->name[0]=='e' || attr->name[0]=='E')
							*(p++) = BYMINUTE_ATTR;
						break;
					case 'd': case 'D':
						*(p++) = DTEND_ATTR; break;
					case 'r': case 'R':
						*(p++) = INTERVAL_ATTR; break;
					case 'l': case 'L':
						*(p++) = UNTIL_ATTR; break;
					case 'c': case 'C':
						*(p++) = BYSECOND_ATTR; break;
					case 'u': case 'U':
						*(p++) = BYHOUR_ATTR; break;
					case 'y': case 'Y':
						*(p++) = BYDAY_ATTR; break;
					case 'e': case 'E':
						*(p++) = BYWEEKNO_ATTR; break;
					default: goto error;
				}
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for PRIORITY node */
		case PRIORITY_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'L': case 'l':
						*(p++) = LESS_ATTR;break;
					case 'G': case 'g':
						*(p++) = GREATER_ATTR;break;
					case 'E': case 'e':
						*(p++) = EQUAL_ATTR;break;
					default: goto error;
				}
				switch (val[0]) {
					case 'e': case 'E':
						*(p++) = EMERGENCY_VAL; break;
					case 'g': case 'G':
						*(p++) = URGENT_VAL;break;
					case 'r': case 'R':
						*(p++) = NORMAL_VAL;break;
					case 'n': case 'N':
						*(p++) = NON_URGENT_VAL;break;
					default: goto error;
				}
			}
			break;
		/* enconding attributes and values for LOCATION node */
		case LOCATION_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'U': case 'u':
						*(p++) = URL_ATTR;
						foo = strlen(val);
						*((unsigned short*)(p)) = (unsigned short)foo;
						p += 2;
						memcpy(p,val,foo);
						p += foo;
						break;
					case 'P': case 'p':
						*(p++) = PRIORITY_ATTR;
						if (val[0]=='0') foo=0;
						else if (val[0]=='1') foo=10;
						else goto error;
						if (val[1]!='.') goto error;
						if (val[2]<'0' || val[2]>'9')
							goto error;
						foo += val[2] - '0';
						if (foo<0 || foo>10)
							goto error;
						*(p++) = (unsigned char)foo;
						break;
					case 'C': case 'c':
						*(p++) = CLEAR_ATTR;
						if (val[0]=='y' || val[0]=='Y')
							*(p++) = YES_VAL;
						else
							*(p++) = NO_VAL;
						break;
					default: goto error;
				}
			}
			break;
		/* enconding attributes and values for LOOKUP node */
		case LOOKUP_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				foo = 0;
				switch(attr->name[0]) {
					case 'S': case 's':
						*(p++) = SOURCE_ATTR;
						foo = 1;
						break;
					case 'T': case 't':
						*(p++) = TIMEOUT_ATTR;
						foo = strtol(val,0,10);
						if (errno||foo<0||foo>255)
							goto error;
						*(p++) = (unsigned char)foo;
						foo = 0;
						break;
					case 'C': case 'c':
						*(p++) = CLEAR_ATTR;
						if (val[0]=='y' || val[0]=='Y')
							*(p++) = YES_VAL;
						else
							*(p++) = NO_VAL;
						foo = 0;
						break;
					case 'U': case 'u':
						*(p++) = USE_ATTR;
						foo = 1;
						break;
					case 'I': case 'i':
						*(p++) = IGNORE_ATTR;
						foo = 1;
						break;
					default: goto error;
				}
				if (foo) {
					foo = strlen(val);
					*((unsigned short*)(p)) = (unsigned short)foo;
					p += 2;
					memcpy(p,val,foo);
					p += foo;
				}
			}
			break;
		/* enconding attributes and values for REMOVE_LOCATION node */
		case REMOVE_LOCATION_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'L': case 'l':
						*(p++) = LOCATION_ATTR;break;
					case 'P': case 'p':
						*(p++) = PARAM_ATTR;break;
					case 'V': case 'v':
						*(p++) = VALUE_ATTR;break;
					default: goto error;
				}
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for REMOVE_LOCATION node */
		case PROXY_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'R': case 'r':
						*(p++) = RECURSE_ATTR;
						if (val[0]=='y' || val[0]=='Y')
							*(p++) = YES_VAL;
						else if (val[0]=='n' || val[0]=='N')
							*(p++) = NO_VAL;
						else goto error;
						break;
					case 'T': case 't':
						*(p++) = TIMEOUT_ATTR;
						foo = strtol(val,0,10);
						if (errno||foo<0||foo>255)
							goto error;
						*(p++) = (unsigned char)foo;
						foo = 0;
						break;
					case 'O': case 'o':
						*(p++) = ORDERING_ATTR;
						switch (val[0]) {
							case 'p': case'P':
								*(p++) = PARALLEL_VAL; break;
							case 'S': case 's':
								*(p++) = SEQUENTIAL_VAL; break;
							case 'F': case 'f':
								*(p++) = FIRSTONLY_VAL; break;
							default: goto error;
						}
						break;
					default: goto error;
				}
			}
			break;
		/* enconding attributes and values for REDIRECT node */
		case REDIRECT_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				if (attr->name[0]=='p' || attr->name[0]=='P') {
					*(p++) = PERMANENT_ATTR;
					if (val[0]=='y' || val[0]=='Y')
						*(p++) = YES_VAL;
					else if (val[0]=='n' || val[0]=='N')
						*(p++) = NO_VAL;
					else goto error;
				} else goto error;
			}
			break;
		/* enconding attributes and values for REJECT node */
		case REJECT_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'R': case 'r':
						*(p++) = REASON_ATTR;
						foo = strlen(val);
						*((unsigned short*)(p)) = (unsigned short)foo;
						p += 2;
						memcpy(p,val,foo);
						p += foo;
						break;
					case 'S': case 's':
						*(p++) = STATUS_ATTR;
						foo = strtol(val,0,10);
						if (errno||foo<0||foo>1000)
							goto error;
						*((unsigned short*)p) = (unsigned short)foo;
						p +=2;
						break;
					default: goto error;
				}
			}
			break;
		/* enconding attributes and values for MAIL node */
		case MAIL_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				if (attr->name[0]=='u' || attr->name[0]=='U') {
					*(p++) = URL_ATTR;
					foo = strlen(val);
					*((unsigned short*)(p)) = (unsigned short)foo;
					p += 2;
					memcpy(p,val,foo);
					p += foo;
				} else goto error;
			}
			break;
		/* enconding attributes and values for LOG node */
		case LOG_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch (attr->name[0] ) {
					case 'n': case 'N':
						*(p++) = NAME_ATTR; break;
					case 'c': case 'C':
						*(p++) = COMMENT_ATTR;break;
					default: goto error;
				}
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for SUBACTION node */
		case SUBACTION_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				if (strcasecmp("id",attr->name)==0 ) {
					if ((list = append_to_list(list, node_ptr,val))==0)
						goto error;
				} else goto error;
			}
			break;
		/* enconding attributes and values for SUB node */
		case SUB_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				if (strcasecmp("ref",attr->name)==0 ) {
					if (( offset = search_the_list(list, val))==0)
						goto error;
					*((unsigned short*)p) = (unsigned short)(node_ptr-offset);
					p += 2;
				} else goto error;
			}
			break;

	}

	*nr_of_attr = nr_attr;
	return (p-ptr);
error:
	LOG(L_ERR,"ERROR:cpl:encript_node_attr: error enconding attributes\n");
	return -1;
}




int encrypt_node( xmlNodePtr node, unsigned char *p)
{
	xmlNodePtr kid;
	int sub_tree_size;
	int attr_size;
	int len;
	int foo;
	int nr;

	/* get node's type (on one byte) */
	NODE_TYPE(p) = encode_node_name((char*)node->name);

	/* counting the kids */
	NR_OF_KIDS(p) = 0;
	for( kid=node->xmlChildrenNode;kid;kid=kid->next)
		NR_OF_KIDS(p)++;

	/* setting the attributes */
	attr_size = encript_node_attr( node, p, NODE_TYPE(p), ATTR_PTR(p), &nr);
	if (attr_size<0) return -1;
	NR_OF_ATTR(p) = (unsigned char)nr;

	sub_tree_size = 2 + 2*(NR_OF_KIDS(p)) + 1 + attr_size;

	/* encrypt all the kids */
	for(kid = node->xmlChildrenNode,foo=0;kid;kid=kid->next,foo++) {
		KID_OFFSET(p,foo) = sub_tree_size;
		len = encrypt_node(kid,p+sub_tree_size);
		if (len<=0) return -1;
		sub_tree_size += len;
	}

	return sub_tree_size;
}




char* encryptXML( char* xml_s, int xml_len, char* DTD_filename, int *bin_len)
{
	static unsigned char buf[2048];
	unsigned int  len;
	xmlValidCtxt  cvp;
	xmlDocPtr  doc;
	xmlNodePtr cur;
	xmlDtdPtr  dtd;

	doc  = 0;
	list = 0;
	dtd  = 0;

	/* parse the xml */
	xml_s[xml_len] = 0;
	doc = xmlParseDoc( xml_s );
	if (!doc) {
		LOG(L_ERR,"ERROR:cpl:encryptXML:CPL script not parsed successfully\n");
		goto error;
	}

	/* parse the dtd file - if any! */
	if (DTD_filename) {
		dtd = xmlParseDTD( NULL, DTD_filename);
		if (!dtd) {
			LOG(L_ERR,"ERROR:cpl:encryptXML: DTD not parsed successfully. \n");
			goto error;
		}
		cvp.userData = (void *) stderr;
		cvp.error    = (xmlValidityErrorFunc) fprintf;
		cvp.warning  = (xmlValidityWarningFunc) fprintf;
		if (xmlValidateDtd(&cvp, doc, dtd)!=1) {
			LOG(L_ERR,"ERROR:cpl:encryptXML: CPL script do not matche DTD\n");
			goto error;
		}
	}


	cur = xmlDocGetRootElement(doc);
	if (!cur) {
		LOG(L_ERR,"ERROR:cpl:encryptXML: empty CPL script!\n");
		goto error;
	}

	len = encrypt_node( cur, buf);
	if (len<0) {
		LOG(L_ERR,"ERROR:cpl:encryptXML: zero lenght return by encripting"
			" function\n");
		goto error;
	}


	if (doc) xmlFreeDoc(doc);
	if (list) delete_list(list);
	if (bin_len) *bin_len = len;
	return buf;
error:
	if (doc) xmlFreeDoc(doc);
	if (list) delete_list(list);
	return 0;
}

