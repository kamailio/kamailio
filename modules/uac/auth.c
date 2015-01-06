/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * UAC Kamailio-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC Kamailio-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#include <ctype.h>
#include <string.h>

#include "../../lib/kcore/cmpapi.h"
#include "../../dprint.h"
#include "../../pvar.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "../../hashes.h"
#include "../../dset.h"
#include "../../modules/tm/tm_load.h"

#include "auth.h"
#include "auth_alg.h"
#include "auth_hdr.h"


extern struct tm_binds uac_tmb;
extern pv_spec_t auth_username_spec;
extern pv_spec_t auth_realm_spec;
extern pv_spec_t auth_password_spec;


static struct uac_credential *crd_list = 0;


#define  duplicate_str(_strd, _strs, _error) \
	do { \
		_strd.s = (char*)pkg_malloc(_strs.len); \
		if (_strd.s==0) \
		{ \
			LM_ERR("no more pkg memory\n");\
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

static str nc = {"00000001", 8};
static str cnonce = {"o", 1};

int has_credentials(void)
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
		LM_ERR("no more pkg mem\n");
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
		LM_ERR("parse error in <%s> "
		"around %ld\n", (char*)val, (long)(p-(char*)val));
error:
	if (crd)
		free_credential(crd);
	return -1;
}


void destroy_credentials(void)
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


struct hdr_field *get_autenticate_hdr(struct sip_msg *rpl, int rpl_code)
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
		LM_ERR("reply is not an "
			"auth request\n");
		goto error;
	}

	LM_DBG("looking for header \"%.*s\"\n",
		hdr_name.len, hdr_name.s);

	/* search the auth hdr, but first parse them all */
	if (parse_headers( rpl, HDR_EOH_F, 0)<0)
	{
		LM_ERR("failed to parse reply\n");
		goto error;
	}
	for( hdr=rpl->headers ; hdr ; hdr=hdr->next )
	{
		if ( rpl_code==WWW_AUTH_CODE && hdr->type==HDR_WWW_AUTHENTICATE_T )
			return hdr;
		if ( rpl_code==PROXY_AUTH_CODE && hdr->type==HDR_PROXY_AUTHENTICATE_T )
			return hdr;
	}

	LM_ERR("reply has no "
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


static inline struct uac_credential *get_avp_credential(struct sip_msg *msg,
																str *realm)
{
	static struct uac_credential crd;
	pv_value_t pv_val;

	if(pv_get_spec_value( msg, &auth_realm_spec, &pv_val)!=0)
		return 0;

	if (pv_val.flags&PV_VAL_NULL || pv_val.rs.len<=0) {
		/* if realm parameter is empty or NULL, match any realm asked for */
		crd.realm = *realm;
	} else {
		crd.realm = pv_val.rs;
		/* is it the domain we are looking for? */
		if (realm->len!=crd.realm.len ||
		  strncmp( realm->s, crd.realm.s, realm->len)!=0 )
			return 0;
	}

	/* get username and password */
	if(pv_get_spec_value( msg, &auth_username_spec, &pv_val)!=0
	|| pv_val.flags&PV_VAL_NULL || pv_val.rs.len<=0)
		return 0;
	crd.user = pv_val.rs;

	if(pv_get_spec_value( msg, &auth_password_spec, &pv_val)!=0
	|| pv_val.flags&PV_VAL_NULL || pv_val.rs.len<=0)
		return 0;
	crd.passwd = pv_val.rs;

	return &crd;
}


void do_uac_auth(str *method, str *uri,
		struct uac_credential *crd,
		struct authenticate_body *auth,
		HASHHEX response)
{
	HASHHEX ha1;
	HASHHEX ha2;

	if((auth->flags&QOP_AUTH) || (auth->flags&QOP_AUTH_INT))
	{
		/* if qop generate nonce-count and cnonce */
		cnonce.s = int2str(get_hash1_raw(auth->nonce.s, auth->nonce.len), 
						   &cnonce.len);

		/* do authentication */
		uac_calc_HA1( crd, auth, &cnonce, ha1);
		uac_calc_HA2( method, uri,
			auth, 0/*hentity*/, ha2 );

		uac_calc_response( ha1, ha2, auth, &nc, &cnonce, response);
		auth->nc = &nc;
		auth->cnonce = &cnonce;
	} else {
		/* do authentication */
		uac_calc_HA1( crd, auth, 0/*cnonce*/, ha1);
		uac_calc_HA2( method, uri,
			auth, 0/*hentity*/, ha2 );

		uac_calc_response( ha1, ha2, auth, 0/*nc*/, 0/*cnonce*/, response);
	}
}


static inline int apply_urihdr_changes( struct sip_msg *req,
													str *uri, str *hdr)
{
	struct lump* anchor;

	/* add the uri - move it to branch directly FIXME (bogdan)*/
	if (req->new_uri.s)
	{
		pkg_free(req->new_uri.s);
		req->new_uri.len=0;
	}
	req->parsed_uri_ok=0;
	req->new_uri.s = (char*)pkg_malloc(uri->len+1);
	if (req->new_uri.s==0)
	{
		LM_ERR("no more pkg\n");
		goto error;
	}
	memcpy( req->new_uri.s, uri->s, uri->len);
	req->new_uri.s[uri->len]=0;
	req->new_uri.len=uri->len;
	ruri_mark_new();

	/* add the header */
	if (parse_headers(req, HDR_EOH_F, 0) == -1)
	{
		LM_ERR("failed to parse message\n");
		goto error;
	}

	anchor = anchor_lump(req, req->unparsed - req->buf, 0, 0);
	if (anchor==0)
	{
		LM_ERR("failed to get anchor\n");
		goto error;
	}

	if (insert_new_lump_before(anchor, hdr->s, hdr->len, 0) == 0)
	{
		LM_ERR("faield to insert lump\n");
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
	int code, branch;
	struct sip_msg *rpl;
	struct cell *t;
	struct hdr_field *hdr;
	HASHHEX response;
	str *new_hdr;

	/* get transaction */
	t = uac_tmb.t_gett();
	if (t==T_UNDEFINED || t==T_NULL_CELL)
	{
		LM_CRIT("no current transaction found\n");
		goto error;
	}

	/* get the selected branch */
	branch = uac_tmb.t_get_picked_branch();
	if (branch<0) {
		LM_CRIT("no picked branch (%d)\n",branch);
		goto error;
	}

	rpl = t->uac[branch].reply;
	code = t->uac[branch].last_received;
	LM_DBG("picked reply is %p, code %d\n",rpl,code);

	if (rpl==0)
	{
		LM_CRIT("empty reply on picked branch\n");
		goto error;
	}
	if (rpl==FAKED_REPLY)
	{
		LM_ERR("cannot process a FAKED reply\n");
		goto error;
	}

	hdr = get_autenticate_hdr( rpl, code);
	if (hdr==0)
	{
		LM_ERR("failed to extract authenticate hdr\n");
		goto error;
	}

	LM_DBG("header found; body=<%.*s>\n",
		hdr->body.len, hdr->body.s);

	if (parse_authenticate_body( &hdr->body, &auth)<0)
	{
		LM_ERR("failed to parse auth hdr body\n");
		goto error;
	}

	/* can we authenticate this realm? */
	crd = 0;
	/* first look into AVP, if set */
	if ( auth_realm_spec.type!=PVT_NONE )
		crd = get_avp_credential( msg, &auth.realm );
	/* if not found, look into predefined credentials */
	if (crd==0)
		crd = lookup_realm( &auth.realm );
	/* found? */
	if (crd==0)
	{
		LM_DBG("no credential for realm \"%.*s\"\n",
			auth.realm.len, auth.realm.s);
		goto error;
	}

	/* do authentication */
	do_uac_auth( &msg->first_line.u.request.method,
			&t->uac[branch].uri, crd, &auth, response);

	/* build the authorization header */
	new_hdr = build_authorization_hdr( code, &t->uac[branch].uri,
		crd, &auth, response);
	if (new_hdr==0)
	{
		LM_ERR("failed to build authorization hdr\n");
		goto error;
	}

	/* so far, so good -> add the header and set the proper RURI */
	if ( apply_urihdr_changes( msg, &t->uac[branch].uri, new_hdr)<0 )
	{
		LM_ERR("failed to apply changes\n");
		goto error;
	}

	/* mark request in T with uac auth for increase of cseq via dialog
	 * - this function is executed in failure route, msg_flags will be
	 *   reset afterwards by tm fake env */
	if(t->uas.request) t->uas.request->msg_flags |= FL_UAC_AUTH;

	return 0;
error:
	return -1;
}



