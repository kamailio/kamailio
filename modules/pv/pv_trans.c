/*
 * Copyright (C) 2007 voice-system.ro
 * Copyright (C) 2009 asipto.com
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

/*! \file
 * \brief Support for transformations
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h" 
#include "../../trim.h" 
#include "../../pvapi.h"
#include "../../dset.h"
#include "../../basex.h"

#include "../../parser/parse_param.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_nameaddr.h"

#include "../../lib/kcore/strcommon.h"
#include "../../lib/srutils/shautils.h"
#include "pv_trans.h"


static char _empty_str[] = "";
static str _tr_empty = { _empty_str, 0 };
static str _tr_uri = {0, 0};
static struct sip_uri _tr_parsed_uri;
static param_t* _tr_uri_params = NULL;

/*! transformation buffer size */
#define TR_BUFFER_SIZE 65536
#define TR_BUFFER_SLOTS	4

/*! transformation buffer */
static char **_tr_buffer_list = NULL;

static char *_tr_buffer = NULL;

static int _tr_buffer_idx = 0;

/*!
 *
 */
int tr_init_buffers(void)
{
	int i;

	_tr_buffer_list = (char**)malloc(TR_BUFFER_SLOTS * sizeof(char*));
	if(_tr_buffer_list==NULL)
		return -1;
	for(i=0; i<TR_BUFFER_SLOTS; i++) {
		_tr_buffer_list[i] = (char*)malloc(TR_BUFFER_SIZE);
		if(_tr_buffer_list[i]==NULL)
			return -1;
	}
	return 0;
}

/*!
 *
 */
char *tr_set_crt_buffer(void)
{
	_tr_buffer = _tr_buffer_list[_tr_buffer_idx];
	_tr_buffer_idx = (_tr_buffer_idx + 1) % TR_BUFFER_SLOTS;
	return _tr_buffer;
}

#define tr_string_clone_result do { \
		if(val->rs.len>TR_BUFFER_SIZE-1) { \
			LM_ERR("result is too big\n"); \
			return -1; \
		} \
		strncpy(_tr_buffer, val->rs.s, val->rs.len); \
		val->rs.s = _tr_buffer; \
	} while(0);

/* -- helper functions */

