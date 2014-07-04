/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * UAC SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2005-01-31  first version (ramona)
 */


#include <ctype.h>
#include <string.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "../../dset.h"
#include "../../modules/tm/tm_load.h"

#include "auth.h"
#include "auth_alg.h"
#include "auth_hdr.h"


extern struct tm_binds uac_tmb;


static struct uac_credential *crd_list = 0;


#define  duplicate_str(_strd, _strs, _error) \
	do { \
		_strd.s = (char*)pkg_malloc(_strs.len); \
		if (_strd.s==0) \
		{ \
			LOG(L_ERR,"ERROR:uac:add_credential: " \
				"no more pkg memory\n");\
			goto _error; \
		} \
		memcpy( _strd.s, _strs.s, _strs.len); \
		_strd.len = _strs.len; \
	}while(0)


#define WWW_AUTH_CODE       401
#define WWW_AUTH_HDR        "WWW-Authenticate"
#define WWW_AUTH_HDR_LEN    (sizeof(WWW_AUTH_HDR)-1)
#define PROXY_AUTH_CODE     407
#define PROXY_AUTH_HDR      "Proxy-Authenticate"
#define PROXY_AUTH_HDR_LEN  (sizeof(PROXY_AUTH_HDR)-1)


int has_credentials()
{
	return (crd_list!=0)?1:0;
}

void free_credential(struct uac_credential *crd)
{
	if (crd)
	{
		if (crd->realm.s)
			pkg_free(crd->realm.s);
		if (crd->user.s)
			pkg_free(crd->user.s);
		if (crd->passwd.s)
			pkg_free(crd->passwd.s);
		pkg_free(crd);
	}
}


int add_credential( unsigned int type, void *val)
{
	struct uac_credential *crd;
	char *p;
	str foo;

	p = (char*)val;
	crd = 0;

	if (p==0 || *p==0)
		goto error;

	crd = (struct uac_credential*)pkg_malloc(sizeof(struct uac_credential));
	if (crd==0)
	{
		LOG(L_ERR,"ERROR:uac:add_credential: no more pkg mem\n");
		goto error;
	}
	memset( crd, 0, sizeof(struct uac_credential));

	/*parse the user */
	while (*p && isspace((int)*p)) p++;
	foo.s = p;
	while (*p && *p!=':' && !isspace((int)*p)) p++;
	if (foo.s==p || *p==0)
		/* missing or empty user */
		goto parse_error;
	foo.len = p - foo.s;
	/* dulicate it */
	duplicate_str( crd->user, foo, error);

	/* parse the ':' separator */
	while (*p && isspace((int)*p)) p++;
	if (*p!=':')
		goto parse_error;
	p++;
	while (*p && isspace((int)*p)) p++;
	if (*p==0)
		goto parse_error;

	/*parse the realm */
	while (*p && isspace((int)*p)) p++;
	foo.s = p;
	while (*p && *p!=':' && !isspace((int)*p)) p++;
	if (foo.s==p || *p==0)
		/* missing or empty realm */
		goto parse_error;
	foo.len = p - foo.s;
	/* dulicate it */
	duplicate_str( crd->realm, foo, error);

	/* parse the ':' separator */
	while (*p && isspace((int)*p)) p++;
	if (*p!=':')
		goto parse_error;
	p++;
	while (*p && isspace((int)*p)) p++;
	if (*p==0)
		goto parse_error;

	/*parse the passwd */
	while (*p && isspace((int)*p)) p++;
	foo.s = p;
	while (*p && !isspace((int)*p)) p++;
	if (foo.s==p)
		/* missing or empty passwd */
		goto parse_error;
	foo.len = p - foo.s;
	/* dulicate it */
	duplicate_str( crd->passwd, foo, error);

	/* end of string */
	while (*p && isspace((int)*p)) p++;
	if (*p!=0)
		goto parse_error;

	/* link the new cred struct */
	crd->next = crd_list;
	crd_list = crd;

	pkg_free(val);
	return 0;
parse_error:
		LOG(L_ERR,"ERROR:uac:add_credential: parse error in <%s> "
		"around %ld\n", (char*)val, (long)(p-(char*)val));
error:
	if (crd)
		free_credential(crd);
	return -1;
}


