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
#include <gnome-xml/xmlmemory.h>
#include <gnome-xml/parser.h>

#include "../../dprint.h"
#include "../../str.h"
#include "../../ut.h"
#include "CPL_tree.h"
#include "sub_list.h"



struct node *list = 0;


#define FOR_ALL_ATTR(_node,_attr) \
	for( (_attr)=(_node)->properties ; (_attr) ; (_attr)=(_attr)->next)

enum {EMAIL_TO,EMAIL_HDR_NAME,EMAIL_KNOWN_HDR_BODY,EMAIL_UNKNOWN_HDR_BODY};


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
			switch (node_name[3]) {
				case 'F':
				case 'f':
					return NOTFOUND_NODE;
				case 'N':
				case 'n':
					return NOANSWER_NODE;
				default:
					return NOT_PRESENT_NODE;
			}
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



#define MAX_EMAIL_HDR_SIZE   7 /*we are looking only for SUBJECT and BODY ;-)*/
#define MAX_EMAIL_BODY_SIZE    512
#define MAX_EMAIL_SUBJECT_SIZE 32

static inline unsigned char *decode_mail_url(unsigned char *p, char *url,
																int *nr_attr)
{
	static char buf[ MAX_EMAIL_HDR_SIZE ];
	char c;
	char foo;
	unsigned short hdr_len;
	unsigned short *len;
	int max_len;
	int status;

	/* init */
	hdr_len = 0;
	max_len = 0;
	status = EMAIL_TO;
	(*nr_attr) ++;
	*(p++) = TO_ATTR; /* attr type */
	len = ((unsigned short*)(p));  /* attr val's len */
	*len = 0; /* init the len */
	p += 2;

	/* parse the whole url */
	do {
		/* extract a char from the encoded url */
		if (*url=='+') {
			/* substitute a blank for a plus */
			c=' ';
			url++;
		/* Look for a hex encoded character */
		} else if ( (*url=='%') && *(url+1) && *(url+2) ) {
			/* hex encoded - convert to a char */
			c = hex2int(url[1]);
			foo = hex2int(url[2]);
			if (c==-1 || foo==-1) {
				LOG(L_ERR, "ERROR:cpl_c:decode_mail_url: non-ASCII escaped "
					"character in mail url [%.*s]\n", 3, url);
				goto error;
			}
			c = c<<4 | foo;
			url += 3;
		} else {
			/* normal character - just copy it without changing */
			c = *url;
			url++;
		}

		/* finally we got a character !! */
		switch (c) {
			case '?':
				switch (status) {
					case EMAIL_TO:
						if (*len==0) {
							LOG(L_ERR,"ERROR:cpl_c:cpl_parser: empty TO "
								"address found in MAIL node!\n");
							goto error;
						}
						hdr_len = 0;
						status = EMAIL_HDR_NAME;
						break;
					default: goto parse_error;
				}
				break;
			case '=':
				switch (status) {
					case EMAIL_HDR_NAME:
						DBG("DEBUG:cpl_c:decode_mail_url: hdr [%.*s] found\n",
							hdr_len,buf);
						if ( hdr_len==BODY_EMAILHDR_LEN &&
						strncasecmp(buf,BODY_EMAILHDR_STR,hdr_len)==0 ) {
							/* BODY hdr found */
							*(p++) = BODY_ATTR; /* attr type */
							max_len = MAX_EMAIL_BODY_SIZE;
						} else if ( hdr_len==SUBJECT_EMAILHDR_LEN &&
						strncasecmp(buf,SUBJECT_EMAILHDR_STR,hdr_len)==0 ) {
							/* SUBJECT hdr found */
							*(p++) = SUBJECT_ATTR; /* attr type */
							max_len = MAX_EMAIL_SUBJECT_SIZE;
						} else {
							DBG("DEBUG:cpl_c:decode_mail_url: unknown hdr ->"
								" ignoring\n");
							status = EMAIL_UNKNOWN_HDR_BODY;
							break;
						}
						(*nr_attr) ++;
						len = ((unsigned short*)(p));  /* attr val's len */
						*len = 0; /* init the len */
						p += 2;
						status = EMAIL_KNOWN_HDR_BODY;
						break;
					default: goto parse_error;
				}
				break;
			case '&':
				switch (status) {
					case EMAIL_KNOWN_HDR_BODY:
					case EMAIL_UNKNOWN_HDR_BODY:
						hdr_len = 0;
						status = EMAIL_HDR_NAME;
						break;
					default: goto parse_error;
				}
				break;
			case 0:
				switch (status) {
					case EMAIL_TO:
						if (*len==0) {
							LOG(L_ERR,"ERROR:cpl_c:decode_mail_url: empty TO "
								"address found in MAIL node!\n");
							goto error;
						}
					case EMAIL_KNOWN_HDR_BODY:
					case EMAIL_UNKNOWN_HDR_BODY:
						break;
					default: goto parse_error;
				}
				break;
			default:
				switch (status) {
					case EMAIL_TO:
						(*len)++;
						*(p++) = c;
						if (*len==URL_MAILTO_LEN &&
						!strncasecmp(p-(*len),URL_MAILTO_STR,(*len)) ) {
							DBG("DEBUG:cpl_c:decode_mail_url: MAILTO: found at"
								" the begining of TO -> removed\n");
							p -= (*len);
							*len = 0;
						}
						break;
					case EMAIL_KNOWN_HDR_BODY:
						if ((*len)<max_len) (*len)++;
						*(p++) = c;
						break;
					case EMAIL_HDR_NAME:
						if (hdr_len<MAX_EMAIL_HDR_SIZE) hdr_len++;
						buf[hdr_len-1] = c;
						break;
					case EMAIL_UNKNOWN_HDR_BODY:
						/* do nothing */
						break;
					default : goto parse_error;
				}
		}
	}while(c!=0);

	return p;
parse_error:
	LOG(L_ERR,"ERROR:cpl_c:decode_mail_url: unexpected char [%c] in state %d"
		" in email url \n",*url,status);
error:
	return 0;
}