/* Converts a hex character to its integer value */
static char pv_from_hex(char ch)
{
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character */
static char pv_to_hex(char code)
{
	static char hex[] = "0123456789abcdef";
	return hex[code & 15];
}

/*! \brief
 *  URL Encodes a string for use in a HTTP query
 */
static int urlencode_param(str *sin, str *sout)
{
	char *at, *p;

	if (sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL ||
			sin->len<0 || sout->len < 3*sin->len+1)
		return -1;

	at = sout->s;
	p  = sin->s;

	while (p < sin->s+sin->len) {
		if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~')
			*at++ = *p;
		else if (*p == ' ')
			*at++ = '+';
		else
			*at++ = '%', *at++ = pv_to_hex(*p >> 4), *at++ = pv_to_hex(*p & 15);
		p++;
	}

	*at = 0;
	sout->len = at - sout->s;
	LM_DBG("urlencoded string is <%s>\n", sout->s);

	return 0;
}

/* URL Decode a string */
static int urldecode_param(str *sin, str *sout) {
	char *at, *p;

	at = sout->s;
	p  = sin->s;

	while (p < sin->s+sin->len) {
		if (*p == '%') {
			if (p[1] && p[2]) {
				*at++ = pv_from_hex(p[1]) << 4 | pv_from_hex(p[2]);
				p += 2;
			}
		} else if (*p == '+') {
			*at++ = ' ';
		} else {
			*at++ = *p;
		}
		p++;
	}

	*at = 0;
	sout->len = at - sout->s;

	LM_DBG("urldecoded string is <%s>\n", sout->s);
	return 0;
}

/* Encode 7BIT PDU */
static int pdu_7bit_encode(str sin) {
	int i, j;
	unsigned char hex;
	unsigned char nleft;
	unsigned char fill;
	char HexTbl[] = {"0123456789ABCDEF"};

	nleft = 1;
	j = 0;
	for(i = 0; i < sin.len; i++) {
		hex = *(sin.s) >> (nleft - 1);
		fill = *(sin.s+1) << (8-nleft);
		hex = hex | fill;
		_tr_buffer[j++] = HexTbl[hex >> 4];
		_tr_buffer[j++] = HexTbl[hex & 0x0F];
		nleft++;
		if(nleft == 8) {
			sin.s++;
			i++;
			nleft = 1;
		}	   										 	
		sin.s++;
	}
	_tr_buffer[j] = '\0';
	return j;
}

/* Decode 7BIT PDU */
static int pdu_7bit_decode(str sin) {
	int i, j;
	unsigned char nleft = 1;
	unsigned char fill = 0;
	unsigned char oldfill = 0;
	j = 0;
	for(i = 0; i < sin.len; i += 2) {
		_tr_buffer[j] = (unsigned char)pv_from_hex(sin.s[i]) << 4;
		_tr_buffer[j] |= (unsigned char)pv_from_hex(sin.s[i+1]);
		fill = (unsigned char)_tr_buffer[j];
		fill >>= (8 - nleft);
		_tr_buffer[j] <<= (nleft -1 );
		_tr_buffer[j] &= 0x7F;
		_tr_buffer[j] |= oldfill;
		oldfill = fill;
		j++;
		nleft++;
		if(nleft == 8) {
			_tr_buffer[j++] = oldfill;
			nleft = 1;
			oldfill = 0;
		}
	}
	_tr_buffer[j] = '\0';
	return j;	
}

/* -- transformations functions */

/*!
 * \brief Evaluate string transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_string(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	int i, j, max;
	char *p, *s;
	str st, st2;
	pv_value_t v, w;
	time_t t;

	if(val==NULL || val->flags&PV_VAL_NULL)
		return -1;

	tr_set_crt_buffer();

	switch(subtype)
	{
		case TR_S_LEN:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);

			val->flags = PV_TYPE_INT|PV_VAL_INT|PV_VAL_STR;
			val->ri = val->rs.len;
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;
		case TR_S_INT:
			if(!(val->flags&PV_VAL_INT))
			{
				if(str2sint(&val->rs, &val->ri)!=0)
					return -1;
			} else { 
				if(!(val->flags&PV_VAL_STR))
					val->rs.s = int2str(val->ri, &val->rs.len);
			}

			val->flags = PV_TYPE_INT|PV_VAL_INT|PV_VAL_STR;
			break;
		case TR_S_MD5:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);

			compute_md5(_tr_buffer, val->rs.s, val->rs.len);
			_tr_buffer[MD5_LEN] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _tr_buffer;
			val->rs.len = MD5_LEN;
			break;
		case TR_S_SHA256:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			compute_sha256(_tr_buffer, (u_int8_t*)val->rs.s, val->rs.len);
			_tr_buffer[SHA256_DIGEST_STRING_LENGTH -1] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _tr_buffer;
			val->rs.len = SHA256_DIGEST_STRING_LENGTH -1 ;
			break;
		case TR_S_SHA384:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			compute_sha384(_tr_buffer, (u_int8_t*)val->rs.s, val->rs.len);
			_tr_buffer[SHA384_DIGEST_STRING_LENGTH -1] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _tr_buffer;
			val->rs.len = SHA384_DIGEST_STRING_LENGTH -1;
			break;
		case TR_S_SHA512:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			compute_sha512(_tr_buffer, (u_int8_t*)val->rs.s, val->rs.len);
			_tr_buffer[SHA512_DIGEST_STRING_LENGTH -1] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _tr_buffer;
			val->rs.len = SHA512_DIGEST_STRING_LENGTH -1;
			break;
		case TR_S_ENCODEHEXA:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE/2-1)
				return -1;
			j = 0;
			for(i=0; i<val->rs.len; i++)
			{
				_tr_buffer[j++] = fourbits2char[val->rs.s[i] >> 4];
				_tr_buffer[j++] = fourbits2char[val->rs.s[i] & 0xf];
			}
			_tr_buffer[j] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = j;
			break;
		case TR_S_DECODEHEXA:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE*2-1)
				return -1;
			for(i=0; i<val->rs.len/2; i++)
			{
				if(val->rs.s[2*i]>='0'&&val->rs.s[2*i]<='9')
					_tr_buffer[i] = (val->rs.s[2*i]-'0') << 4;
				else if(val->rs.s[2*i]>='a'&&val->rs.s[2*i]<='f')
					_tr_buffer[i] = (val->rs.s[2*i]-'a'+10) << 4;
				else if(val->rs.s[2*i]>='A'&&val->rs.s[2*i]<='F')
					_tr_buffer[i] = (val->rs.s[2*i]-'A'+10) << 4;
				else return -1;

				if(val->rs.s[2*i+1]>='0'&&val->rs.s[2*i+1]<='9')
					_tr_buffer[i] += val->rs.s[2*i+1]-'0';
				else if(val->rs.s[2*i+1]>='a'&&val->rs.s[2*i+1]<='f')
					_tr_buffer[i] += val->rs.s[2*i+1]-'a'+10;
				else if(val->rs.s[2*i+1]>='A'&&val->rs.s[2*i+1]<='F')
					_tr_buffer[i] += val->rs.s[2*i+1]-'A'+10;
				else return -1;
			}
			_tr_buffer[i] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_ENCODE7BIT:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len > (TR_BUFFER_SIZE*7/8) -1)
				return -1;
			i = pdu_7bit_encode(val->rs);
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_DECODE7BIT:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE/2-1)
				return -1;
			i = pdu_7bit_decode(val->rs);
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_ENCODEBASE64:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			i = base64_enc((unsigned char *) val->rs.s, val->rs.len,
					(unsigned char *) _tr_buffer, TR_BUFFER_SIZE-1);
			if (i < 0)
				return -1;
			_tr_buffer[i] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_DECODEBASE64:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			i = base64_dec((unsigned char *) val->rs.s, val->rs.len,
					(unsigned char *) _tr_buffer, TR_BUFFER_SIZE-1);
			if (i < 0 || (i == 0 && val->rs.len > 0))
				return -1;
			_tr_buffer[i] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_ESCAPECOMMON:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE/2-1)
				return -1;
			i = escape_common(_tr_buffer, val->rs.s, val->rs.len);
			_tr_buffer[i] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_UNESCAPECOMMON:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			i = unescape_common(_tr_buffer, val->rs.s, val->rs.len);
			_tr_buffer[i] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = i;
			break;
		case TR_S_ESCAPEUSER:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE/2-1)
				return -1;
			st.s = _tr_buffer;
			st.len = TR_BUFFER_SIZE;
			if (escape_user(&val->rs, &st))
				return -1;
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;
		case TR_S_UNESCAPEUSER:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			st.s = _tr_buffer;
			st.len = TR_BUFFER_SIZE;
			if (unescape_user(&val->rs, &st))
				return -1;
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;
		case TR_S_ESCAPEPARAM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE/2-1)
				return -1;
			st.s = _tr_buffer;
			st.len = TR_BUFFER_SIZE;
			if (escape_param(&val->rs, &st) < 0)
				return -1;
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;
		case TR_S_UNESCAPEPARAM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			st.s = _tr_buffer;
			st.len = TR_BUFFER_SIZE;
			if (unescape_param(&val->rs, &st) < 0)
				return -1;
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;
		case TR_S_SUBSTR:
			if(tp==NULL || tp->next==NULL)
			{
				LM_ERR("substr invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(tp->type==TR_PARAM_NUMBER)
			{
				i = tp->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("substr cannot get p1\n");
					return -1;
				}
				i = v.ri;
			}
			if(tp->next->type==TR_PARAM_NUMBER)
			{
				j = tp->next->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->next->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("substr cannot get p2\n");
					return -1;
				}
				j = v.ri;
			}
			LM_DBG("i=%d j=%d\n", i, j);
			if(j<0)
			{
				LM_ERR("substr negative offset\n");
				return -1;
			}
			val->flags = PV_VAL_STR;
			val->ri = 0;
			if(i>=0)
			{
				if(i>=val->rs.len)
				{
					LM_ERR("substr out of range\n");
					return -1;
				}
				if(i+j>=val->rs.len) j=0;
				if(j==0)
				{ /* to end */
					val->rs.s += i;
					val->rs.len -= i;
					tr_string_clone_result;
					break;
				}
				val->rs.s += i;
				val->rs.len = j;
				break;
			}
			i = -i;
			if(i>val->rs.len)
			{
				LM_ERR("substr out of range\n");
				return -1;
			}
			if(i<j) j=0;
			if(j==0)
			{ /* to end */
				val->rs.s += val->rs.len-i;
				val->rs.len = i;
				tr_string_clone_result;
				break;
			}
			val->rs.s += val->rs.len-i;
			val->rs.len = j;
			tr_string_clone_result;
			break;

		case TR_S_SELECT:
			if(tp==NULL || tp->next==NULL)
			{
				LM_ERR("select invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(tp->type==TR_PARAM_NUMBER)
			{
				i = tp->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("select cannot get p1\n");
					return -1;
				}
				i = v.ri;
			}
			val->flags = PV_VAL_STR;
			val->ri = 0;
			if(i<0)
			{
				s = val->rs.s+val->rs.len-1;
				p = s;
				i = -i;
				i--;
				while(p>=val->rs.s)
				{
					if(*p==tp->next->v.s.s[0])
					{
						if(i==0)
							break;
						s = p-1;
						i--;
					}
					p--;
				}
				if(i==0)
				{
					val->rs.s = p+1;
					val->rs.len = s-p;
				} else {
					val->rs = _tr_empty;
				}
			} else {
				s = val->rs.s;
				p = s;
				while(p<val->rs.s+val->rs.len)
				{
					if(*p==tp->next->v.s.s[0])
					{
						if(i==0)
							break;
						s = p + 1;
						i--;
					}
					p++;
				}
				if(i==0)
				{
					val->rs.s = s;
					val->rs.len = p-s;
				} else {
					val->rs = _tr_empty;
				}
			}
			tr_string_clone_result;
			break;

		case TR_S_TOLOWER:
			if(!(val->flags&PV_VAL_STR))
			{
				val->rs.s = int2str(val->ri, &val->rs.len);
				val->flags |= PV_VAL_STR;
				break;
			}
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			st.s = _tr_buffer;
			st.len = val->rs.len;
			for (i=0; i<st.len; i++)
				st.s[i]=(val->rs.s[i]>='A' && val->rs.s[i]<='Z')
							?('a' + val->rs.s[i] -'A'):val->rs.s[i];
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;

		case TR_S_TOUPPER:
			if(!(val->flags&PV_VAL_STR))
			{
				val->rs.s = int2str(val->ri, &val->rs.len);
				val->flags |= PV_VAL_STR;
				break;
			}
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			st.s = _tr_buffer;
			st.len = val->rs.len;
			for (i=0; i<st.len; i++)
				st.s[i]=(val->rs.s[i]>='a' && val->rs.s[i]<='z')
							?('A' + val->rs.s[i] -'a'):val->rs.s[i];
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;

		case TR_S_STRIP:
		case TR_S_STRIPTAIL:
			if(tp==NULL)
			{
				LM_ERR("strip invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(tp->type==TR_PARAM_NUMBER)
			{
				i = tp->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("select cannot get p1\n");
					return -1;
				}
				i = v.ri;
			}
			val->flags = PV_VAL_STR;
			val->ri = 0;
			if(i<=0)
				break;
			if(i>=val->rs.len)
			{
				_tr_buffer[0] = '\0';
				val->rs.s = _tr_buffer;
				val->rs.len = 0;
				break;
			}
			if(subtype==TR_S_STRIP)
				val->rs.s += i;
			val->rs.len -= i;
			tr_string_clone_result;
			break;


		case TR_S_STRIPTO:
			if(tp==NULL)
			{
				LM_ERR("stripto invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);

			if(tp->type==TR_PARAM_STRING)
			{
				st = tp->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("stripto cannot get p1\n");
					return -1;
				}
				st = v.rs;
			}

			val->flags = PV_VAL_STR;
			val->ri = 0;
			for(i=0; i<val->rs.len; i++)
			{
				if(val->rs.s[i] == st.s[0])
					break;
			}
			if(i>=val->rs.len)
			{
				_tr_buffer[0] = '\0';
				val->rs.s = _tr_buffer;
				val->rs.len = 0;
				break;
			}
			val->rs.s += i;
			val->rs.len -= i;
			tr_string_clone_result;
			break;

		case TR_S_PREFIXES:
		case TR_S_PREFIXES_QUOT:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);

			/* Set maximum prefix length */
			max = val->rs.len;
			if(tp!=NULL) {
				if(tp->type==TR_PARAM_NUMBER) {
					if (tp->v.n > 0 && tp->v.n < max)
						max = tp->v.n;
				} else {
					if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
							|| (!(v.flags&PV_VAL_INT)))
					{
						LM_ERR("prefixes cannot get max\n");
						return -1;
					}
					if (v.ri > 0 && v.ri < max)
						max  = v.ri;
				}
			}

			if(max * (max/2 + (subtype==TR_S_PREFIXES_QUOT ? 1 : 3)) > TR_BUFFER_SIZE-1) {
				LM_ERR("prefixes buffer too short\n");
				return -1;
			}

			j = 0;
			for (i=1; i <= max; i++) {
				if (subtype==TR_S_PREFIXES_QUOT)
					_tr_buffer[j++] = '\'';
				memcpy(&(_tr_buffer[j]), val->rs.s, i);
				j += i;
				if (subtype==TR_S_PREFIXES_QUOT)
					_tr_buffer[j++] = '\'';
				_tr_buffer[j++] = ',';
			}
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			val->rs.len = j-1;
			break;


		case TR_S_REPLACE:
			if(tp==NULL || tp->next==NULL)
			{
				LM_ERR("select invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);

			if(tp->type==TR_PARAM_STRING)
			{
				st = tp->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("replace cannot get p1\n");
					return -1;
				}
				st = v.rs;
			}

			if(tp->next->type==TR_PARAM_STRING)
			{
				st2 = tp->next->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->next->v.data, &w)!=0
						|| (!(w.flags&PV_VAL_STR)) || w.rs.len<=0)
				{
					LM_ERR("replace cannot get p2\n");
					return -1;
				}
				st2 = w.rs;
			}
			
			val->flags = PV_VAL_STR;
			val->ri = 0;

			i = 0;
			j = 0;
			max = val->rs.len - st.len;
			while (i < val->rs.len && j < TR_BUFFER_SIZE) {
				if (i <= max && val->rs.s[i] == st.s[0]
						&& strncmp(val->rs.s+i, st.s, st.len) == 0) {
					strncpy(_tr_buffer+j, st2.s, st2.len);
					i += st.len;
					j += st2.len;
				} else {
					_tr_buffer[j++] = val->rs.s[i++];
				}
			}
			val->rs.s = _tr_buffer;
			val->rs.len = j;
			break;

		case TR_S_TIMEFORMAT:
			if(tp==NULL)
			{
				LM_ERR("timeformat invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_INT) && (str2int(&val->rs,
							(unsigned int*) &val->ri)!=0))
			{
				LM_ERR("value is not numeric\n");
				return -1;
			}
			if(tp->type==TR_PARAM_STRING)
			{
				st = tp->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("timeformat cannot get p1\n");
					return -1;
				}
				st = v.rs;
			}
			s = pkg_malloc(st.len + 1);
			if (s==NULL)
			{
				LM_ERR("no more pkg memory\n");
				return -1;
			}
			memcpy(s, st.s, st.len);
			s[st.len] = '\0';
			t = val->ri;
			val->rs.len = strftime(_tr_buffer, TR_BUFFER_SIZE-1, s,
			                localtime(&t));
			pkg_free(s);
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			break;

		case TR_S_TRIM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-2)
				return -1;
			memcpy(_tr_buffer, val->rs.s, val->rs.len);
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			trim(&val->rs);
			val->rs.s[val->rs.len] = '\0';
			break;

		case TR_S_RTRIM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-2)
				return -1;
			memcpy(_tr_buffer, val->rs.s, val->rs.len);
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			trim_trailing(&val->rs);
			val->rs.s[val->rs.len] = '\0';
			break;

		case TR_S_LTRIM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-2)
				return -1;
			memcpy(_tr_buffer, val->rs.s, val->rs.len);
			val->flags = PV_VAL_STR;
			val->rs.s = _tr_buffer;
			trim_leading(&val->rs);
			val->rs.s[val->rs.len] = '\0';
			break;

		case TR_S_RM:
			if(tp==NULL)
			{
				LM_ERR("invalid parameters\n");
				return -1;
			}
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-2)
				return -1;
			if(tp->type==TR_PARAM_STRING)
			{
				st = tp->v.s;
				if(memchr(st.s, '\\', st.len)) {
					p = pv_get_buffer();
					if(st.len>=pv_get_buffer_size()-1)
						return -1;
					j=0;
					for(i=0; i<st.len-1; i++) {
						if(st.s[i]=='\\') {
							switch(st.s[i+1]) {
								case 'n':
									p[j++] = '\n';
								break;
								case 'r':
									p[j++] = '\r';
								break;
								case 't':
									p[j++] = '\t';
								break;
								case '\\':
									p[j++] = '\\';
								break;
								default:
									p[j++] = st.s[i+1];
							}
							i++;
						} else {
							p[j++] = st.s[i];
						}
					}
					if(i==st.len-1)
						p[j++] = st.s[i];
					p[j] = '\0';
					st.s = p;
					st.len = j;
				}
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("cannot get parameter value\n");
					return -1;
				}
				st = v.rs;
			}
			LM_DBG("removing [%.*s](%d) in [%.*s](%d)\n",
					st.len, st.s, st.len, val->rs.len, val->rs.s, val->rs.len);
			val->flags = PV_VAL_STR;
			val->ri = 0;

			i = 0;
			j = 0;
			max = val->rs.len - st.len;
			while (i < val->rs.len && j < TR_BUFFER_SIZE) {
				if (i <= max && val->rs.s[i] == st.s[0]
						&& strncmp(val->rs.s+i, st.s, st.len) == 0) {
					i += st.len;
				} else {
					_tr_buffer[j++] = val->rs.s[i++];
				}
			}
			val->rs.s = _tr_buffer;
			val->rs.s[j] = '\0';
			val->rs.len = j;
			break;

		case TR_S_URLENCODEPARAM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			st.s = _tr_buffer;
			st.len = TR_BUFFER_SIZE;
			if (urlencode_param(&val->rs, &st) < 0)
				return -1;
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;

		case TR_S_URLDECODEPARAM:
			if(!(val->flags&PV_VAL_STR))
				val->rs.s = int2str(val->ri, &val->rs.len);
			if(val->rs.len>TR_BUFFER_SIZE-1)
				return -1;
			st.s = _tr_buffer;
			st.len = TR_BUFFER_SIZE;
			if (urldecode_param(&val->rs, &st) < 0)
				return -1;
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs = st;
			break;

		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
	return 0;
}