void destroy_credentials()
{
	struct uac_credential *foo;

	while (crd_list)
	{
		foo = crd_list;
		crd_list = crd_list->next;
		free_credential(foo);
	}
	crd_list = 0;
}


static inline struct hdr_field *get_autenticate_hdr(struct sip_msg *rpl,
																int rpl_code)
{
	struct hdr_field *hdr;
	str hdr_name;

	/* what hdr should we look for */
	if (rpl_code==WWW_AUTH_CODE)
	{
		hdr_name.s = WWW_AUTH_HDR;
		hdr_name.len = WWW_AUTH_HDR_LEN;
	} else if (rpl_code==PROXY_AUTH_CODE) {
		hdr_name.s = PROXY_AUTH_HDR;
		hdr_name.len = PROXY_AUTH_HDR_LEN;
	} else {
		LOG( L_ERR,"ERROR:uac:get_autenticate_hdr: reply is not an "
			"auth request\n");
		goto error;
	}

	DBG("DEBUG:uac:get_autenticate_hdr: looking for header \"%.*s\"\n",
		hdr_name.len, hdr_name.s);

	/* search the auth hdr, but first parse them all */
	if (parse_headers( rpl, HDR_EOH_F, 0)<0)
	{
		LOG( L_ERR,"ERROR:uac:get_autenticate_hdr: failed to parse reply\n");
		goto error;
	}
	for( hdr=rpl->headers ; hdr ; hdr=hdr->next )
	{
		if ( hdr->type!=HDR_OTHER_T )
			continue;
		if (hdr->name.len==hdr_name.len &&
		strncasecmp(hdr->name.s,hdr_name.s, hdr_name.len)==0 )
			return hdr;
	}

	LOG( L_ERR,"ERROR:uac:get_autenticate_hdr: reply has no "
		"auth hdr (%.*s)\n", hdr_name.len, hdr_name.s);
error:
	return 0;
}


static inline struct uac_credential *lookup_realm( str *realm)
{
	struct uac_credential *crd;

	for( crd=crd_list ; crd ; crd=crd->next )
		if (realm->len==crd->realm.len &&
		strncmp( realm->s, crd->realm.s, realm->len)==0 )
			return crd;
	return 0;
}


static inline void do_uac_auth(struct sip_msg *req, str *uri,
		struct uac_credential *crd, struct authenticate_body *auth,
		HASHHEX response)
{
	HASHHEX ha1;
	HASHHEX ha2;

	/* do authentication */
	uac_calc_HA1( crd, auth, 0/*cnonce*/, ha1);
	uac_calc_HA2( &req->first_line.u.request.method, uri,
		auth, 0/*hentity*/, ha2 );

	uac_calc_response( ha1, ha2, auth, 0/*nc*/, 0/*cnonce*/, response);
}


static inline int apply_urihdr_changes( struct sip_msg *req,
													str *uri, str *hdr)
{
	struct lump* anchor;

	/* add the uri */
	if (req->new_uri.s)
	{
		pkg_free(req->new_uri.s);
		req->new_uri.len=0;
	}
	req->parsed_uri_ok=0;
	req->new_uri.s = (char*)pkg_malloc(uri->len+1);
	if (req->new_uri.s==0)
	{
		LOG(L_ERR,"ERROR:uac:apply_urihdr_changes: no more pkg\n");
		goto error;
	}
	memcpy( req->new_uri.s, uri->s, uri->len);
	req->new_uri.s[uri->len]=0;
	req->new_uri.len=uri->len;
	ruri_mark_new();

	/* add the header */
	if (parse_headers(req, HDR_EOH_F, 0) == -1)
	{
		LOG(L_ERR,"ERROR:uac:apply_urihdr_changes: failed to parse message\n");
		goto error;
	}

	anchor = anchor_lump(req, req->unparsed - req->buf, 0, 0);
	if (anchor==0)
	{
		LOG(L_ERR,"ERROR:uac:apply_urihdr_changes: failed to get anchor\n");
		goto error;
	}

	if (insert_new_lump_before(anchor, hdr->s, hdr->len, 0) == 0)
	{
		LOG(L_ERR,"ERROR:uac:apply_urihdr_changes: faield to insert lump\n");
		goto error;
	}

	return 0;
error:
	pkg_free( hdr->s );
	return -1;
}