int encript_node_attr( xmlNodePtr node, unsigned char *node_ptr,
					unsigned int type, unsigned char *ptr,int *nr_of_attr)
{
	xmlAttrPtr    attr;
	char          *val;
	char          *end;
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
		/* enconding attributes and values for ADDRESS-SWITCH node */
		case ADDRESS_SWITCH_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				switch(attr->name[0]) {
					case 'F': case 'f':
						*(p++) = FIELD_ATTR;
						if (val[0]=='D' || val[0]=='d')
							*(p++) = DESTINATION_VAL;
						else if (val[6]=='A' || val[6]=='a')
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
		/* enconding attributes and values for ADDRESS node */
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
				foo = strlen(val)+1; /* copy also the /0 from the end */
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
				/* attribute's name */
				switch(attr->name[0]) {
					case 'L': case 'l':
						*(p++) = LESS_ATTR;break;
					case 'G': case 'g':
						*(p++) = GREATER_ATTR;break;
					case 'E': case 'e':
						*(p++) = EQUAL_ATTR;break;
					default: goto error;
				}
				/* attribute's encoded value */
				if ( !strcasecmp(val,EMERGENCY_STR) ) {
					*(p++) = EMERGENCY_VAL;
				} else if ( !strcasecmp(val,URGENT_STR) ) {
					*(p++) = URGENT_VAL;
				} else if ( !strcasecmp(val,NORMAL_STR) ) {
					*(p++) = NORMAL_VAL;
				} else if ( !strcasecmp(val,NON_URGENT_STR) ) {
					*(p++) = NON_URGENT_VAL;
				} else {
					*(p++) = UNKNOWN_PRIO_VAL;
				}
				/* attributte's string value */
				foo = strlen(val);
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
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
						foo = strlen(val) + 1; /*copy also the \0 */
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
						foo = strlen(val)+1; /* grab also the /0 */
						*((unsigned short*)(p)) = (unsigned short)foo;
						p += 2;
						memcpy(p,val,foo);
						p += foo;
						break;
					case 'S': case 's':
						*(p++) = STATUS_ATTR;
						for(foo=strlen(val);val[foo-1]==' ';val[--foo]=0);
						foo = strtol(val,&end,10);
						if (errno||foo<0||foo>1000)
							goto error;
						if (*end!=0) {
							/*it was a non numeric value */
							if (!strcasecmp(val,BUSY_STR)) {
								foo = BUSY_VAL;
							} else if (!strcasecmp(val,NOTFOUND_STR)) {
								foo = NOTFOUND_VAL;
							} else if (!strcasecmp(val,ERROR_STR)) {
								foo = ERROR_VAL;
							} else foo = REJECT_VAL;
						} else if (foo/100!=4 && foo/100!=5 && foo/100!=6) {
							foo = REJECT_VAL;
						}
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
				val = (char*)xmlGetProp(node,attr->name);
				if (attr->name[0]=='u' || attr->name[0]=='U') {
					p = decode_mail_url( p, val, &nr_attr);
					if (p==0)
						goto error;
				} else goto error;
			}
			break;
		/* enconding attributes and values for LOG node */
		case LOG_NODE:
			FOR_ALL_ATTR(node,attr) {
				nr_attr++;
				val = (char*)xmlGetProp(node,attr->name);
				foo = strlen(val);
				switch (attr->name[0] ) {
					case 'n': case 'N':
						if (foo>MAX_NAME_SIZE) foo=MAX_NAME_SIZE;
						*(p++) = NAME_ATTR; break;
					case 'c': case 'C':
						if (foo>MAX_COMMENT_SIZE) foo=MAX_COMMENT_SIZE;
						*(p++) = COMMENT_ATTR;break;
					default: goto error;
				}
				*((unsigned short*)(p)) = (unsigned short)foo;
				p += 2;
				memcpy(p,val,foo);
				p += foo;
			}
			break;
		/* enconding attributes and values for SUBACTION node */
		case SUBACTION_NODE:
			FOR_ALL_ATTR(node,attr) {
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
					*(p++) = REF_ATTR;
					*((unsigned short*)p) = (unsigned short)(node_ptr-offset);
					p += 2;
				} else goto error;
			}
			break;

	}

	*nr_of_attr = nr_attr;
	return (p-ptr);
error:
	LOG(L_ERR,"ERROR:cpl:encript_node_attr: error enconding attributes for "
		"node %d\n",type);
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




int encodeXML( str *xml, char* DTD_filename, str *bin)
{
	static unsigned char buf[2048];
	xmlValidCtxt  cvp;
	xmlDocPtr  doc;
	xmlNodePtr cur;
	xmlDtdPtr  dtd;

	doc  = 0;
	list = 0;
	dtd  = 0;

	/* parse the xml */
	xml->s[xml->len] = 0;
	doc = xmlParseDoc( xml->s );
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

	bin->len = encrypt_node( cur, buf);
	if (bin->len<0) {
		LOG(L_ERR,"ERROR:cpl:encryptXML: zero lenght return by encripting"
			" function\n");
		goto error;
	}


	if (doc) xmlFreeDoc(doc);
	if (list) delete_list(list);
	bin->s = buf;
	return 1;
error:
	if (doc) xmlFreeDoc(doc);
	if (list) delete_list(list);
	return 0;
}