/*!
 * \brief Evaluate URI transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_uri(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	pv_value_t v;
	str sv;
	param_hooks_t phooks;
	param_t *pit=NULL;

	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;

	if(_tr_uri.len==0 || _tr_uri.len!=val->rs.len ||
			strncmp(_tr_uri.s, val->rs.s, val->rs.len)!=0)
	{
		if(val->rs.len>_tr_uri.len)
		{
			if(_tr_uri.s) pkg_free(_tr_uri.s);
			_tr_uri.s = (char*)pkg_malloc((val->rs.len+1)*sizeof(char));
			if(_tr_uri.s==NULL)
			{
				LM_ERR("no more private memory\n");
				if(_tr_uri_params != NULL)
				{
					free_params(_tr_uri_params);
					_tr_uri_params = 0;
				}
				memset(&_tr_uri, 0, sizeof(str));
				memset(&_tr_parsed_uri, 0, sizeof(struct sip_uri));
				return -1;
			}
		}
		_tr_uri.len = val->rs.len;
		memcpy(_tr_uri.s, val->rs.s, val->rs.len);
		_tr_uri.s[_tr_uri.len] = '\0';
		/* reset old values */
		memset(&_tr_parsed_uri, 0, sizeof(struct sip_uri));
		if(_tr_uri_params != NULL)
		{
			free_params(_tr_uri_params);
			_tr_uri_params = 0;
		}
		/* parse uri -- params only when requested */
		if(parse_uri(_tr_uri.s, _tr_uri.len, &_tr_parsed_uri)!=0)
		{
			LM_ERR("invalid uri [%.*s]\n", val->rs.len,
					val->rs.s);
			if(_tr_uri_params != NULL)
			{
				free_params(_tr_uri_params);
				_tr_uri_params = 0;
			}
			pkg_free(_tr_uri.s);
			memset(&_tr_uri, 0, sizeof(str));
			memset(&_tr_parsed_uri, 0, sizeof(struct sip_uri));
			return -1;
		}
	}
	memset(val, 0, sizeof(pv_value_t));
	val->flags = PV_VAL_STR;

	switch(subtype)
	{
		case TR_URI_USER:
			val->rs = (_tr_parsed_uri.user.s)?_tr_parsed_uri.user:_tr_empty;
			break;
		case TR_URI_HOST:
			val->rs = (_tr_parsed_uri.host.s)?_tr_parsed_uri.host:_tr_empty;
			break;
		case TR_URI_PASSWD:
			val->rs = (_tr_parsed_uri.passwd.s)?_tr_parsed_uri.passwd:_tr_empty;
			break;
		case TR_URI_PORT:
			val->flags |= PV_TYPE_INT|PV_VAL_INT;
			val->rs = (_tr_parsed_uri.port.s)?_tr_parsed_uri.port:_tr_empty;
			val->ri = _tr_parsed_uri.port_no;
			break;
		case TR_URI_PARAMS:
			val->rs = (_tr_parsed_uri.sip_params.s)?_tr_parsed_uri.sip_params:_tr_empty;
			break;
		case TR_URI_PARAM:
			if(tp==NULL)
			{
				LM_ERR("param invalid parameters\n");
				return -1;
			}
			if(_tr_parsed_uri.sip_params.len<=0)
			{
				val->rs = _tr_empty;
				val->flags = PV_VAL_STR;
				val->ri = 0;
				break;
			}

			if(_tr_uri_params == NULL)
			{
				sv = _tr_parsed_uri.sip_params;
				if (parse_params(&sv, CLASS_ANY, &phooks, &_tr_uri_params)<0)
					return -1;
			}
			if(tp->type==TR_PARAM_STRING)
			{
				sv = tp->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("param cannot get p1\n");
					return -1;
				}
				sv = v.rs;
			}
			for (pit = _tr_uri_params; pit; pit=pit->next)
			{
				if (pit->name.len==sv.len
						&& strncasecmp(pit->name.s, sv.s, sv.len)==0)
				{
					val->rs = pit->body;
					goto done;
				}
			}
			val->rs = _tr_empty;
			break;
		case TR_URI_HEADERS:
			val->rs = (_tr_parsed_uri.headers.s)?_tr_parsed_uri.headers:
						_tr_empty;
			break;
		case TR_URI_TRANSPORT:
			val->rs = (_tr_parsed_uri.transport_val.s)?
				_tr_parsed_uri.transport_val:_tr_empty;
			break;
		case TR_URI_TTL:
			val->rs = (_tr_parsed_uri.ttl_val.s)?
				_tr_parsed_uri.ttl_val:_tr_empty;
			break;
		case TR_URI_UPARAM:
			val->rs = (_tr_parsed_uri.user_param_val.s)?
				_tr_parsed_uri.user_param_val:_tr_empty;
			break;
		case TR_URI_MADDR:
			val->rs = (_tr_parsed_uri.maddr_val.s)?
				_tr_parsed_uri.maddr_val:_tr_empty;
			break;
		case TR_URI_METHOD:
			val->rs = (_tr_parsed_uri.method_val.s)?
				_tr_parsed_uri.method_val:_tr_empty;
			break;
		case TR_URI_LR:
			val->rs = (_tr_parsed_uri.lr_val.s)?
				_tr_parsed_uri.lr_val:_tr_empty;
			break;
		case TR_URI_R2:
			val->rs = (_tr_parsed_uri.r2_val.s)?
				_tr_parsed_uri.r2_val:_tr_empty;
			break;
		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
