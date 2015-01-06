/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include "string.h"
#include "ctype.h"

#include "../../dprint.h"
#include "../../str.h"
#include "../../mem/mem.h"

#include "auth_hdr.h"
#include "auth.h"


#define AUTHENTICATE_MD5         (1<<0)
#define AUTHENTICATE_MD5SESS     (1<<1)
#define AUTHENTICATE_STALE       (1<<2)

#define AUTHENTICATE_DIGEST_S    "Digest"
#define AUTHENTICATE_DIGEST_LEN  (sizeof(AUTHENTICATE_DIGEST_S)-1)

#define LOWER1B(_n) \
	((_n)|0x20)
#define LOWER4B(_n) \
	((_n)|0x20202020)
#define GET4B(_p) \
	((*(_p)<<24) + (*(_p+1)<<16) + (*(_p+2)<<8) + *(_p+3))
#define GET3B(_p) \
	((*(_p)<<24) + (*(_p+1)<<16) + (*(_p+2)<<8) + 0xff)

#define CASE_5B(_hex4,_c5, _new_state, _quoted) \
	case _hex4: \
		if (p+5<end && LOWER1B(*(p+4))==_c5 ) \
		{ \
			p+=5; \
			state = _new_state; \
			quoted_val = _quoted; \
		} else { \
			p+=4; \
		} \
		break;

#define CASE_6B(_hex4,_c5,_c6, _new_state, _quoted) \
	case _hex4: \
		if (p+6<end && LOWER1B(*(p+4))==_c5 && LOWER1B(*(p+5))==_c6) \
		{ \
			p+=6; \
			state = _new_state; \
			quoted_val = _quoted; \
		} else { \
			p+=4; \
		} \
		break;

#define OTHER_STATE      0
#define QOP_STATE        1
#define REALM_STATE      2
#define NONCE_STATE      3
#define STALE_STATE      4
#define DOMAIN_STATE     5
#define OPAQUE_STATE     6
#define ALGORITHM_STATE  7



int parse_authenticate_body( str *body, struct authenticate_body *auth)
{
	char *p;
	char *end;
	int  n;
	int state;
	str name;
	str val;
	int quoted_val;

	if (body->s==0 || *body->s==0 )
	{
		LM_ERR("empty body\n");
		goto error;
	}

	memset( auth, 0, sizeof(struct authenticate_body));
	p = body->s;
	end = body->s + body->len;

	/* parse the "digest" */
	while (p<end && isspace((int)*p)) p++;
	if (p+AUTHENTICATE_DIGEST_LEN>=end )
		goto parse_error;
	if (strncasecmp(p,AUTHENTICATE_DIGEST_S,AUTHENTICATE_DIGEST_LEN)!=0)
		goto parse_error;
	p += AUTHENTICATE_DIGEST_LEN;
	if (!isspace((int)*p))
		goto parse_error;
	p++;
	while (p<end && isspace((int)*p)) p++;
	if (p==end)
		goto parse_error;

	while (p<end)
	{
		state = OTHER_STATE;
		quoted_val = 0;
		/* get name */
		name.s = p;
		if (p+4<end)
		{
			n = LOWER4B( GET4B(p) );
			switch(n)
			{
				CASE_5B( 0x7265616c, 'm', REALM_STATE, 1); /*realm*/
				CASE_5B( 0x6e6f6e63, 'e', NONCE_STATE, 1); /*nonce*/
				CASE_5B( 0x7374616c, 'e', STALE_STATE, 0); /*stale*/
				CASE_6B( 0x646f6d62, 'i', 'n', DOMAIN_STATE, 1); /*domain*/
				CASE_6B( 0x6f706171, 'u', 'e', OPAQUE_STATE, 1); /*opaque*/
				case 0x616c676f: /*algo*/
					if (p+9<end && LOWER4B(GET4B(p+4))==0x72697468
						&& LOWER1B(*(p+8))=='m' )
					{
						p+=9;
						state = ALGORITHM_STATE;
					} else {
						p+=4;
					}
					break;
				default:
					if ((n|0xff)==0x716f70ff) /*qop*/
					{
						state = QOP_STATE;
						p+=3;
					}
			}
		} else if (p+3<end) {
			n = LOWER4B( GET3B(p) );
			if (n==0x716f70ff) /*qop*/
			{
				p+=3;
				state = QOP_STATE;
			}
		}

		/* parse to the "=" */
		for( n=0 ; p<end&&!isspace((int)*p)&&*p!='=' ; n++,p++  );
		if (p==end)
			goto parse_error;
		if (n!=0)
			state = OTHER_STATE;
		name.len = p-name.s;
		/* get the '=' */
		while (p<end && isspace((int)*p)) p++;
		if (p==end || *p!='=')
			goto parse_error;
		p++;
		/* get the value (quoted or not) */
		while (p<end && isspace((int)*p)) p++;
		if (p+1>=end || (quoted_val && *p!='\"'))
			goto parse_error;
		if (!quoted_val && *p=='\"')
			quoted_val = 1;
		if (quoted_val)
		{
			val.s = ++p;
			while (p<end && *p!='\"')
				p++;
			if (p==end)
				goto error;
		} else {
			val.s = p;
			while (p<end && !isspace((int)*p) && *p!=',')
				p++;
		}
		val.len = p - val.s;
		if (val.len==0)
			val.s = 0;
		/* consume the closing '"' if quoted */
		p += quoted_val;
		while (p<end && isspace((int)*p)) p++;
		if (p<end && *p==',')
		{
			p++;
			while (p<end && isspace((int)*p)) p++;
		}

		LM_DBG("<%.*s>=\"%.*s\" state=%d\n",
			name.len,name.s,val.len,val.s,state);

		/* process the AVP */
		switch (state)
		{
			case QOP_STATE:
				auth->qop = val;
				if(val.len>=4 && !strncmp(val.s, "auth", 4))
					auth->flags |= QOP_AUTH;
				break;
			case REALM_STATE:
				auth->realm = val;
				break;
			case NONCE_STATE:
				auth->nonce = val;
				break;
			case DOMAIN_STATE:
				auth->domain = val;
				break;
			case OPAQUE_STATE:
				auth->opaque = val;
				break;
			case ALGORITHM_STATE:
				if (val.len==3)
				{
					if ( LOWER4B(GET3B(val.s))==0x6d6435ff) /*MD5*/
						auth->flags |= AUTHENTICATE_MD5;
				} else {
					LM_ERR("unsupported algorithm \"%.*s\"\n",val.len,val.s);
					goto error;
				}
				break;
			case STALE_STATE:
				if (val.len==4 && LOWER4B(GET4B(val.s))==0x74727565) /*true*/
				{
						auth->flags |= AUTHENTICATE_STALE;
				} else if ( !(val.len==5 && LOWER1B(val.s[4])=='e' && 
					LOWER4B(GET4B(val.s))==0x66616c73) )
				{
					LM_ERR("unsupported stale value \"%.*s\"\n",val.len,val.s);
					goto error;
				}
				break;
			default:
				break;
		}
	}

	/* some checkings */
	if (auth->nonce.s==0 || auth->realm.s==0)
	{
		LM_ERR("realm or nonce missing\n");
		goto error;
	}

	return 0;
parse_error:
		LM_ERR("parse error in <%.*s> around %ld\n", body->len, body->s, (long)(p-body->s));
error:
	return -1;
}


