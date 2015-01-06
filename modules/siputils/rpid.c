/*
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
 *
 */

/*!
 * \file
 * \brief Remote-Party-ID related functions
 * \ingroup auth
 * - Module: \ref auth
 */

#include <string.h>
#include <strings.h>
#include "../../str.h"
#include "../../data_lump.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_nameaddr.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "rpid.h"


#define RPID_HF_NAME "Remote-Party-ID: "
#define RPID_HF_NAME_LEN (sizeof(RPID_HF_NAME) - 1)

extern str rpid_prefix;       /*!< Remote-Party-ID prefix */
extern str rpid_suffix;       /*!< Remote-Party-ID suffix */

/* rpid AVP specs */
static unsigned short rpid_avp_type;
static int_str rpid_avp_name;


/*!
 * \brief Parse and set the RPID AVP specs
 * \param rpid_avp_param RPID AVP parameter
 * \return 0 on success, -1 on failure
 */
int init_rpid_avp(char *rpid_avp_param)
{
	pv_spec_t avp_spec;
	str stmp;
	if (rpid_avp_param && *rpid_avp_param) {
		stmp.s = rpid_avp_param; stmp.len = strlen(stmp.s);
		if (pv_parse_spec(&stmp, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", rpid_avp_param);
			return -1;
		}

		if(pv_get_avp_name(0, &(avp_spec.pvp), &rpid_avp_name,
					&rpid_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", rpid_avp_param);
			return -1;
		}
	} else {
		rpid_avp_name.n = 0;
		rpid_avp_type = 0;
	}

	return 0;
}


/*!
 * \brief Gets the RPID avp specs
 * \param rpid_avp_p AVP name
 * \param rpid_avp_type_p AVP type
 */
void get_rpid_avp( int_str *rpid_avp_p, int *rpid_avp_type_p )
{
	*rpid_avp_p = rpid_avp_name;
	*rpid_avp_type_p = rpid_avp_type;
}


/*!
 * \brief Check if user is a E164 number
 * \param _user user 
 * \note Copy of is_e164 from enum module
 * \return 1 if its a E164 number, -1 if not
 */
static inline int is_e164(str* _user)
{
	int i;
	char c;
	
	if ((_user->len > 2) && (_user->len < 17) && ((_user->s)[0] == '+')) {
		for (i = 1; i < _user->len; i++) {
			c = (_user->s)[i];
			if ((c < '0') || (c > '9')) return -1;
		}
		return 1;
	} else {
	    return -1;
	}
}


/*!
 * \brief Append RPID helper function
 * \param _m SIP message
 * \param _s appended string
 * \note Copy of append_hf_helper from textops
 * \return 0 on success, negative on failure
 */
static inline int append_rpid_helper(struct sip_msg* _m, str *_s)
{
	struct lump* anchor;
	
	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse message\n");
		return -1;
	}
	
	anchor = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (!anchor) {
		LM_ERR("can't get anchor\n");
		return -2;
	}
	
	if (!insert_new_lump_before(anchor, _s->s, _s->len, 0)) {
		LM_ERR("can't insert lump\n");
		return -3;
	}

	return 0;
}


/*!
 * \brief Append RPID header field to the message
 * \param _m SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return 1 on success, -1 on failure
 */
int append_rpid_hf(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct usr_avp *avp;
	str rpid_hf, rpid;
	char *at;
	int_str val;

	if (rpid_avp_name.n==0) {
		LM_ERR("rpid avp not defined\n");
		return -1;
	}

	if ( (avp=search_first_avp( rpid_avp_type , rpid_avp_name, &val, 0))==0 ) {
		LM_DBG("no rpid AVP\n");
		return -1;
	}

	if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
		LM_DBG("empty or non-string rpid, nothing to append\n");
		return -1;
	}

	rpid = val.s;

	rpid_hf.len = RPID_HF_NAME_LEN + rpid_prefix.len + rpid.len
					+ rpid_suffix.len + CRLF_LEN;
	rpid_hf.s = pkg_malloc(rpid_hf.len);
	if (!rpid_hf.s) {
		LM_ERR("no memory left\n");
		return -1;
	}

	at = rpid_hf.s;
	memcpy(at, RPID_HF_NAME, RPID_HF_NAME_LEN);
	at += RPID_HF_NAME_LEN;

	memcpy(at, rpid_prefix.s, rpid_prefix.len);
	at += rpid_prefix.len;

	memcpy(at, rpid.s, rpid.len);
	at += rpid.len;

	memcpy(at, rpid_suffix.s, rpid_suffix.len);
	at += rpid_suffix.len;

	memcpy(at, CRLF, CRLF_LEN);

	if (append_rpid_helper(_m, &rpid_hf) < 0) {
		pkg_free(rpid_hf.s);
		return -1;
	}

	return 1;
}