done:
	return 0;
}

static str _tr_params_str = {0, 0};
static param_t* _tr_params_list = NULL;
static char _tr_params_separator = ';';


/*!
 * \brief Evaluate parameter transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_paramlist(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	pv_value_t v;
	str sv;
	int n, i;
	char separator = ';';
	param_hooks_t phooks;
	param_t *pit=NULL;

	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;

	if (tp != NULL)
	{
		if (subtype == TR_PL_COUNT)
		{
			if(tp->type != TR_PARAM_STRING || tp->v.s.len != 1)
				return -1;

				separator = tp->v.s.s[0];
		}
		else if (tp->next != NULL)
		{
			if(tp->next->type != TR_PARAM_STRING
					|| tp->next->v.s.len != 1)
				return -1;

			separator = tp->next->v.s.s[0];
		}
	}

	if(_tr_params_str.len==0 || _tr_params_str.len!=val->rs.len ||
			strncmp(_tr_params_str.s, val->rs.s, val->rs.len)!=0 ||
			_tr_params_separator != separator)
	{
		_tr_params_separator = separator;

		if(val->rs.len>_tr_params_str.len)
		{
			if(_tr_params_str.s) pkg_free(_tr_params_str.s);
			_tr_params_str.s = (char*)pkg_malloc((val->rs.len+1)*sizeof(char));
			if(_tr_params_str.s==NULL)
			{
				LM_ERR("no more private memory\n");
				memset(&_tr_params_str, 0, sizeof(str));
				if(_tr_params_list != NULL)
				{
					free_params(_tr_params_list);
					_tr_params_list = 0;
				}
				return -1;
			}
		}
		_tr_params_str.len = val->rs.len;
		memcpy(_tr_params_str.s, val->rs.s, val->rs.len);
		_tr_params_str.s[_tr_params_str.len] = '\0';
		
		/* reset old values */
		if(_tr_params_list != NULL)
		{
			free_params(_tr_params_list);
			_tr_params_list = 0;
		}
		
		/* parse params */
		sv = _tr_params_str;
		if (parse_params2(&sv, CLASS_ANY, &phooks, &_tr_params_list,
					_tr_params_separator)<0)
			return -1;
	}
	
	if(_tr_params_list==NULL)
		return -1;

	memset(val, 0, sizeof(pv_value_t));
	val->flags = PV_VAL_STR;

	switch(subtype)
	{
		case TR_PL_VALUE:
			if(tp==NULL)
			{
				LM_ERR("value invalid parameters\n");
				return -1;
			}

			if(tp->type==TR_PARAM_STRING)
			{
				sv = tp->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("value cannot get p1\n");
					return -1;
				}
				sv = v.rs;
			}
			
			for (pit = _tr_params_list; pit; pit=pit->next)
			{
				if (pit->name.len==sv.len
						&& strncasecmp(pit->name.s, sv.s, sv.len)==0)
				{
					val->rs = pit->body;
					goto done;
				}
			}
			val->rs = _tr_empty;
			break;

		case TR_PL_VALUEAT:
			if(tp==NULL)
			{
				LM_ERR("name invalid parameters\n");
				return -1;
			}

			if(tp->type==TR_PARAM_NUMBER)
			{
				n = tp->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("name cannot get p1\n");
					return -1;
				}
				n = v.ri;
			}
			if(n<0)
			{
				n = -n;
				n--;
				for (pit = _tr_params_list; pit; pit=pit->next)
				{
					if(n==0)
					{
						val->rs = pit->body;
						goto done;
					}
					n--;
				}
			} else {
				/* ugly hack -- params are in reverse order 
				 * - first count then find */
				i = 0;
				for (pit = _tr_params_list; pit; pit=pit->next)
					i++;
				if(n<i)
				{
					n = i - n - 1;
					for (pit = _tr_params_list; pit; pit=pit->next)
					{
						if(n==0)
						{
							val->rs = pit->body;
							goto done;
						}
						n--;
					}
				}
			}
			val->rs = _tr_empty;
			break;

		case TR_PL_NAME:
			if(tp==NULL)
			{
				LM_ERR("name invalid parameters\n");
				return -1;
			}

			if(tp->type==TR_PARAM_NUMBER)
			{
				n = tp->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("name cannot get p1\n");
					return -1;
				}
				n = v.ri;
			}
			if(n<0)
			{
				n = -n;
				n--;
				for (pit = _tr_params_list; pit; pit=pit->next)
				{
					if(n==0)
					{
						val->rs = pit->name;
						goto done;
					}
					n--;
				}
			} else {
				/* ugly hack -- params are in reverse order 
				 * - first count then find */
				i = 0;
				for (pit = _tr_params_list; pit; pit=pit->next)
					i++;
				if(n<i)
				{
					n = i - n - 1;
					for (pit = _tr_params_list; pit; pit=pit->next)
					{
						if(n==0)
						{
							val->rs = pit->name;
							goto done;
						}
						n--;
					}
				}
			}
			val->rs = _tr_empty;
			break;

		case TR_PL_COUNT:
			val->ri = 0;
			for (pit = _tr_params_list; pit; pit=pit->next)
				val->ri++;
			val->flags = PV_TYPE_INT|PV_VAL_INT|PV_VAL_STR;
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;

		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
