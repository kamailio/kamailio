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

#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../ut.h"
#include "CPL_tree.h"
#include "sub_list.h"



static struct node *list = 0;
static xmlDtdPtr     dtd;   /* DTD file */
static xmlValidCtxt  cvp;   /* validating context */


typedef unsigned short length_type ;
typedef length_type*   length_type_ptr;

enum {EMAIL_TO,EMAIL_HDR_NAME,EMAIL_KNOWN_HDR_BODY,EMAIL_UNKNOWN_HDR_BODY};


#define ENCONDING_BUFFER_SIZE 65536

#define FOR_ALL_ATTR(_node,_attr) \
	for( (_attr)=(_node)->properties ; (_attr) ; (_attr)=(_attr)->next)

/* right and left space triming */
#define trimlr(_s_) \
	do{\
		for(;(_s_).s[(_s_).len-1]==' ';(_s_).s[--(_s_).len]=0);\
		for(;(_s_).s[0]==' ';(_s_).s=(_s_).s+1,(_s_).len--);\
	}while(0);

#define check_overflow(_p_,_offset_,_end_,_error_) \
	do{\
		if ((_p_)+(_offset_)>=(_end_)) { \
			LOG(L_ERR,"ERROR:cpl-c:%s:%d: overflow -> buffer to small\n",\
				__FUNCTION__,__LINE__);\
			goto _error_;\
		}\
	}while(0)\

#define set_attr_type(_p_,_type_,_end_,_error_) \
	do{\
		check_overflow(_p_,sizeof(length_type),_end_,_error_);\
		*((length_type_ptr)(_p_)) = htons((length_type)(_type_));\
		(_p_) += sizeof(length_type);\
	}while(0)\

#define append_short_attr(_p_,_n_,_end_,_error_) \
	do{\
		check_overflow(_p_,sizeof(length_type),_end_,_error_);\
		*((length_type_ptr)(_p_)) = htons((length_type)(_n_));\
		(_p_) += sizeof(length_type);\
	}while(0)

#define append_str_attr(_p_,_s_,_end_,_error_) \
	do{\
		check_overflow(_p_,(_s_).len + 1*((((_s_).len)&0x0001)==1),\
			_end_,_error_);\
		*((length_type_ptr)(_p_)) = htons((length_type)(_s_).len);\
		(_p_) += sizeof(length_type);\
		memcpy( (_p_), (_s_).s, (_s_).len);\
		(_p_) += (_s_).len + 1*((((_s_).len)&0x0001)==1);\
	}while(0)

#define get_attr_val(_attr_name_,_val_,_error_) \
	do { \
		(_val_).s = (char*)xmlGetProp(node,(_attr_name_));\
		(_val_).len = strlen((_val_).s);\
		/* remove all spaces from begin and end */\
		trimlr( (_val_) );\
		if ((_val_).len==0) {\
			LOG(L_ERR,"ERROR:cpl_c:%s:%d: attribute <%s> has an "\
				"empty value\n",__FUNCTION__,__LINE__,(_attr_name_));\
			goto _error_;\
		}\
	}while(0)\


#define MAX_EMAIL_HDR_SIZE   7 /*we are looking only for SUBJECT and BODY ;-)*/
#define MAX_EMAIL_BODY_SIZE    512
#define MAX_EMAIL_SUBJECT_SIZE 32