int uac_auth( struct sip_msg *msg)
{
	static struct authenticate_body auth;
	struct uac_credential *crd;
	int picked_code, picked_br, b;
	struct sip_msg *rpl;
	struct cell *t;
	struct hdr_field *hdr;
	HASHHEX response;
	str *new_hdr;

	/* get transaction */
	t = uac_tmb.t_gett();
	if (t==T_UNDEFINED || t==T_NULL_CELL)
	{
		LOG(L_CRIT,"BUG:uac:uac_auth: no current transaction found\n");
		goto error;
	}

	/* pick the selected reply */
	picked_br = -1;
	picked_code = 999;
	for ( b=0; b<t->nr_of_outgoings ; b++ )
	{
		/* skip 'empty branches' */
		if (!t->uac[b].request.buffer)
			continue;
		/* there is still an unfinished UAC transaction? */
		if ( t->uac[b].last_received<200 )
		{
			LOG(L_CRIT,"BUG:uac:uac_auth: incomplet transaction in failure "
				"route\n");
			goto error;
		}
		if ( t->uac[b].last_received<picked_code )
		{
			picked_br = b;
			picked_code = t->uac[b].last_received;
		}
	}
	if (picked_br<0)
	{
		LOG(L_CRIT,"BUG:uac:uac_auth: empty transaction in failure "
			"route\n");
		goto error;
	}

	rpl = t->uac[picked_br].reply;
	DBG("DEBUG:uac:uac_auth: picked reply is %p, code %d\n",rpl,picked_code);

	if (rpl==0)
	{
		LOG(L_CRIT,"BUG:uac:uac_auth: empty reply on picked branch\n");
		goto error;
	}
	if (rpl==FAKED_REPLY)
	{
		LOG(L_ERR,"ERROR:uac:uac_auth: cannot process a FAKED reply\n");
		goto error;
	}

	hdr = get_autenticate_hdr( rpl, picked_code);
	if (hdr==0)
	{
		LOG( L_ERR,"ERROR:uac:uac_auth: failed to extract authenticate hdr\n");
		goto error;
	}

	DBG("DEBUG:uac:uac_auth: header found; body=<%.*s>\n",
		hdr->body.len, hdr->body.s);

	if (parse_authenticate_body( &hdr->body, &auth)<0)
	{
		LOG(L_ERR,"ERROR:uac:uac_auth: failed to parse auth hdr body\n");
		goto error;
	}

	/* can we authenticate this realm? */
	crd = lookup_realm( &auth.realm );
	if (crd==0)
	{
		LOG(L_ERR,"ERROR:uac:uac_auth: no credential for realm \"%.*s\"\n",
			auth.realm.len, auth.realm.s);
		goto error;
	}

	/* do authentication */
	do_uac_auth( msg, &t->uac[picked_br].uri, crd, &auth, response);

	/* build the authorization header */
	new_hdr = build_authorization_hdr( picked_code, &t->uac[picked_br].uri,
		crd, &auth, response);
	if (new_hdr==0)
	{
		LOG(L_ERR,"ERROR:uac:uac_auth: failed to build authorization hdr\n");
		goto error;
	}

	/* so far, so good -> add the header and set the proper RURI */
	if ( apply_urihdr_changes( msg, &t->uac[picked_br].uri, new_hdr)<0 )
	{
		LOG(L_ERR,"ERROR:uac:uac_auth: failed to apply changes\n");
		goto error;
	}

	return 0;
error:
	return -1;
}