done:
	return 0;
}

static str _tr_nameaddr_str = {0, 0};
static name_addr_t _tr_nameaddr;


/*!
 * \brief Evaluate name-address transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_nameaddr(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	str sv;
	int ret;

	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;

	if(_tr_nameaddr_str.len==0 || _tr_nameaddr_str.len!=val->rs.len ||
			strncmp(_tr_nameaddr_str.s, val->rs.s, val->rs.len)!=0)
	{
		if(val->rs.len>_tr_nameaddr_str.len)
		{
			if(_tr_nameaddr_str.s)
				pkg_free(_tr_nameaddr_str.s);
			_tr_nameaddr_str.s = (char*)pkg_malloc((val->rs.len+1)*sizeof(char));

			if(_tr_nameaddr_str.s==NULL)
			{
				LM_ERR("no more private memory\n");
				memset(&_tr_nameaddr_str, 0, sizeof(str));
				memset(&_tr_nameaddr, 0, sizeof(name_addr_t));
				return -1;
			}
		}
		_tr_nameaddr_str.len = val->rs.len;
		memcpy(_tr_nameaddr_str.s, val->rs.s, val->rs.len);
		_tr_nameaddr_str.s[_tr_nameaddr_str.len] = '\0';
		
		/* reset old values */
		memset(&_tr_nameaddr, 0, sizeof(name_addr_t));
		
		/* parse params */
		sv = _tr_nameaddr_str;
		ret = parse_nameaddr(&sv, &_tr_nameaddr);
		if (ret < 0) {
			if(ret != -3) return -1;
			/* -3 means no "<" found so treat whole nameaddr as an URI */
			_tr_nameaddr.uri = _tr_nameaddr_str;
			_tr_nameaddr.name = _tr_empty;
			_tr_nameaddr.len = _tr_nameaddr_str.len;
		}
	}
	
	memset(val, 0, sizeof(pv_value_t));
	val->flags = PV_VAL_STR;

	switch(subtype)
	{
		case TR_NA_URI:
			val->rs = (_tr_nameaddr.uri.s)?_tr_nameaddr.uri:_tr_empty;
			break;
		case TR_NA_LEN:
			val->flags = PV_TYPE_INT|PV_VAL_INT|PV_VAL_STR;
			val->ri = _tr_nameaddr.len;
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;
		case TR_NA_NAME:
			val->rs = (_tr_nameaddr.name.s)?_tr_nameaddr.name:_tr_empty;
			break;

		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
	return 0;
}

static str _tr_tobody_str = {0, 0};
static struct to_body _tr_tobody = {0};

/*!
 * \brief Evaluate To-Body transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_tobody(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	str sv;

	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;

	if(_tr_tobody_str.len==0 || _tr_tobody_str.len!=val->rs.len ||
			strncmp(_tr_tobody_str.s, val->rs.s, val->rs.len)!=0)
	{
		if(_tr_tobody_str.s==NULL || val->rs.len>_tr_tobody_str.len)
		{
			if(_tr_tobody_str.s)
				pkg_free(_tr_tobody_str.s);
			_tr_tobody_str.s = (char*)pkg_malloc((val->rs.len+3)*sizeof(char));

			if(_tr_tobody_str.s==NULL)
			{
				LM_ERR("no more private memory\n");
				free_to_params(&_tr_tobody);
				memset(&_tr_tobody, 0, sizeof(struct to_body));
				memset(&_tr_tobody_str, 0, sizeof(str));
				return -1;
			}
		}
		_tr_tobody_str.len = val->rs.len;
		memcpy(_tr_tobody_str.s, val->rs.s, val->rs.len);
		_tr_tobody_str.s[_tr_tobody_str.len] = '\r';
		_tr_tobody_str.s[_tr_tobody_str.len+1] = '\n';
		_tr_tobody_str.s[_tr_tobody_str.len+2] = '\0';
		
		/* reset old values */
		free_to_params(&_tr_tobody);
		memset(&_tr_tobody, 0, sizeof(struct to_body));
		
		/* parse params */
		sv = _tr_tobody_str;
		parse_to(sv.s, sv.s + sv.len + 2, &_tr_tobody);
		if (_tr_tobody.error == PARSE_ERROR)
		{
			free_to_params(&_tr_tobody);
			memset(&_tr_tobody, 0, sizeof(struct to_body));
			pkg_free(_tr_tobody_str.s);
			memset(&_tr_tobody_str, 0, sizeof(str));
			return -1;
		}
		if (parse_uri(_tr_tobody.uri.s, _tr_tobody.uri.len,
				&_tr_tobody.parsed_uri)<0)
		{
			free_to_params(&_tr_tobody);
			memset(&_tr_tobody, 0, sizeof(struct to_body));
			pkg_free(_tr_tobody_str.s);
			memset(&_tr_tobody_str, 0, sizeof(str));
			return -1;
		}
	}
	
	memset(val, 0, sizeof(pv_value_t));
	val->flags = PV_VAL_STR;

	switch(subtype)
	{
		case TR_TOBODY_URI:
			val->rs = (_tr_tobody.uri.s)?_tr_tobody.uri:_tr_empty;
			break;
		case TR_TOBODY_TAG:
			val->rs = (_tr_tobody.tag_value.s)?_tr_tobody.tag_value:_tr_empty;
			break;
		case TR_TOBODY_DISPLAY:
			val->rs = (_tr_tobody.display.s)?_tr_tobody.display:_tr_empty;
			break;
		case TR_TOBODY_URI_USER:
			val->rs = (_tr_tobody.parsed_uri.user.s)
							?_tr_tobody.parsed_uri.user:_tr_empty;
			break;
		case TR_TOBODY_URI_HOST:
			val->rs = (_tr_tobody.parsed_uri.host.s)
							?_tr_tobody.parsed_uri.host:_tr_empty;
			break;
		case TR_TOBODY_PARAMS:
			if(_tr_tobody.param_lst!=NULL)
			{
				val->rs.s = _tr_tobody.param_lst->name.s;
				val->rs.len = _tr_tobody_str.s + _tr_tobody_str.len
								- val->rs.s;
			} else val->rs = _tr_empty;
			break;

		default:
			LM_ERR("unknown subtype %d\n", subtype);
			return -1;
	}
	return 0;
}