#define AUTHORIZATION_HDR_START       "Authorization: Digest "
#define AUTHORIZATION_HDR_START_LEN   (sizeof(AUTHORIZATION_HDR_START)-1)

#define PROXY_AUTHORIZATION_HDR_START      "Proxy-Authorization: Digest "
#define PROXY_AUTHORIZATION_HDR_START_LEN  \
	(sizeof(PROXY_AUTHORIZATION_HDR_START)-1)

#define USERNAME_FIELD_S         "username=\""
#define USERNAME_FIELD_LEN       (sizeof(USERNAME_FIELD_S)-1)
#define REALM_FIELD_S            "realm=\""
#define REALM_FIELD_LEN          (sizeof(REALM_FIELD_S)-1)
#define NONCE_FIELD_S            "nonce=\""
#define NONCE_FIELD_LEN          (sizeof(NONCE_FIELD_S)-1)
#define URI_FIELD_S              "uri=\""
#define URI_FIELD_LEN            (sizeof(URI_FIELD_S)-1)
#define OPAQUE_FIELD_S           "opaque=\""
#define OPAQUE_FIELD_LEN         (sizeof(OPAQUE_FIELD_S)-1)
#define RESPONSE_FIELD_S         "response=\""
#define RESPONSE_FIELD_LEN       (sizeof(RESPONSE_FIELD_S)-1)
#define ALGORITHM_FIELD_S        "algorithm=MD5"
#define ALGORITHM_FIELD_LEN       (sizeof(ALGORITHM_FIELD_S)-1)
#define FIELD_SEPARATOR_S        "\", "
#define FIELD_SEPARATOR_LEN      (sizeof(FIELD_SEPARATOR_S)-1)
#define FIELD_SEPARATOR_UQ_S     ", "
#define FIELD_SEPARATOR_UQ_LEN   (sizeof(FIELD_SEPARATOR_UQ_S)-1)

#define QOP_FIELD_S              "qop="
#define QOP_FIELD_LEN            (sizeof(QOP_FIELD_S)-1)
#define NC_FIELD_S               "nc="
#define NC_FIELD_LEN             (sizeof(NC_FIELD_S)-1)
#define CNONCE_FIELD_S           "cnonce=\""
#define CNONCE_FIELD_LEN         (sizeof(CNONCE_FIELD_S)-1)

#define add_string( _p, _s, _l) \
	do {\
		memcpy( _p, _s, _l);\
		_p += _l; \
	}while(0)