/*!
 * \brief Append RPID header field to the message with parameters
 * \param _m SIP message
 * \param _prefix prefix
 * \param _suffix suffix
 * \return 1 on success, -1 on failure
 */
int append_rpid_hf_p(struct sip_msg* _m, char* _prefix, char* _suffix)
{
	struct usr_avp *avp;
	str rpid_hf, rpid;
	char* at;
	str* p, *s;
	int_str val;

	if (rpid_avp_name.n==0) {
		LM_ERR("rpid avp not defined\n");
		return -1;
	}

	if ( (avp=search_first_avp( rpid_avp_type , rpid_avp_name, &val, 0))==0 ) {
		LM_DBG("no rpid AVP\n");
		return -1;
	}

	if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
		LM_DBG("empty or non-string rpid, nothing to append\n");
		return -1;
	}

	rpid = val.s;

	p = (str*)_prefix;
	s = (str*)_suffix;

	rpid_hf.len = RPID_HF_NAME_LEN + p->len + rpid.len + s->len + CRLF_LEN;
	rpid_hf.s = pkg_malloc(rpid_hf.len);
	if (!rpid_hf.s) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	at = rpid_hf.s;
	memcpy(at, RPID_HF_NAME, RPID_HF_NAME_LEN);
	at += RPID_HF_NAME_LEN;

	memcpy(at, p->s, p->len);
	at += p->len;

	memcpy(at, rpid.s, rpid.len);
	at += rpid.len;

	memcpy(at, s->s, s->len);
	at += s->len;

	memcpy(at, CRLF, CRLF_LEN);

	if (append_rpid_helper(_m, &rpid_hf) < 0) {
		pkg_free(rpid_hf.s);
		return -1;
	}

	return 1;
}


/*!
 * \brief Check if URI in RPID AVP contains an E164 user part
 * \param _m SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return 1 if the URI contains an E164 user part, -1 if not
 */
int is_rpid_user_e164(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct usr_avp *avp;
	name_addr_t parsed;
	str tmp, rpid;
	struct sip_uri uri;
	int_str val;

	if (rpid_avp_name.n==0) {
		LM_ERR("rpid avp not defined\n");
		return -1;
	}

	if ( (avp=search_first_avp( rpid_avp_type , rpid_avp_name, &val, 0))==0 ) {
		LM_DBG("no rpid AVP\n");
		goto err;
	}

	if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
		LM_DBG("empty or non-string rpid, nothing to append\n");
		return -1;
	}

	rpid = val.s;

	if (find_not_quoted(&rpid, '<')) {
		if (parse_nameaddr(&rpid, &parsed) < 0) {
			LM_ERR("failed to parse RPID\n");
			goto err;
		}
		tmp = parsed.uri;
	} else {
		tmp = rpid;
	}

	if (parse_uri(tmp.s, tmp.len, &uri) < 0) {
	    LM_ERR("failed to parse RPID URI\n");
	    goto err;
	}

	return is_e164(&uri.user);

err:
	return -1;
}