void *memfindrchr(const void *buf, int c, size_t n)
{
	int i;
	unsigned char *p;

	p = (unsigned char*)buf;

	for (i=n-1; i>=0; i--) {
		if (p[i] == (unsigned char)c) {
			return (void*)(p+i);
		}
	}
	return NULL;
}

/*!
 * \brief Evaluate line transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_line(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	pv_value_t v;
	str sv;
	str mv;
	char *p;
	int n, i;

	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;

	switch(subtype)
	{
		case TR_LINE_SW:
			if(tp==NULL)
			{
				LM_ERR("value invalid parameters\n");
				return -1;
			}

			if(tp->type==TR_PARAM_STRING)
			{
				sv = tp->v.s;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("value cannot get p1\n");
					return -1;
				}
				sv = v.rs;
			}

			if(val->rs.len < sv.len)
			{
				val->rs = _tr_empty;
				goto done;
			}
			p = val->rs.s;
			do {
				if(strncmp(p, sv.s, sv.len)==0)
				{
					/* match */
					mv.s = p;
					p += sv.len;
					p = memchr(p, '\n', (val->rs.s + val->rs.len) - p);
					if(p==NULL)
					{
						/* last line */
						mv.len = (val->rs.s + val->rs.len) - mv.s;
					} else {
						mv.len = p - mv.s;
					}
					val->rs = mv;
					goto done;
				}
				p = memchr(p, '\n', (val->rs.s + val->rs.len) - p);
			} while(p && ((++p)<=val->rs.s+val->rs.len-sv.len));
			val->rs = _tr_empty;
			break;

		case TR_LINE_AT:
			if(tp==NULL)
			{
				LM_ERR("name invalid parameters\n");
				return -1;
			}

			if(tp->type==TR_PARAM_NUMBER)
			{
				n = tp->v.n;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_INT)))
				{
					LM_ERR("name cannot get p1\n");
					return -1;
				}
				n = v.ri;
			}
			if(n<0)
			{
				p = val->rs.s + val->rs.len - 1;
				if(*p=='\n')
					p--;
				mv.s = p;
				n = -n;
				i=1;
				p = memfindrchr(val->rs.s, '\n', p - val->rs.s);
				if(p!=NULL)
					p--;
				while(i<n && p)
				{
					mv.s = p;
					p = memfindrchr(val->rs.s, '\n', p - val->rs.s);
					if(p!=NULL)
						p--;
					i++;
				}
				if(i==n)
				{
					if(p==NULL)
					{
						/* first line */
						mv.len = mv.s - val->rs.s + 1;
						mv.s = val->rs.s;
					} else {
						mv.len = mv.s - p - 1;
						mv.s = p + 2;
					}
					val->rs = mv;
					goto done;
				}
			} else {
				p = val->rs.s;
				i = 0;
				while(i<n && p)
				{
					p = memchr(p, '\n', (val->rs.s + val->rs.len) - p);
					if(p!=NULL)
						p++;
					i++;
				}
				if(i==n && p!=NULL)
				{
					/* line found */
					mv.s = p;
					p = memchr(p, '\n', (val->rs.s + val->rs.len) - p);
					if(p==NULL)
					{
						/* last line */
						mv.len = (val->rs.s + val->rs.len) - mv.s;
					} else {
						mv.len = p - mv.s;
					}
					val->rs = mv;
					goto done;
				}
			}
			val->rs = _tr_empty;
			break;

		case TR_LINE_COUNT:
			n=0;
			for(i=0; i<val->rs.len; i++)
				if(val->rs.s[i]=='\n')
					n++;
			if(n==0 && val->rs.len>0)
				n = 1;
			val->flags = PV_TYPE_INT|PV_VAL_INT|PV_VAL_STR;
			val->ri = n;
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;

			break;

		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
done:
	if(val->rs.len>0)
	{
		/* skip ending '\r' if present */
		if(val->rs.s[val->rs.len-1]=='\r')
			val->rs.len--;
	}
	val->flags = PV_VAL_STR;
	return 0;
}


#define _tr_parse_nparam(_p, _p0, _tp, _spec, _n, _sign, _in, _s) \
	while(is_in_str(_p, _in) && (*_p==' ' || *_p=='\t' || *_p=='\n')) _p++; \
	if(*_p==PV_MARKER) \
	{ /* pseudo-variable */ \
		_spec = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t)); \
		if(_spec==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		_s.s = _p; _s.len = _in->s + _in->len - _p; \
		_p0 = pv_parse_spec(&_s, _spec); \
		if(_p0==NULL) \
		{ \
			LM_ERR("invalid spec in substr transformation: %.*s!\n", \
				_in->len, _in->s); \
			goto error; \
		} \
		_p = _p0; \
		_tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_SPEC; \
		_tp->v.data = (void*)_spec; \
	} else { \
		if(*_p=='+' || *_p=='-' || (*_p>='0' && *_p<='9')) \
		{ /* number */ \
			_sign = 1; \
			if(*_p=='-') { \
				_p++; \
				_sign = -1; \
			} else if(*_p=='+') _p++; \
			_n = 0; \
			while(is_in_str(_p, _in) && (*_p==' ' || *_p=='\t' || *_p=='\n')) \
					_p++; \
			while(is_in_str(_p, _in) && *_p>='0' && *_p<='9') \
			{ \
				_n = _n*10 + *_p - '0'; \
				_p++; \
			} \
			_tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t)); \
			if(_tp==NULL) \
			{ \
				LM_ERR("no more private memory!\n"); \
				goto error; \
			} \
			memset(_tp, 0, sizeof(tr_param_t)); \
			_tp->type = TR_PARAM_NUMBER; \
			_tp->v.n = sign*n; \
		} else { \
			LM_ERR("tinvalid param in transformation: %.*s!!\n", \
				_in->len, _in->s); \
			goto error; \
		} \
	}

