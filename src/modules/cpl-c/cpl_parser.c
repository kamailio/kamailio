/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "../../parser/parse_uri.h"
#include "../../trim.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../ut.h"
#include "CPL_tree.h"
#include "sub_list.h"
#include "cpl_log.h"



static struct node *list = 0;
static xmlDtdPtr     dtd;     /* DTD file */
static xmlValidCtxt  cvp;     /* validating context */


typedef unsigned short length_type ;
typedef length_type*   length_type_ptr;

enum {EMAIL_TO,EMAIL_HDR_NAME,EMAIL_KNOWN_HDR_BODY,EMAIL_UNKNOWN_HDR_BODY};


#define ENCONDING_BUFFER_SIZE 65536

#define FOR_ALL_ATTR(_node,_attr) \
	for( (_attr)=(_node)->properties ; (_attr) ; (_attr)=(_attr)->next)

#define check_overflow(_p_,_offset_,_end_,_error_) \
	do{\
		if ((_p_)+(_offset_)>=(_end_)) { \
			LM_ERR("%s:%d: overflow -> buffer to small\n",\
				__FILE__,__LINE__);\
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

#define append_double_str_attr(_p_,_s1_,_s2_,_end_,_error_) \
	do{\
		check_overflow(_p_,(_s1_).len + (_s2_).len +\
			1*((((_s2_).len+(_s2_).len)&0x0001)==1), _end_, _error_);\
		*((length_type_ptr)(_p_))=htons((length_type)((_s1_).len)+(_s2_).len);\
		(_p_) += sizeof(length_type);\
		memcpy( (_p_), (_s1_).s, (_s1_).len);\
		(_p_) += (_s1_).len;\
		memcpy( (_p_), (_s2_).s, (_s2_).len);\
		(_p_) += (_s2_).len + 1*((((_s1_).len+(_s2_).len)&0x0001)==1);\
	}while(0)

#define get_attr_val(_attr_name_,_val_,_error_) \
	do { \
		(_val_).s = (char*)xmlGetProp(node,(_attr_name_));\
		(_val_).len = strlen((_val_).s);\
		/* remove all spaces from begin and end */\
		trim_spaces_lr( (_val_) );\
		if ((_val_).len==0) {\
			LM_ERR("%s:%d: attribute <%s> has an "\
				"empty value\n",__FILE__,__LINE__,(_attr_name_));\
			goto _error_;\
		}\
	}while(0)\



#define MAX_EMAIL_HDR_SIZE   7 /*we are looking only for SUBJECT and BODY ;-)*/
#define MAX_EMAIL_BODY_SIZE    512
#define MAX_EMAIL_SUBJECT_SIZE 32

static inline char *decode_mail_url(char *p, char *p_end, char *url,
														unsigned char *nr_attr)
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
				LM_ERR("non-ASCII escaped "
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
							LM_ERR("empty TO "
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
						LM_DBG("hdr [%.*s] found\n",
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
							LM_DBG("unknown hdr -> ignoring\n");
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
							LM_ERR("empty TO "
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
						!strncasecmp(p-(*len),URL_MAILTO_STR,(*len))) {
							LM_DBG("MAILTO: found at"
								" the beginning of TO -> removed\n");
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
	LM_ERR("unexpected char [%c] in state %d"
		" in email url \n",*url,status);
error:
	return 0;
}



/* Attr. encoding for ADDRESS node:
 *   | attr_t(2) attr_len(2) attr_val(2*x) |  IS/CONTAINS/SUBDOMAIN_OF attr (NT)
 */
static inline int encode_address_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
				LM_ERR("unknown attribute "
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
static inline int encode_address_switch_attr(xmlNodePtr node, char *node_ptr,
																char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
					LM_ERR("unknown"
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
						LM_ERR("unknown value <%s> for SUBFIELD attr\n",val.s);
						goto error;
				}
				break;
			default:
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_lang_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)

{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
	unsigned char  *nr_attr;
	char           *end;
	char           *val_bk;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		/* there is only one attribute -> MATCHES */
		if (attr->name[0]!='M' && attr->name[0]!='m') {
			LM_ERR("unknown attribute "
				"<%s>\n",attr->name);
			goto error;
		}
		val.s = val_bk = (char*)xmlGetProp(node,attr->name);
		/* parse the language-tag */
		for(end=val.s,val.len=0;;end++) {
			/* trim all spaces from the beginning of the tag */
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
			/*LM_DBG("----> language tag=%d; %d [%.*s]\n",*(p-1),
				val.len,val.len,end-val.len);*/
			val.s = end-val.len;
			append_str_attr(p, val, buf_end, error);
			val.len = 0;
			if (*end==0) break;
		}
	}

	return p-p_orig;
lang_error:
	LM_ERR("bad value for language_tag <%s>\n",val_bk);
error:
	return -1;
}



/* Attr. encoding for PRIORITY node:
 *   | attr1_t(2) attr1_val(2) |                  LESS/GREATER/EQUAL attr
 *  [| attr2_t(2) attr2_len(2) attr_val(2*x) |]?  PRIOSTR attr (NT)
 */
static inline int encode_priority_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char  *p, *p_orig;
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
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_string_switch_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* there is only one attribute -> MATCHES */
		if (attr->name[0]!='F' && attr->name[0]!='f') {
			LM_ERR("unknown attribute <%s>\n",attr->name);
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
				LM_ERR("unknown "
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
static inline int encode_string_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char  *p, *p_orig;
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
				LM_ERR("unknown "
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
static inline int encode_time_switch_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
{
	static str     tz_str = {"TZ=",3};
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
				append_double_str_attr(p,tz_str,val, buf_end, error);
				break;
			case 'U': case 'u':
				/* set_attr_type(p, TZURL_ATTR, buf_end, error);
				 * is a waste of space to copy the url - the interpreter doesn't
				 * use it at all ;-) */
				break;
			default:
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_time_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
				if (!attr->name[7])
					set_attr_type(p, BYMONTH_ATTR, buf_end, error);
				else if (attr->name[7]=='D' || attr->name[7]=='d')
					set_attr_type(p, BYMONTHDAY_ATTR, buf_end, error);
				else if (attr->name[7]=='e' || attr->name[7]=='E')
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
				LM_ERR("unknown attribute <%s>\n",attr->name);
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



/* Attr. encoding for LOOKUP node:
 *  | attr1_t(2) attr1_len(2) attr1_val(2*x) |      SOURCE attr  (NT)
 * [| attr2_t(2) attr2_val(2) |]?                   CLEAR attr
 */
static inline int encode_lookup_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
	unsigned char  *nr_attr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		/* get attribute's value */
		get_attr_val( attr->name , val, error);
		if ( !strcasecmp((const char*)attr->name,"source") ) {
			/* this param will not be copied, since it has only one value ;-)*/
			if ( val.len!=SOURCE_REG_STR_LEN ||
			strncasecmp( val.s, SOURCE_REG_STR, val.len) ) {
				LM_ERR("unsupported value"
					" <%.*s> in SOURCE param\n",val.len,val.s);
				goto error;
			}
		} else if ( !strcasecmp((const char*)attr->name,"clear") ) {
			(*nr_attr)++;
			set_attr_type(p, CLEAR_ATTR, buf_end, error);
			if ( val.len==3 && !strncasecmp(val.s,"yes",3) )
				append_short_attr(p, YES_VAL, buf_end, error);
			else if ( val.len==2 && !strncasecmp(val.s,"no",2) )
				append_short_attr(p, NO_VAL, buf_end, error);
			else {
				LM_ERR("unknown value "
					"<%.*s> for attribute CLEAR\n",val.len,val.s);
				goto error;
			}
		} else if ( !strcasecmp((const char*)attr->name,"timeout") ) {
			LM_WARN("unsupported param TIMEOUT; skipping\n");
		} else {
			LM_ERR("unknown attribute <%s>\n",attr->name);
			goto error;
		}
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
static inline int encode_location_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
{
	struct sip_uri uri;
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
					LM_ERR("<%s> is not a valid SIP URL\n",val.s);
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
				LM_ERR("unknown attribute <%s>\n",attr->name);
				goto error;
		}
	}

	return p-p_orig;
prio_error:
	LM_ERR("invalid priority <%s>\n",val.s);
error:
	return -1;
}



/* Attr. encoding for REMOVE_LOCATION node:
 * [| attr1_t(2) attr1_len(2) attr1_val(2*x) |]?    LOCATION attr  (NT)
 */
static inline int encode_rmvloc_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	struct sip_uri uri;
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
					LM_ERR("<%s> is not a valid SIP URL\n",val.s);
					goto error;
				}
				val.len++; /*copy also the \0 */
				append_str_attr(p,val, buf_end, error);
				break;
			case 'P': case 'p':
			case 'V': case 'v':
				/* as the interpreter ignores PARAM and VALUE attributes, we will
				 * do the same ;-) */
				break;
			default:
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_proxy_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
					LM_ERR("unknown value "
						"<%s> for attribute RECURSE\n",val.s);
					goto error;
				}
				break;
			case 'T': case 't':
				set_attr_type(p, TIMEOUT_ATTR, buf_end, error);
				if (str2int(&val,&nr)==-1) {
					LM_ERR("bad value <%.*s>"
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
						LM_ERR("unknown "
							"value <%s> for attribute ORDERING\n",val.s);
						goto error;
				}
				break;
			default:
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_reject_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
						LM_ERR("bad val. <%s> for STATUS\n",val.s);
						goto error;
					}
				} else if (nr<400 || nr>700) {
					LM_ERR("bad code <%d> for STATUS\n",nr);
					goto error;
				} else {
					append_short_attr(p, nr, buf_end, error);
				}
				break;
			default:
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_redirect_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
				LM_ERR("bad val. <%s> for PERMANENT\n",val.s);
				goto error;
			}
		} else {
			LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_log_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
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
				LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_mail_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
	unsigned char  *nr_attr;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		/* there is only one attribute -> URL */
		if (attr->name[0]!='u' && attr->name[0]!='U') {
			LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_subaction_attr(xmlNodePtr  node, char *node_ptr,
																char *buf_end)
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
				LM_ERR("failed to add "
					"subaction into list -> pkg_malloc failed?\n");
				goto error;
			}
		} else {
			LM_ERR("unknown attribute <%s>\n",attr->name);
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
static inline int encode_sub_attr(xmlNodePtr  node, char *node_ptr, char *buf_end)
{
	xmlAttrPtr     attr;
	char           *p, *p_orig;
	unsigned char  *nr_attr;
	char           *sub_ptr;
	str            val;

	nr_attr = &(NR_OF_ATTR(node_ptr));
	*nr_attr = 0;
	p = p_orig = ATTR_PTR(node_ptr);

	FOR_ALL_ATTR(node,attr) {
		(*nr_attr)++;
		/* there is only one attribute -> REF */
		if ( strcasecmp("ref",(char*)attr->name)!=0 ) {
			LM_ERR("unknown attribute <%s>\n",attr->name);
			goto error;
		}
		set_attr_type(p, REF_ATTR, buf_end, error);
		/* get the value of the attribute */
		get_attr_val( attr->name , val, error);
		if ( (sub_ptr=search_the_list(list, val.s))==0 ) {
			LM_ERR("unable to find declaration "
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
int encode_node( xmlNodePtr node, char *p, char *p_end)
{
	xmlNodePtr kid;
	unsigned short sub_tree_size;
	int attr_size;
	int kid_size;
	int foo;

	/* counting the kids */
	for(kid=node->children,foo=0;kid;kid=kid->next)
		if (kid->type==XML_ELEMENT_NODE) foo++;
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
					attr_size = encode_lookup_attr( node, p, p_end);
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
			LM_ERR("unknown node <%s>\n",node->name);
			goto error;
	}

	/* compute the total length of the node (including attributes) */
	if (attr_size<0)
		goto error;
	sub_tree_size =  SIMPLE_NODE_SIZE(p) + (unsigned short)attr_size;

	/* encrypt all the kids */
	for(kid = node->children,foo=0;kid;kid=kid->next) {
		if (kid->type!=XML_ELEMENT_NODE) continue;
		SET_KID_OFFSET( p, foo, sub_tree_size);
		kid_size = encode_node( kid, p+sub_tree_size, p_end);
		if (kid_size<=0)
			goto error;
		sub_tree_size += (unsigned short)kid_size;
		foo++;
	}

	return sub_tree_size;
error:
	return -1;
}



#define BAD_XML       "CPL script is not a valid XML document"
#define BAD_XML_LEN   (sizeof(BAD_XML)-1)
#define BAD_CPL       "CPL script doesn't respect CPL grammar"
#define BAD_CPL_LEN   (sizeof(BAD_CPL)-1)
#define NULL_CPL      "Empty CPL script"
#define NULL_CPL_LEN  (sizeof(NULL_CPL)-1)
#define ENC_ERR       "Encoding of the CPL script failed"
#define ENC_ERR_LEN   (sizeof(ENC_ERR)-1)

int encodeCPL( str *xml, str *bin, str *log)
{
	static char buf[ENCONDING_BUFFER_SIZE];
	xmlDocPtr  doc;
	xmlNodePtr cur;

	doc  = 0;
	list = 0;

	/* reset all the logs (if any) to catch some possible err/warn/notice
	 * from the parser/validater/encoder */
	reset_logs();

	/* parse the xml */
	doc = xmlParseDoc( (unsigned char*)xml->s );
	if (!doc) {
		append_log( 1, MSG_ERR BAD_XML LF, MSG_ERR_LEN+BAD_XML_LEN+LF_LEN);
		LM_ERR( BAD_XML "\n");
		goto error;
	}

	/* check the xml against dtd */
	if (xmlValidateDtd(&cvp, doc, dtd)!=1) {
		append_log( 1, MSG_ERR BAD_CPL LF, MSG_ERR_LEN+BAD_CPL_LEN+LF_LEN);
		LM_ERR( BAD_CPL "\n");
		goto error;
	}

	cur = xmlDocGetRootElement(doc);
	if (!cur) {
		append_log( 1, MSG_ERR NULL_CPL LF, MSG_ERR_LEN+NULL_CPL_LEN+LF_LEN);
		LM_ERR( NULL_CPL "\n");
		goto error;
	}

	bin->len = encode_node( cur, buf, buf+ENCONDING_BUFFER_SIZE);
	if (bin->len<0) {
		append_log( 1, MSG_ERR ENC_ERR LF, MSG_ERR_LEN+ENC_ERR_LEN+LF_LEN);
		LM_ERR( ENC_ERR "\n");
		goto error;
	}

	xmlFreeDoc(doc);
	if (list) delete_list(list);
	/* compile the log buffer */
	compile_logs( log );
	bin->s = buf;
	return 1;
error:
	if (doc) xmlFreeDoc(doc);
	if (list) delete_list(list);
	/* compile the log buffer */
	compile_logs( log );
	return 0;
}



/* loads and parse the dtd file; a validating context is created */
int init_CPL_parser( char* DTD_filename )
{
	dtd = xmlParseDTD( NULL, (unsigned char*)DTD_filename);
	if (!dtd) {
		LM_ERR("DTD not parsed successfully\n");
		return -1;
	}
	cvp.userData = (void *) stderr;
	cvp.error    = (xmlValidityErrorFunc) /*err_print*/ fprintf;
	cvp.warning  = (xmlValidityWarningFunc) /*err_print*/ fprintf;

	return 1;
}