str* build_authorization_hdr(int code, str *uri, 
		struct uac_credential *crd, struct authenticate_body *auth,
		char *response)
{
	static str _uac_auth_hdr;
	char *p;
	int len;
	int response_len;

	response_len = strlen(response);

	/* compile then len */
	len = (code==401?
		AUTHORIZATION_HDR_START_LEN:PROXY_AUTHORIZATION_HDR_START_LEN) +
		USERNAME_FIELD_LEN + crd->user.len + FIELD_SEPARATOR_LEN +
		REALM_FIELD_LEN + crd->realm.len + FIELD_SEPARATOR_LEN +
		NONCE_FIELD_LEN + auth->nonce.len + FIELD_SEPARATOR_LEN +
		URI_FIELD_LEN + uri->len + FIELD_SEPARATOR_LEN +
		(auth->opaque.len?
			(OPAQUE_FIELD_LEN + auth->opaque.len + FIELD_SEPARATOR_LEN):0) +
		RESPONSE_FIELD_LEN + response_len + FIELD_SEPARATOR_LEN +
		ALGORITHM_FIELD_LEN + CRLF_LEN;
	if((auth->flags&QOP_AUTH) || (auth->flags&QOP_AUTH_INT))
		len += QOP_FIELD_LEN + 4 /*auth*/ + FIELD_SEPARATOR_UQ_LEN +
				NC_FIELD_LEN + auth->nc->len + FIELD_SEPARATOR_UQ_LEN +
				CNONCE_FIELD_LEN + auth->cnonce->len + FIELD_SEPARATOR_LEN;

	_uac_auth_hdr.s = (char*)pkg_malloc( len + 1);
	if (_uac_auth_hdr.s==0)
	{
		LM_ERR("no more pkg mem\n");
		goto error;
	}

	p = _uac_auth_hdr.s;
	/* header start */
	if (code==401)
	{
		add_string( p, AUTHORIZATION_HDR_START USERNAME_FIELD_S,
			AUTHORIZATION_HDR_START_LEN+USERNAME_FIELD_LEN);
	} else {
		add_string( p, PROXY_AUTHORIZATION_HDR_START USERNAME_FIELD_S,
			PROXY_AUTHORIZATION_HDR_START_LEN+USERNAME_FIELD_LEN);
	}
	/* username */
	add_string( p, crd->user.s, crd->user.len);
	/* REALM */
	add_string( p, FIELD_SEPARATOR_S REALM_FIELD_S,
		FIELD_SEPARATOR_LEN+REALM_FIELD_LEN);
	add_string( p, crd->realm.s, crd->realm.len);
	/* NONCE */
	add_string( p, FIELD_SEPARATOR_S NONCE_FIELD_S, 
		FIELD_SEPARATOR_LEN+NONCE_FIELD_LEN);
	add_string( p, auth->nonce.s, auth->nonce.len);
	/* URI */
	add_string( p, FIELD_SEPARATOR_S URI_FIELD_S,
		FIELD_SEPARATOR_LEN+URI_FIELD_LEN);
	add_string( p, uri->s, uri->len);
	/* OPAQUE */
	if (auth->opaque.len )
	{
		add_string( p, FIELD_SEPARATOR_S OPAQUE_FIELD_S, 
			FIELD_SEPARATOR_LEN+OPAQUE_FIELD_LEN);
		add_string( p, auth->opaque.s, auth->opaque.len);
	}
	if((auth->flags&QOP_AUTH) || (auth->flags&QOP_AUTH_INT))
	{
		add_string( p, FIELD_SEPARATOR_S QOP_FIELD_S, 
			FIELD_SEPARATOR_LEN+QOP_FIELD_LEN);
		add_string( p, "auth", 4);
		add_string( p, FIELD_SEPARATOR_UQ_S NC_FIELD_S, 
			FIELD_SEPARATOR_UQ_LEN+NC_FIELD_LEN);
		add_string( p, auth->nc->s, auth->nc->len);
		add_string( p, FIELD_SEPARATOR_UQ_S CNONCE_FIELD_S, 
			FIELD_SEPARATOR_UQ_LEN+CNONCE_FIELD_LEN);
		add_string( p, auth->cnonce->s, auth->cnonce->len);
	}
	/* RESPONSE */
	add_string( p, FIELD_SEPARATOR_S RESPONSE_FIELD_S,
		FIELD_SEPARATOR_LEN+RESPONSE_FIELD_LEN);
	add_string( p, response, response_len);
	/* ALGORITHM */
	add_string( p, FIELD_SEPARATOR_S ALGORITHM_FIELD_S CRLF,
		FIELD_SEPARATOR_LEN+ALGORITHM_FIELD_LEN+CRLF_LEN);

	_uac_auth_hdr.len = p - _uac_auth_hdr.s;

	if (_uac_auth_hdr.len!=len)
	{
		LM_CRIT("BUG: bad buffer computation "
			"(%d<>%d)\n",len,_uac_auth_hdr.len);
		pkg_free( _uac_auth_hdr.s );
		goto error;
	}

	LM_DBG("hdr is <%.*s>\n",
		_uac_auth_hdr.len,_uac_auth_hdr.s);

	return &_uac_auth_hdr;
error:
	return 0;
}