#define _tr_parse_sparam(_p, _p0, _tp, _spec, _ps, _in, _s) \
	while(is_in_str(_p, _in) && (*_p==' ' || *_p=='\t' || *_p=='\n')) _p++; \
	if(*_p==PV_MARKER) \
	{ /* pseudo-variable */ \
		_spec = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t)); \
		if(_spec==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		_s.s = _p; _s.len = _in->s + _in->len - _p; \
		_p0 = pv_parse_spec(&_s, _spec); \
		if(_p0==NULL) \
		{ \
			LM_ERR("invalid spec in substr transformation: %.*s!\n", \
				_in->len, _in->s); \
			goto error; \
		} \
		_p = _p0; \
		_tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_SPEC; \
		_tp->v.data = (void*)_spec; \
	} else { /* string */ \
		_ps = _p; \
		while(is_in_str(_p, _in) && *_p!='\t' && *_p!='\n' \
				&& *_p!=TR_PARAM_MARKER && *_p!=TR_RBRACKET) \
				_p++; \
		if(*_p=='\0') \
		{ \
			LM_ERR("invalid param in transformation: %.*s!!\n", \
				_in->len, _in->s); \
			goto error; \
		} \
		_tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_STRING; \
		_tp->v.s.s = _ps; \
		_tp->v.s.len = _p - _ps; \
	}


/*!
 * \brief Helper fuction to parse a string transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* tr_parse_string(str* in, trans_t *t)
{
	char *p;
	char *p0;
	char *ps;
	str name;
	str s;
	pv_spec_t *spec = NULL;
	int n;
	int sign;
	tr_param_t *tp = NULL;

	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_STRING;
	t->trf = tr_eval_string;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==3 && strncasecmp(name.s, "len", 3)==0)
	{
		t->subtype = TR_S_LEN;
		goto done;
	} else if(name.len==3 && strncasecmp(name.s, "int", 3)==0) {
		t->subtype = TR_S_INT;
		goto done;
	} else if(name.len==3 && strncasecmp(name.s, "md5", 3)==0) {
		t->subtype = TR_S_MD5;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "sha256", 6)==0) {
		t->subtype = TR_S_SHA256;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "sha384", 6)==0) {
		t->subtype = TR_S_SHA384;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "sha512", 6)==0) {
		t->subtype = TR_S_SHA512;
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "tolower", 7)==0) {
		t->subtype = TR_S_TOLOWER;
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "toupper", 7)==0) {
		t->subtype = TR_S_TOUPPER;
		goto done;
	} else if(name.len==11 && strncasecmp(name.s, "encode.hexa", 11)==0) {
		t->subtype = TR_S_ENCODEHEXA;
		goto done;
	} else if(name.len==11 && strncasecmp(name.s, "decode.hexa", 11)==0) {
		t->subtype = TR_S_DECODEHEXA;
		goto done;
	} else if(name.len==11 && strncasecmp(name.s, "encode.7bit", 11)==0) {
		t->subtype = TR_S_ENCODE7BIT;
		goto done;
	} else if(name.len==11 && strncasecmp(name.s, "decode.7bit", 11)==0) {
		t->subtype = TR_S_DECODE7BIT;
		goto done;
	} else if(name.len==13 && strncasecmp(name.s, "encode.base64", 13)==0) {
		t->subtype = TR_S_ENCODEBASE64;
		goto done;
	} else if(name.len==13 && strncasecmp(name.s, "decode.base64", 13)==0) {
		t->subtype = TR_S_DECODEBASE64;
		goto done;
	} else if(name.len==13 && strncasecmp(name.s, "escape.common", 13)==0) {
		t->subtype = TR_S_ESCAPECOMMON;
		goto done;
	} else if(name.len==15 && strncasecmp(name.s, "unescape.common", 15)==0) {
		t->subtype = TR_S_UNESCAPECOMMON;
		goto done;
	} else if(name.len==11 && strncasecmp(name.s, "escape.user", 11)==0) {
		t->subtype = TR_S_ESCAPEUSER;
		goto done;
	} else if(name.len==13 && strncasecmp(name.s, "unescape.user", 13)==0) {
		t->subtype = TR_S_UNESCAPEUSER;
		goto done;
	} else if(name.len==12 && strncasecmp(name.s, "escape.param", 12)==0) {
		t->subtype = TR_S_ESCAPEPARAM;
		goto done;
	} else if(name.len==14 && strncasecmp(name.s, "unescape.param", 14)==0) {
		t->subtype = TR_S_UNESCAPEPARAM;
		goto done;
	} else if(name.len==8 && strncasecmp(name.s, "prefixes", 8)==0) {
		t->subtype = TR_S_PREFIXES;
		if(*p!=TR_PARAM_MARKER)
			goto done;
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid prefixes transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==15 && strncasecmp(name.s, "prefixes.quoted", 15)==0) {
		t->subtype = TR_S_PREFIXES_QUOT;
		if(*p!=TR_PARAM_MARKER)
			goto done;
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid prefixes transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "substr", 6)==0) {
		t->subtype = TR_S_SUBSTR;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid substr transformation: %.*s!\n", in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid substr transformation: %.*s!\n",
				in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		if(tp->type==TR_PARAM_NUMBER && tp->v.n<0)
		{
			LM_ERR("substr negative offset\n");
			goto error;
		}
		t->params->next = tp;
		tp = 0;
		while(is_in_str(p, in) && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid substr transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "select", 6)==0) {
		t->subtype = TR_S_SELECT;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid select transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_PARAM_MARKER || *(p+1)=='\0')
		{
			LM_ERR("invalid select transformation: %.*s!\n", in->len, in->s);
			goto error;
		}
		p++;
		tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t));
		if(tp==NULL)
		{
			LM_ERR("no more private memory!\n");
			goto error;
		}
		memset(tp, 0, sizeof(tr_param_t));
		tp->type = TR_PARAM_STRING;
		tp->v.s.s = p;
		tp->v.s.len = 1;
		t->params->next = tp;
		tp = 0;
		p++;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid select transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "strip", 5)==0) {
		t->subtype = TR_S_STRIP;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid strip transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid strip transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==9 && strncasecmp(name.s, "striptail", 9)==0) {
		t->subtype = TR_S_STRIPTAIL;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid striptail transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid striptail transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "stripto", 7)==0) {
		t->subtype = TR_S_STRIPTO;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid stripto transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid strip transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "ftime", 5)==0) {
		t->subtype = TR_S_TIMEFORMAT;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid ftime transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid ftime transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "replace", 7)==0) {
		t->subtype = TR_S_REPLACE;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid replace transformation: %.*s!\n", in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid replace transformation: %.*s!\n",
				in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params->next = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid replace transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "trim", 4)==0) {
		t->subtype = TR_S_TRIM;
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "rtrim", 5)==0) {
		t->subtype = TR_S_RTRIM;
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "ltrim", 5)==0) {
		t->subtype = TR_S_LTRIM;
		goto done;
	} else if(name.len==2 && strncasecmp(name.s, "rm", 2)==0) {
		t->subtype = TR_S_RM;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid ftime transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid ftime transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==15 && strncasecmp(name.s, "urlencode.param", 15)==0) {
		t->subtype = TR_S_URLENCODEPARAM;
		goto done;
	} else if(name.len==15 && strncasecmp(name.s, "urldecode.param", 15)==0) {
		t->subtype = TR_S_URLDECODEPARAM;
		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	if(tp)
		tr_param_free(tp);
	if(spec)
		pv_spec_free(spec);
	return NULL;
done:
	t->name = name;
	return p;
}


/*!
 * \brief Helper fuction to parse a URI transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* tr_parse_uri(str* in, trans_t *t)
{
	char *p;
	char *p0;
	char *ps;
	str name;
	str s;
	pv_spec_t *spec = NULL;
	tr_param_t *tp = NULL;

	if(in==NULL || in->s==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_URI;
	t->trf = tr_eval_uri;

	/* find next token */
	while(*p && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n", in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==4 && strncasecmp(name.s, "user", 4)==0)
	{
		t->subtype = TR_URI_USER;
		goto done;
	} else if((name.len==4 && strncasecmp(name.s, "host", 4)==0)
			|| (name.len==6 && strncasecmp(name.s, "domain", 6)==0)) {
		t->subtype = TR_URI_HOST;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "passwd", 6)==0) {
		t->subtype = TR_URI_PASSWD;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "port", 4)==0) {
		t->subtype = TR_URI_PORT;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "params", 6)==0) {
		t->subtype = TR_URI_PARAMS;
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "headers", 7)==0) {
		t->subtype = TR_URI_HEADERS;
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "param", 5)==0) {
		t->subtype = TR_URI_PARAM;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid param transformation: %.*s\n", in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid param transformation: %.*s!\n", in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==9 && strncasecmp(name.s, "transport", 9)==0) {
		t->subtype = TR_URI_TRANSPORT;
		goto done;
	} else if(name.len==3 && strncasecmp(name.s, "ttl", 3)==0) {
		t->subtype = TR_URI_TTL;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "uparam", 6)==0) {
		t->subtype = TR_URI_UPARAM;
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "maddr", 5)==0) {
		t->subtype = TR_URI_MADDR;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "method", 6)==0) {
		t->subtype = TR_URI_METHOD;
		goto done;
	} else if(name.len==2 && strncasecmp(name.s, "lr", 2)==0) {
		t->subtype = TR_URI_LR;
		goto done;
	} else if(name.len==2 && strncasecmp(name.s, "r2", 2)==0) {
		t->subtype = TR_URI_R2;
		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s!\n", in->len,
			in->s, name.len, name.s);