static inline unsigned char *decode_mail_url(unsigned char *p,
						unsigned char *p_end, char *url, unsigned char *nr_attr)
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
	set_attr_type(p, TO_ATTR, p_end, error); /* attr type */
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
							LOG(L_ERR,"ERROR:cpl_c:decode_mail_url: empty TO "
								"address found in MAIL node!\n");
							goto error;
						}
						if (((*len)&0x0001)==1) p++;
						*len = htons(*len);
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
							set_attr_type( p, BODY_ATTR, p_end, error);
							max_len = MAX_EMAIL_BODY_SIZE;
						} else if ( hdr_len==SUBJECT_EMAILHDR_LEN &&
						strncasecmp(buf,SUBJECT_EMAILHDR_STR,hdr_len)==0 ) {
							/* SUBJECT hdr found */
							set_attr_type( p, SUBJECT_ATTR, p_end, error);
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
						if (((*len)&0x0001)==1) p++;
						*len = htons(*len);
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
						if (((*len)&0x0001)==1) p++;
						*len = htons(*len);
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



#if 0
static inline unsigned char encode_node_name(char *node_name)
{
	switch (node->name[0]) {
		case 'a':
		case 'A':
			switch (node->name[7]) {
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
			switch (node->name[2]) {
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
					if (node->name[8])
						return LANGUAGE_SWITCH_NODE;
					else
						return LANGUAGE_NODE;
			}
		case 'm':
		case 'M':
			return MAIL_NODE;
		case 'n':
		case 'N':
			switch (node->name[3]) {
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
			if (node->name[1]=='t' || node->name[1]=='T')
				return OTHERWISE_NODE;
			else
				return OUTGOING_NODE;
		case 'p':
		case 'P':
			if (node->name[2]=='o' || node->name[2]=='O')
				return PROXY_NODE;
			if (node->name[8])
				return PRIORITY_SWITCH_NODE;
			else
				return PRIORITY_NODE;
		case 'r':
		case 'R':
			switch (node->name[2]) {
				case 'j':
				case 'J':
					return REJECT_NODE;
				case 'm':
				case 'M':
					return REMOVE_LOCATION_NODE;
				default:
					if (node->name[8])
						return REDIRECTION_NODE;
					else
						return REDIRECT_NODE;
			}
		case 's':
		case 'S':
			switch (node->name[3]) {
				case 0:
					return SUB_NODE;
				case 'c':
				case 'C':
					return SUCCESS_NODE;
				case 'a':
				case 'A':
					return SUBACTION_NODE;
				default:
					if (node->name[6])
						return STRING_SWITCH_NODE;
					else
						return STRING_NODE;
			}
		case 't':
		case 'T':
			if (node->name[4])
				return TIME_SWITCH_NODE;
			else
				return TIME_NODE;
	}
	return 0;
}


int encode_node_attr( xmlNodePtr node, unsigned char *node_ptr,
					unsigned int type, unsigned char *ptr,int *nr_of_attr)
{
	xmlAttrPtr     attr;
	struct sip_uri uri;
	char           *val;
	char           *end;
	unsigned char  *p;
	unsigned char  *offset;
	int            nr_attr;
	int            foo;
	str            s;

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
				val = (char*)xmlGetProp(node,attr->name);
				if (attr->name[0]!='M' && attr->name[0]!='m')
					goto error;
				for(end=val,foo=0;;end++) {
					if (!foo && (*end==' ' || *end=='\t')) continue;
					if (nr_attr>=2) goto error;
					if (((*end)|0x20)>='a' && ((*end)|0x20)<='z') {
						foo++; continue;
					} else if (*end=='*' && foo==0 && nr_attr==0 &&
					(*end==' '|| *end=='\t' || *end==0)) {
						foo++;
						*(p++)=MATCHES_TAG_ATTR;
					} else if (foo && nr_attr==0 && *end=='-' ) {
						*(p++)=MATCHES_TAG_ATTR;
					} else if (foo && nr_attr>=0 && nr_attr<=1 &&
					(*end==' '|| *end=='\t' || *end==0)) {
						*(p++)=(!nr_attr)?MATCHES_TAG_ATTR:MATCHES_SUBTAG_ATTR;
					} else goto error;
					nr_attr++;
					/*DBG("----> language tag=%d; %d [%.*s]\n",*(p-1),
						foo,foo,end-foo);*/
					*((unsigned short*)(p)) = (unsigned short)foo;
					p += 2;
					memcpy(p,end-foo,foo);
					p += foo;
					foo = 0;
					if (*end==0) break;
				}
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
						/* check if it's a valid SIP URL -> just call
						 * parse uri function and see if returns error ;-) */
						if (parse_uri( val, foo-1, &uri)!=0) {
							LOG(L_ERR,"ERROR:cpl-c:encript_node_attr: <%s> is "
								"not a valid SIP URL\n",val);
							goto error;
						}
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
						s.s = val;
						s.len = strlen(val);
						/* right and left space triming */
						for(;s.s[s.len-1]==' ';s.s[--s.len]=0);
						for(;s.s[0]==' ';s.s=s.s+1,s.len--);
						if (str2int(&s,&foo)==-1) {
							/*it was a non numeric value */
							if (!strcasecmp(val,BUSY_STR)) {
								foo = BUSY_VAL;
							} else if (!strcasecmp(val,NOTFOUND_STR)) {
								foo = NOTFOUND_VAL;
							} else if (!strcasecmp(val,ERROR_STR)) {
								foo = ERROR_VAL;
							} else if (!strcasecmp(val,REJECT_STR)) {
								foo = REJECT_VAL;
							} else {
								LOG(L_ERR,"ERROR:cpl_c:encode_node_attr: bad "
									"val in reject node for status <%s>\n",val);
								goto error;
							}
						} else if (foo<400 || foo>700) {
							LOG(L_ERR,"ERROR:cpl_c:encode_node_attr: bad "
								"code in reject node for status <%d>\n",foo);
								goto error;
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
					//p = decode_mail_url( p, val, &nr_attr);
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
	LOG(L_ERR,"ERROR:cpl:encode_node_attr: error enconding attributes for "
		"node %d\n",type);
	return -1;
}
#endif




/* Attr. encoding for ADDRESS node:
 *   | attr_t(2) attr_len(2) attr_val(2*x) |  IS/CONTAINS/SUBDOMAIN_OF attr (NT)
 */
inline int encode_address_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		switch (attr->name[0]) {
			case 'i': case 'I':
				set_attr_type(p, IS_ATTR, buf_end, error);
				break;
			case 'c': case 'C':
				set_attr_type(p, CONTAINS_ATTR, buf_end, error);
				break;
			case 's': case 'S':
				set_attr_type(p, SUBDOMAIN_OF_ATTR, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_address_attr: unknown attribute "
					"<%s>\n",attr->name);
				goto error;
		}
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		/* copy also the \0 from the end of string */
		val.len++;
		append_str_attr(p, val, buf_end, error);
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for ADDRESS_SWITCH node:
 *   | attr1_t(2) attr1_val(2) |                FIELD attr
 *  [| attr2_t(2) attr2_val(2) |]?              SUBFILED attr
 */
inline int encode_address_switch_attr(xmlNodePtr node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		switch(attr->name[0]) {
			case 'F': case 'f':
				set_attr_type(p, FIELD_ATTR, buf_end, error);
				if (val.s[0]=='D' || val.s[0]=='d')
					append_short_attr(p, DESTINATION_VAL, buf_end, error);
				else if (val.s[6]=='A' || val.s[6]=='a')
					append_short_attr(p,ORIGINAL_DESTINATION_VAL,buf_end,error);
				else if (!val.s[6])
					append_short_attr(p, ORIGIN_VAL, buf_end, error);
				else {
					LOG(L_ERR,"ERROR:cpl_c:encode_address_switch_attr: unknown"
					" value <%s> for FIELD attr\n",val.s);
					goto error;
				};
				break;
			case 'S': case 's':
				set_attr_type(p, SUBFIELD_ATTR, buf_end, error);
				switch (val.s[0]) {
					case 'u': case 'U':
						append_short_attr(p, USER_VAL, buf_end, error);
						break;
					case 'h': case 'H':
						append_short_attr(p, HOST_VAL, buf_end, error);
						break;
					case 'p': case 'P':
						append_short_attr(p, PORT_VAL, buf_end, error);
						break;
					case 't': case 'T':
						append_short_attr(p, TEL_VAL, buf_end, error);
						break;
					case 'd': case 'D':
						/*append_short_attr(p, DISPLAY_VAL, buf_end, error);
						break;*/  /* NOT YET SUPPORTED BY INTERPRETER */
					case 'a': case 'A':
						/*append_short_attr(p, ADDRESS_TYPE_VAL, buf_end,error);
						break;*/  /* NOT YET SUPPORTED BY INTERPRETER */
					default:
						LOG(L_ERR,"ERROR:cpl_c:encode_address_switch_attr: "
							"unknown value <%s> for SUBFIELD attr\n",val.s);
						goto error;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_address_switch_attr: unknown"
					" attribute <%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for LANGUAGE node:
 *   | attr_t(2) attr_len(2) attr_val(2*x) |              MATCHES attr  (NNT)
 */
inline int encode_lang_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)

{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	unsigned char  *end;
	unsigned char  *val_bk;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		/* there is only one attribute -> MATCHES */
		if (attr->name[0]!='M' && attr->name[0]!='m') {
			LOG(L_ERR,"ERROR:cpl_c:encode_lang_attr: unknown attribute "
				"<%s>\n",attr->name);
			goto error;
		}
		val.s = val_bk = (char*)xmlGetProp(node,attr->name);
		/* parse the language-tag */
		for(end=val.s,val.len=0;;end++) {
			/* trim all spaces from the begining of the tag */
			if (!val.len && (*end==' ' || *end=='\t')) continue;
			/* we cannot have more than 2 attrs - LANG_TAG and LANG_SUBTAG */
			if ((*nr_attr)>=2) goto lang_error;
			if (((*end)|0x20)>='a' && ((*end)|0x20)<='z') {
				val.len++; continue;
			} else if (*end=='*' && val.len==0 && (*nr_attr)==0 &&
			(*end==' '|| *end=='\t' || *end==0)) {
				val.len++;
				set_attr_type(p, MATCHES_TAG_ATTR, buf_end, error);
			} else if (val.len && (*nr_attr)==0 && *end=='-' ) {
				set_attr_type(p, MATCHES_TAG_ATTR, buf_end, error);
			} else if (val.len && ((*nr_attr)==0 || (*nr_attr)==1) &&
			(*end==' '|| *end=='\t' || *end==0)) {
				set_attr_type(p,
					(!(*nr_attr))?MATCHES_TAG_ATTR:MATCHES_SUBTAG_ATTR,
					buf_end, error );
			} else goto lang_error;
			(*nr_attr)++;
			/*DBG("----> language tag=%d; %d [%.*s]\n",*(p-1),
				val.len,val.len,end-val.len);*/
			val.s = end-val.len;
			append_str_attr(p, val, buf_end, error);
			val.len = 0;
			if (*end==0) break;
		}
	}

	return p-p_orig;
lang_error:
	LOG(L_ERR,"ERROR:cpl-c:encode_lang_attr: bad value for language_tag <%s>\n",
		val_bk);
error:
	return -1;
}



/* Attr. encoding for PRIORITY node:
 *   | attr1_t(2) attr1_val(2) |                  LESS/GREATER/EQUAL attr
 *  [| attr2_t(2) attr2_len(2) attr_val(2*x) |]?  PRIOSTR attr (NT)
 */
inline int encode_priority_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* attribute's name */
		switch(attr->name[0]) {
			case 'L': case 'l':
				set_attr_type(p, LESS_ATTR, buf_end, error);
				break;
			case 'G': case 'g':
				set_attr_type(p, GREATER_ATTR, buf_end, error);
				break;
			case 'E': case 'e':
				set_attr_type(p, EQUAL_ATTR, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_priority_attr: unknown attribute "
					"<%s>\n",attr->name);
				goto error;
		}
		/* attribute's encoded value */
		get_attr_val( attr->name , val, error);
		if ( val.len==EMERGENCY_STR_LEN &&
		!strncasecmp(val.s,EMERGENCY_STR,val.len) ) {
			append_short_attr(p, EMERGENCY_VAL, buf_end, error);
		} else if ( val.len==URGENT_STR_LEN &&
		!strncasecmp(val.s,URGENT_STR,val.len) ) {
			append_short_attr(p, URGENT_VAL, buf_end, error);
		} else if ( val.len==NORMAL_STR_LEN &&
		!strncasecmp(val.s,NORMAL_STR,val.len) ) {
			append_short_attr(p, NORMAL_VAL, buf_end, error);
		} else if ( val.len==NON_URGENT_STR_LEN &&
		!strncasecmp(val.s,NON_URGENT_STR,val.len) ) {
			append_short_attr(p, NON_URGENT_VAL, buf_end, error);
		} else {
			append_short_attr(p, UNKNOWN_PRIO_VAL, buf_end, error);
			set_attr_type(p, PRIOSTR_ATTR, buf_end, error);
			val.len++; /* append \0 also */
			append_str_attr(p, val, buf_end, error);
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for STRING_SWITCH node:
 *  [| attr1_t(2) attr1_len(2) attr_val(2*x) |]?  IS attr  (NT)
 *  [| attr2_t(2) attr2_len(2) attr_val(2*x) |]?  CONTAINS attr (NT)
 */
inline int encode_string_switch_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* there is only one attribute -> MATCHES */
		if (attr->name[0]!='F' && attr->name[0]!='f') {
			LOG(L_ERR,"ERROR:cpl_c:encode_string_switch_attr: unknown "
				"attribute <%s>\n",attr->name);
			goto error;
		}
		set_attr_type(p, FIELD_ATTR, buf_end, error);
		/* attribute's encoded value */
		get_attr_val( attr->name , val, error);
		switch (val.s[0]) {
			case 'S': case 's':
				append_short_attr(p, SUBJECT_VAL, buf_end, error);
				break;
			case 'O': case 'o':
				append_short_attr(p, ORGANIZATION_VAL, buf_end, error);
				break;
			case 'U': case 'u':
				append_short_attr(p, USER_AGENT_VAL, buf_end, error);
				break;
			case 'D': case 'd':
				append_short_attr(p, DISPLAY_VAL, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_string_switch_attr: unknown "
					"value <%s> for FIELD\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for STRING node:
 *  [| attr1_t(2) attr1_len(2) attr_val(2*x) |]?  IS attr  (NT)
 *  [| attr2_t(2) attr2_len(2) attr_val(2*x) |]?  CONTAINS attr (NT)
 */
inline int encode_string_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		switch(attr->name[0]) {
			case 'I': case 'i':
				set_attr_type(p, IS_ATTR, buf_end, error);
				break;
			case 'C': case 'c':
				set_attr_type(p, CONTAINS_ATTR, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_string_attr: unknown "
					"attribute <%s>\n",attr->name);
				goto error;
		}
		/* attribute's encoded value */
		get_attr_val( attr->name , val, error);
		val.len++; /* grab also the \0 */
		append_str_attr(p,val, buf_end, error);
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for TIME_SWITCH node:
 *  [| attr1_t(2) attr1_len(2) attr_val(2*x) |]?  TZID attr  (NT)
 *  [| attr2_t(2) attr2_len(2) attr_val(2*x) |]?  TZURL attr (NT)
 */
inline int encode_time_switch_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		switch(attr->name[2]) {
			case 'I': case 'i':
				set_attr_type(p, TZID_ATTR, buf_end, error);
				/* attribute's encoded value */
				get_attr_val( attr->name , val, error);
				val.len++; /* grab also the \0 */
				append_str_attr(p,val, buf_end, error);
				break;
			case 'U': case 'u':
				/* set_attr_type(p, TZURL_ATTR, buf_end, error);
				 * is a waste of space to copy the url - the interpreter doesn't
				 * use it at all ;-) */
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_time_switch_attr: unknown "
					"attribute <%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for TIME node:
 *   | attr1_t(2) attr1_len(2) attr1_val(2*x) |       DSTART attr  (NT)
 *  [| attr2_t(2) attr2_len(2) attr2_val(2*x) |]?     DTEND attr (NT)
 *  [| attr3_t(2) attr3_len(2) attr3_val(2*x) |]?     DURATION attr (NT)
 *  [| attr4_t(2) attr4_len(2) attr4_val(2*x) |]?     FREQ attr (NT)
 *  [| attr5_t(2) attr5_len(2) attr5_val(2*x) |]?     WKST attr (NT)
 *  [| attr6_t(2) attr6_len(2) attr6_val(2*x) |]?     BYYEARDAY attr (NT)
 *  [| attr7_t(2) attr7_len(2) attr7_val(2*x) |]?     COUNT attr (NT)
 *  [| attr8_t(2) attr8_len(2) attr8_val(2*x) |]?     BYSETPOS attr (NT)
 *  [| attr9_t(2) attr9_len(2) attr9_val(2*x) |]?     BYMONTH attr (NT)
 *  [| attr10_t(2) attr10_len(2) attr_val10(2*x) |]?  BYMONTHDAY attr (NT)
 *  [| attr11_t(2) attr11_len(2) attr_val11(2*x) |]?  BYMINUTE attr (NT)
 *  [| attr12_t(2) attr12_len(2) attr_val12(2*x) |]?  INTERVAL attr (NT)
 *  [| attr13_t(2) attr13_len(2) attr_val13(2*x) |]?  UNTIL attr (NT)
 *  [| attr14_t(2) attr14_len(2) attr_val14(2*x) |]?  BYSECOND attr (NT)
 *  [| attr15_t(2) attr15_len(2) attr_val15(2*x) |]?  BYHOUR attr (NT)
 *  [| attr16_t(2) attr16_len(2) attr_val16(2*x) |]?  BYDAY attr (NT)
 *  [| attr17_t(2) attr17_len(2) attr_val17(2*x) |]?  BYWEEKNO attr (NT)
 */
inline int encode_time_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		switch (attr->name[4]) {
			case 0:
				if (attr->name[0]=='F' || attr->name[0]=='f')
					set_attr_type(p, FREQ_ATTR, buf_end, error);
				else if (attr->name[0]=='W' || attr->name[0]=='w')
					set_attr_type(p, WKST_ATTR, buf_end, error);
				break;
			case 'a': case 'A':
				if (attr->name[0]=='D' || attr->name[0]=='d')
					set_attr_type(p, DTSTART_ATTR, buf_end, error);
				else if (attr->name[0]=='B' || attr->name[0]=='b')
					set_attr_type(p, BYYEARDAY_ATTR, buf_end, error);
				break;
			case 't': case 'T':
				if (attr->name[0]=='D' || attr->name[0]=='d')
					set_attr_type(p, DURATION_ATTR, buf_end, error);
				else if (attr->name[0]=='C' || attr->name[0]=='c')
					set_attr_type(p, COUNT_ATTR, buf_end, error);
				else if (attr->name[0]=='B' || attr->name[0]=='b')
					set_attr_type(p, BYSETPOS_ATTR, buf_end, error);
				break;
			case 'n': case 'N':
				if (!attr->name[0])
					set_attr_type(p, BYMONTH_ATTR, buf_end, error);
				else if (attr->name[0]=='D' || attr->name[0]=='d')
					set_attr_type(p, BYMONTHDAY_ATTR, buf_end, error);
				else if (attr->name[0]=='e' || attr->name[0]=='E')
					set_attr_type(p, BYMINUTE_ATTR, buf_end, error);
				break;
			case 'd': case 'D':
				set_attr_type(p, DTEND_ATTR, buf_end, error);
				break;
			case 'r': case 'R':
				set_attr_type(p, INTERVAL_ATTR, buf_end, error);
				break;
			case 'l': case 'L':
				set_attr_type(p, UNTIL_ATTR, buf_end, error);
				break;
			case 'c': case 'C':
				set_attr_type(p, BYSECOND_ATTR, buf_end, error);
				break;
			case 'u': case 'U':
				set_attr_type(p, BYHOUR_ATTR, buf_end, error);
				break;
			case 'y': case 'Y':
				set_attr_type(p, BYDAY_ATTR, buf_end, error);
				break;
			case 'e': case 'E':
				set_attr_type(p, BYWEEKNO_ATTR, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_time_attr: unknown "
					"attribute <%s>\n",attr->name);
				goto error;
		}
		/* attribute's encoded value */
		get_attr_val( attr->name , val, error);
		val.len++; /* grab also the \0 */
		append_str_attr(p,val, buf_end, error);
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for LOCATION node:
 *  | attr1_t(2) attr1_len(2) attr1_val(2*x) |      URL attr  (NT)
 * [| attr2_t(2) attr2_val(2) |]?                   PRIORITY attr
 * [| attr3_t(2) attr3_val(2) |]?                   CLEAR attr
 */
inline int encode_location_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	struct sip_uri uri;
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	unsigned short nr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* get attribute's value */
		get_attr_val( attr->name , val, error);
		switch(attr->name[0]) {
			case 'U': case 'u':
				set_attr_type(p, URL_ATTR, buf_end, error);
				/* check if it's a valid SIP URL -> just call
				 * parse uri function and see if returns error ;-) */
				if (parse_uri( val.s, val.len, &uri)!=0) {
					LOG(L_ERR,"ERROR:cpl-c:encript_location_attr: <%s> is "
						"not a valid SIP URL\n",val.s);
					goto error;
				}
				val.len++; /*copy also the \0 */
				append_str_attr(p,val, buf_end, error);
				break;
			case 'P': case 'p':
				set_attr_type(p, PRIORITY_ATTR, buf_end, error);
				if (val.s[0]=='0') nr=0;
				else if (val.s[0]=='1') nr=10;
				else goto prio_error;
				if (val.s[1]!='.') goto prio_error;
				if (val.s[2]<'0' || val.s[2]>'9') goto prio_error;
				nr += val.s[2] - '0';
				if (nr>10)
					goto prio_error;
				append_short_attr(p, nr, buf_end, error);
				break;
			case 'C': case 'c':
				set_attr_type(p, CLEAR_ATTR, buf_end, error);
				if (val.s[0]=='y' || val.s[0]=='Y')
					append_short_attr(p, YES_VAL, buf_end, error);
				else
					append_short_attr(p, NO_VAL, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_location_attr: unknown attribute "
					"<%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
prio_error:
	LOG(L_ERR,"ERROR:cpl_c:encode_location_attr: invalid priority <%s>\n",
		val.s);
error:
	return -1;
}



/* Attr. encoding for REMOVE_LOCATION node:
 * [| attr1_t(2) attr1_len(2) attr1_val(2*x) |]?    LOCATION attr  (NT)
 */
inline int encode_rmvloc_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	struct sip_uri uri;
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		switch(attr->name[0]) {
			case 'L': case 'l':
				set_attr_type(p, LOCATION_ATTR, buf_end, error);
				/* get the value of the attribute */
				get_attr_val( attr->name , val, error);
				/* check if it's a valid SIP URL -> just call
				 * parse uri function and see if returns error ;-) */
				if (parse_uri( val.s, val.len, &uri)!=0) {
					LOG(L_ERR,"ERROR:cpl-c:encript_rmvloc_attr: <%s> is "
						"not a valid SIP URL\n",val.s);
					goto error;
				}
				val.len++; /*copy also the \0 */
				append_str_attr(p,val, buf_end, error);
				break;
			case 'P': case 'p':
			case 'V': case 'v':
				/* as the interpreter ignors PARAM and VALUE attributes, we will
				 * do the same ;-) */
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_rmvloc_attr: unknown attribute "
					"<%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for PROXY node:
 * [| attr1_t(2) attr1_val(2) |]?                   RECURSE attr
 * [| attr2_t(2) attr2_val(2) |]?                   TIMEOUT attr
 * [| attr3_t(2) attr3_val(2) |]?                   ORDERING attr
 */
inline int encode_proxy_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	unsigned int   nr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		switch(attr->name[0]) {
			case 'R': case 'r':
				set_attr_type(p, RECURSE_ATTR, buf_end, error);
				if (val.s[0]=='y' || val.s[0]=='Y')
					append_short_attr(p, YES_VAL, buf_end, error);
				else if (val.s[0]=='n' || val.s[0]=='N')
					append_short_attr(p, NO_VAL, buf_end, error);
				else {
					LOG(L_ERR,"ERROR:cpl_c:encode_proxy_attr: unknown value "
					"<%s> for attribute RECURSE\n",val.s);
					goto error;
				}
				break;
			case 'T': case 't':
				set_attr_type(p, TIMEOUT_ATTR, buf_end, error);
				if (str2int(&val,&nr)==-1) {
					LOG(L_ERR,"ERROR:cpl_c:encode_proxy_attr: bad value <%.*s>"
						" for attribute TIMEOUT\n",val.len,val.s);
					goto error;
				}
				append_short_attr(p, (unsigned short)nr, buf_end, error);
				break;
			case 'O': case 'o':
				set_attr_type(p, ORDERING_ATTR, buf_end, error);
				switch (val.s[0]) {
					case 'p': case'P':
						append_short_attr(p, PARALLEL_VAL, buf_end, error);
						break;
					case 'S': case 's':
						append_short_attr(p, SEQUENTIAL_VAL, buf_end, error);
						break;
					case 'F': case 'f':
						append_short_attr(p, FIRSTONLY_VAL, buf_end, error);
						break;
					default:
						LOG(L_ERR,"ERROR:cpl_c:encode_proxy_attr: unknown "
							"value <%s> for attribute ORDERING\n",val.s);
						goto error;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_proxy_attr: unknown attribute "
					"<%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for REJECT node:
 *  | attr1_t(2) attr1_val(2) |                      STATUS attr
 * [| attr2_t(2) attr2_len(2) attr2_val(2*x)|]?      REASON attr (NT)
 */
inline int encode_reject_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	unsigned int   nr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		switch(attr->name[0]) {
			case 'R': case 'r':
				set_attr_type(p, REASON_ATTR, buf_end, error);
				val.len++; /* grab also the /0 */
				append_str_attr(p, val, buf_end, error);
				break;
			case 'S': case 's':
				set_attr_type(p, STATUS_ATTR, buf_end, error);
				if (str2int(&val,&nr)==-1) {
					/*it was a non numeric value */
					if (val.len==BUSY_STR_LEN &&
					!strncasecmp(val.s,BUSY_STR,val.len)) {
						append_short_attr(p, BUSY_VAL, buf_end, error);
					} else if (val.len==NOTFOUND_STR_LEN &&
					!strncasecmp(val.s,NOTFOUND_STR,val.len)) {
						append_short_attr(p, NOTFOUND_VAL, buf_end, error);
					} else if (val.len==ERROR_STR_LEN &&
					!strncasecmp(val.s,ERROR_STR,val.len)) {
						append_short_attr(p, ERROR_VAL, buf_end, error);
					} else if (val.len==REJECT_STR_LEN &&
					!strncasecmp(val.s,REJECT_STR,val.len)) {
						append_short_attr(p, REJECT_VAL, buf_end, error);
					} else {
						LOG(L_ERR,"ERROR:cpl_c:encode_priority_attr: bad "
							"val. <%s> for STATUS\n",val.s);
						goto error;
					}
				} else if (nr<400 || nr>700) {
					LOG(L_ERR,"ERROR:cpl_c:encode_priority_attr: bad "
						"code <%d> for STATUS\n",nr);
					goto error;
				} else {
					append_short_attr(p, nr, buf_end, error);
				}
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_priority_attr: unknown attribute "
					"<%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for REDIRECT node:
 *  | attr1_t(2) attr1_val(2) |                      STATUS attr
 * [| attr2_t(2) attr2_len(2) attr2_val(2*x)|]?      REASON attr (NT)
 */
inline int encode_redirect_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		if (attr->name[0]=='p' || attr->name[0]=='P') {
			set_attr_type(p, PERMANENT_ATTR, buf_end, error);
			/* get the value */
			get_attr_val( attr->name , val, error);
			if (val.s[0]=='y' || val.s[0]=='Y')
				append_short_attr( p, YES_VAL, buf_end, error);
			else if (val.s[0]=='n' || val.s[0]=='N')
				append_short_attr( p, NO_VAL, buf_end, error);
			else {
				LOG(L_ERR,"ERROR:cpl_c:encode_redirect_attr: bad "
					"val. <%s> for PERMANENT\n",val.s);
				goto error;
			}
		} else {
			LOG(L_ERR,"ERROR:cpl_c:encode_redirect_attr: unknown attribute "
				"<%s>\n",attr->name);
			goto error;
		}
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for LOG node:
 *  [| attr1_t(2) attr1_len(2) attr1_val(2*x) |]?       NAME attr  (NT)
 *  [| attr2_t(2) attr2_len(2) attr2_val(2*x) |]?       COMMENT attr (NT)
 */
inline int encode_log_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		switch (attr->name[0] ) {
			case 'n': case 'N':
				if (val.len>MAX_NAME_SIZE) val.len=MAX_NAME_SIZE;
				set_attr_type(p, NAME_ATTR, buf_end, error);
				break;
			case 'c': case 'C':
				if (val.len>MAX_COMMENT_SIZE) val.len=MAX_COMMENT_SIZE;
				set_attr_type(p, COMMENT_ATTR, buf_end, error);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:encode_log_attr: unknown attribute "
					"<%s>\n",attr->name);
					goto error;
		}
		/* be sure there is a \0 at the end of string */
		val.s[val.len++]=0;
		append_str_attr(p,val, buf_end, error);
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for MAIL node:
 *   | attr1_t(2) attr1_len(2) attr1_val(2*x) |        TO_ATTR attr  (NNT)
 *  [| attr2_t(2) attr2_len(2) attr2_val(2*x) |]?      SUBJECT_ATTR attr (NNT)
 *  [| attr3_t(2) attr3_len(2) attr3_val(2*x) |]?      BODY_ATTR attr (NNT)
 */
inline int encode_mail_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		/* there is only one attribute -> URL */
		if (attr->name[0]!='u' && attr->name[0]!='U') {
			LOG(L_ERR,"ERROR:cpl_c:encode_node_attr: unknown attribute "
					"<%s>\n",attr->name);
			goto error;
		}
		p = decode_mail_url( p, buf_end,
			(char*)xmlGetProp(node,attr->name), nr_attr);
		if (p==0)
			goto error;
	}

	return p-p_orig;
error:
	return -1;
}



/* Attr. encoding for SUBACTION node:
 */
inline int encode_subaction_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	str            val;

	FOR_ALL_ATTR(node,attr) {
		/* there is only one attribute -> ID */
		if ((attr->name[0]|0x20)=='i' && ((attr->name[1]|0x20)=='d') &&
		attr->name[2]==0 ) {
			/* get the value of the attribute */
			get_attr_val( attr->name , val, error);
			if ((list = append_to_list(list, node_ptr,val.s))==0) {
				LOG(L_ERR,"ERROR:cpl_c:encode_subaction_attr: failed to add "
					"subaction into list -> pkg_malloc failed?\n");
				goto error;
			}
		} else {
			LOG(L_ERR,"ERROR:cpl_c:encode_subaction_attr: unknown attribute "
				"<%s>\n",attr->name);
			goto error;
		}
	}

	return 0;
error:
	return -1;
}



/* Attr. encoding for SUB node:
 *   | attr1_t(2) attr1_val(2) |              REF_ATTR attr
 */
inline int encode_sub_attr(xmlNodePtr  node, unsigned char *node_ptr,
														unsigned char *buf_end)
{
	xmlAttrPtr     attr;
	unsigned char  *p, *p_orig;
	unsigned char  *nr_attr;
	unsigned char  *sub_ptr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* there is only one attribute -> REF */
		if ( strcasecmp("ref",attr->name)!=0 ) {
			LOG(L_ERR,"ERROR:cpl_c:encode_sub_attr: unknown attribute "
				"<%s>\n",attr->name);
			goto error;
		}
		set_attr_type(p, REF_ATTR, buf_end, error);
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		if ( (sub_ptr=search_the_list(list, val.s))==0 ) {
			LOG(L_ERR,"ERROR:cpl_c:encode_sub_attr: unable to find declaration "
				"of subaction <%s>\n",val.s);
			goto error;
		}
		append_short_attr(p,(unsigned short)(node_ptr-sub_ptr),buf_end,error);
	}

	return p-p_orig;
error:
	return -1;
}



/* Returns :  -1 - error
 *            >0 - subtree size of the given node
 */
int encode_node( xmlNodePtr node, unsigned char *p, unsigned char *p_end)
{
	xmlNodePtr kid;
	unsigned short sub_tree_size;
	int attr_size;
	int kid_size;
	int foo;

	/* counting the kids */
	for(kid=node->xmlChildrenNode,foo=0;kid;kid=kid->next,foo++);
	check_overflow(p,GET_NODE_SIZE(foo),p_end,error);
	NR_OF_KIDS(p) = foo;

	/* size of the encoded attributes */
	attr_size = 0;

	/* init the number of attributes */
	NR_OF_ATTR(p) = 0;

	/* encode node name */
	switch (node->name[0]) {
		case 'a':case 'A':
			switch (node->name[7]) {
				case 0:
					NODE_TYPE(p) = ADDRESS_NODE;
					attr_size = encode_address_attr( node, p, p_end);
					break;
				case '-':
					NODE_TYPE(p) = ADDRESS_SWITCH_NODE;
					attr_size = encode_address_switch_attr( node, p, p_end);
					break;
				default:
					NODE_TYPE(p) = ANCILLARY_NODE;
					break;
			}
			break;
		case 'B':case 'b':
			NODE_TYPE(p) = BUSY_NODE;
			break;
		case 'c':case 'C':
			NODE_TYPE(p) = CPL_NODE;
			break;
		case 'd':case 'D':
			NODE_TYPE(p) = DEFAULT_NODE;
			break;
		case 'f':case 'F':
			NODE_TYPE(p) = FAILURE_NODE;
			break;
		case 'i':case 'I':
			NODE_TYPE(p) = INCOMING_NODE;
			break;
		case 'l':case 'L':
			switch (node->name[2]) {
				case 'g':case 'G':
					NODE_TYPE(p) = LOG_NODE;
					attr_size = encode_log_attr( node, p, p_end);
					break;
				case 'o':case 'O':
					NODE_TYPE(p) = LOOKUP_NODE;
					/* we do not encode lookup node because we decide that it's
					 * to unsecure to run it */
					break;
				case 'c':case 'C':
					NODE_TYPE(p) = LOCATION_NODE;
					attr_size = encode_location_attr( node, p, p_end);
					break;
				default:
					if (node->name[8]) {
						NODE_TYPE(p) = LANGUAGE_SWITCH_NODE;
					} else {
						NODE_TYPE(p) = LANGUAGE_NODE;
						attr_size = encode_lang_attr( node, p, p_end);
					}
					break;
			}
			break;
		case 'm':case 'M':
			NODE_TYPE(p) =  MAIL_NODE;
			attr_size = encode_mail_attr( node, p, p_end);
			break;
		case 'n':case 'N':
			switch (node->name[3]) {
				case 'F':case 'f':
					NODE_TYPE(p) = NOTFOUND_NODE;
					break;
				case 'N':case 'n':
					NODE_TYPE(p) = NOANSWER_NODE;
					break;
				default:
					NODE_TYPE(p) = NOT_PRESENT_NODE;
					break;
			}
			break;
		case 'o':case 'O':
			if (node->name[1]=='t' || node->name[1]=='T') {
				NODE_TYPE(p) = OTHERWISE_NODE;
			} else {
				NODE_TYPE(p) = OUTGOING_NODE;
			}
			break;
		case 'p':case 'P':
			if (node->name[2]=='o' || node->name[2]=='O') {
				NODE_TYPE(p) = PROXY_NODE;
				attr_size = encode_proxy_attr( node, p, p_end);
			} else if (node->name[8]) {
				NODE_TYPE(p) = PRIORITY_SWITCH_NODE;
			} else {
				NODE_TYPE(p) = PRIORITY_NODE;
				attr_size = encode_priority_attr( node, p, p_end);
			}
			break;
		case 'r':case 'R':
			switch (node->name[2]) {
				case 'j':case 'J':
					NODE_TYPE(p) = REJECT_NODE;
					attr_size = encode_reject_attr( node, p, p_end);
					break;
				case 'm':case 'M':
					NODE_TYPE(p) = REMOVE_LOCATION_NODE;
					attr_size = encode_rmvloc_attr( node, p, p_end);
					break;
				default:
					if (node->name[8]) {
						NODE_TYPE(p) = REDIRECTION_NODE;
					} else {
						NODE_TYPE(p) = REDIRECT_NODE;
						attr_size = encode_redirect_attr( node, p, p_end);
					}
					break;
			}
			break;
		case 's':case 'S':
			switch (node->name[3]) {
				case 0:
					NODE_TYPE(p) = SUB_NODE;
					attr_size = encode_sub_attr( node, p, p_end);
					break;
				case 'c':case 'C':
					NODE_TYPE(p) = SUCCESS_NODE;
					break;
				case 'a':case 'A':
					NODE_TYPE(p) = SUBACTION_NODE;
					attr_size = encode_subaction_attr( node, p, p_end);
					break;
				default:
					if (node->name[6]) {
						NODE_TYPE(p) = STRING_SWITCH_NODE;
						attr_size = encode_string_switch_attr( node, p, p_end);
					} else {
						NODE_TYPE(p) = STRING_NODE;
						attr_size = encode_string_attr( node, p, p_end);
					}
					break;
			}
			break;
		case 't':case 'T':
			if (node->name[4]) {
				NODE_TYPE(p) = TIME_SWITCH_NODE;
				attr_size = encode_time_switch_attr( node, p, p_end);
			} else {
				NODE_TYPE(p) = TIME_NODE;
				attr_size = encode_time_attr( node, p, p_end);
			}
			break;
		default:
			LOG(L_ERR,"ERROR:cpl-c:encode_node: unknown node <%s>\n",
				node->name);
			goto error;
	}

	/* compute the total length of the node (including attributes) */
	if (attr_size<0)
		goto error;
	sub_tree_size =  SIMPLE_NODE_SIZE(p) + (unsigned short)attr_size;

	/* encrypt all the kids */
	for(kid = node->xmlChildrenNode,foo=0;kid;kid=kid->next,foo++) {
		SET_KID_OFFSET( p, foo, sub_tree_size);
		kid_size = encode_node( kid, p+sub_tree_size, p_end);
		if (kid_size<=0)
			goto error;
		sub_tree_size += (unsigned short)kid_size;
	}

	return sub_tree_size;
error:
	return -1;
}



int encodeCPL( str *xml, str *bin)
{
	static unsigned char buf[ENCONDING_BUFFER_SIZE];
	xmlDocPtr  doc;
	xmlNodePtr cur;

	doc  = 0;
	list = 0;

	/* parse the xml */
	doc = xmlParseDoc( xml->s );
	if (!doc) {
		LOG(L_ERR,"ERROR:cpl:encodeCPL:CPL script not parsed successfully\n");
		goto error;
	}

	/* check the xml against dtd */
	if (xmlValidateDtd(&cvp, doc, dtd)!=1) {
		LOG(L_ERR,"ERROR:cpl-c:encodeCPL: CPL script do not matche DTD\n");
		goto error;
	}

	cur = xmlDocGetRootElement(doc);
	if (!cur) {
		LOG(L_ERR,"ERROR:cpl-c:encodeCPL: empty CPL script!\n");
		goto error;
	}

	bin->len = encode_node( cur, buf, buf+ENCONDING_BUFFER_SIZE);
	if (bin->len<0) {
		LOG(L_ERR,"ERROR:cpl-c:encodeCPL: zero lenght return by encripting"
			" function\n");
		goto error;
	}

	xmlFreeDoc(doc);
	if (list) delete_list(list);
	bin->s = buf;
	/*write_to_file("cpl.dat", bin);  only for debugging */
	return 1;
error:
	if (doc) xmlFreeDoc(doc);
	if (list) delete_list(list);
	return 0;
}



/* loads and parse the dtd file; a validating context is created */
int init_CPL_parser( char* DTD_filename )
{
	dtd = xmlParseDTD( NULL, DTD_filename);
	if (!dtd) {
		LOG(L_ERR,"ERROR:cpl-c:init_CPL_parser: DTD not parsed successfully\n");
		return -1;
	}
	cvp.userData = (void *) stderr;
	cvp.error    = (xmlValidityErrorFunc) fprintf;
	cvp.warning  = (xmlValidityWarningFunc) fprintf;

	return 1;
}