error:
	if(tp)
		tr_param_free(tp);
	if(spec)
		pv_spec_free(spec);
	return NULL;
done:
	t->name = name;
	return p;
}


/*!
 * \brief Helper fuction to parse a parameter transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* tr_parse_paramlist(str* in, trans_t *t)
{
	char *p;
	char *p0;
	char *ps;
	char *start_pos;
	str s;
	str name;
	int n;
	int sign;
	pv_spec_t *spec = NULL;
	tr_param_t *tp = NULL;

	if(in==NULL || in->s==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_PARAMLIST;
	t->trf = tr_eval_paramlist;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==5 && strncasecmp(name.s, "value", 5)==0)
	{
		t->subtype = TR_PL_VALUE;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid value transformation: %.*s\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;

		if(*p==TR_PARAM_MARKER)
		{
			start_pos = ++p;
			_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
			t->params->next = tp;
			tp = 0;
			if (p - start_pos != 1)
			{
				LM_ERR("invalid separator in transformation: "
					"%.*s\n", in->len, in->s);
				goto error;
			}
			while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		}

		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid value transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "valueat", 7)==0) {
		t->subtype = TR_PL_VALUEAT;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid name transformation: %.*s\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s)
		t->params = tp;
		tp = 0;
		while(is_in_str(p, in) && (*p==' ' || *p=='\t' || *p=='\n')) p++;

		if(*p==TR_PARAM_MARKER)
		{
			start_pos = ++p;
			_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
			t->params->next = tp;
			tp = 0;
			if (p - start_pos != 1)
			{
				LM_ERR("invalid separator in transformation: "
					"%.*s\n", in->len, in->s);
				goto error;
			}
			while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		}

		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid name transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "name", 4)==0) {
		t->subtype = TR_PL_NAME;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid name transformation: %.*s\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s)
		t->params = tp;
		tp = 0;
		while(is_in_str(p, in) && (*p==' ' || *p=='\t' || *p=='\n')) p++;

		if(*p==TR_PARAM_MARKER)
		{
			start_pos = ++p;
			_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
			t->params->next = tp;
			tp = 0;
			if (p - start_pos != 1)
			{
				LM_ERR("invalid separator in transformation: "
					"%.*s\n", in->len, in->s);
				goto error;
			}
			while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		}

		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid name transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "count", 5)==0) {
		t->subtype = TR_PL_COUNT;
		if(*p==TR_PARAM_MARKER)
		{
			start_pos = ++p;
			_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
			t->params = tp;
			tp = 0;
			if (p - start_pos != 1)
			{
				LM_ERR("invalid separator in transformation: "
					"%.*s\n", in->len, in->s);
				goto error;
			}

			while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
			if(*p!=TR_RBRACKET)
			{
				LM_ERR("invalid name transformation: %.*s!\n",
					in->len, in->s);
				goto error;
			}
		}

		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s!\n",
			in->len, in->s, name.len, name.s);
error:
	if(tp)
		tr_param_free(tp);
	if(spec)
		pv_spec_free(spec);
	return NULL;
done:
	t->name = name;
	return p;
}


/*!
 * \brief Helper fuction to parse a name-address transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* tr_parse_nameaddr(str* in, trans_t *t)
{
	char *p;
	str name;

	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_NAMEADDR;
	t->trf = tr_eval_nameaddr;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==3 && strncasecmp(name.s, "uri", 3)==0)
	{
		t->subtype = TR_NA_URI;
		goto done;
	} else if(name.len==3 && strncasecmp(name.s, "len", 3)==0)
	{
		t->subtype = TR_NA_LEN;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "name", 4)==0) {
		t->subtype = TR_NA_NAME;
		goto done;
	}


	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	return NULL;
done:
	t->name = name;
	return p;
}

/*!
 * \brief Helper fuction to parse a name-address transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* tr_parse_tobody(str* in, trans_t *t)
{
	char *p;
	str name;

	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_TOBODY;
	t->trf = tr_eval_tobody;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==3 && strncasecmp(name.s, "uri", 3)==0)
	{
		t->subtype = TR_TOBODY_URI;
		goto done;
	} else if(name.len==3 && strncasecmp(name.s, "tag", 3)==0) {
		t->subtype = TR_TOBODY_TAG;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "user", 4)==0) {
		t->subtype = TR_TOBODY_URI_USER;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "host", 4)==0) {
		t->subtype = TR_TOBODY_URI_HOST;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "params", 6)==0) {
		t->subtype = TR_TOBODY_PARAMS;
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "display", 7)==0) {
		t->subtype = TR_TOBODY_DISPLAY;
		goto done;
	}


	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	return NULL;
done:
	t->name = name;
	return p;
}

/*!
 * \brief Helper fuction to parse a line transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* tr_parse_line(str* in, trans_t *t)
{
	char *p;
	char *p0;
	char *ps;
	str s;
	str name;
	int n;
	int sign;
	pv_spec_t *spec = NULL;
	tr_param_t *tp = NULL;


	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_LINE;
	t->trf = tr_eval_line;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==2 && strncasecmp(name.s, "at", 2)==0)
	{
		t->subtype = TR_LINE_AT;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid name transformation: %.*s\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_nparam(p, p0, tp, spec, n, sign, in, s)
		t->params = tp;
		tp = 0;
		while(is_in_str(p, in) && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid name transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}

		goto done;
	} else if(name.len==2 && strncasecmp(name.s, "sw", 2)==0) {
		t->subtype = TR_LINE_SW;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid value transformation: %.*s\n",
					in->len, in->s);
			goto error;
		}
		p++;
		_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid value transformation: %.*s!\n",
					in->len, in->s);
			goto error;
		}
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "count", 5)==0) {
		t->subtype = TR_LINE_COUNT;
		goto done;
	}


	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	return NULL;
done:
	t->name = name;
	return p;
}
