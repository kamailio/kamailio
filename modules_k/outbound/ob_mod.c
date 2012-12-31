/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <openssl/hmac.h>

#include "../../basex.h"
#include "../../dprint.h"
#include "../../dset.h"
#include "../../forward.h"
#include "../../ip_addr.h"
#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"

#include "api.h"

MODULE_VERSION

static int mod_init(void);

static unsigned int ob_force_bflag = (unsigned int) -1;
static str ob_key = {0, 0};

static cmd_export_t cmds[]= 
{
	{ "bind_ob", (cmd_function) bind_ob,
	  1, 0, 0,
	  0 },
	{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]=
{
	{ "force_outbound_bflag",	INT_PARAM, &ob_force_bflag },
	{ "flow_token_key",		STR_PARAM, &ob_key.s},
	{ 0, 0, 0 }
};

struct module_exports exports= 
{
	"outbound",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* Exported functions */
	params,			/* Exported parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,		/* module initialization function */
	0,			/* response function */
	0,			/* destroy function */
	0			/* per-child initialization function */
};

static int mod_init(void)
{
	if (ob_force_bflag == (unsigned int) -1)
		ob_force_bflag = 0;
	else if (ob_force_bflag >= 8 * sizeof (ob_force_bflag)) {
		LM_ERR("force_outbound_bflag (%d) too big!\n", ob_force_bflag);
		return -1;
	} else
		ob_force_bflag = 1 << ob_force_bflag;

	if (ob_key.s == 0)
	{
		LM_ERR("flow_token_key not set\n");
		return -1;
	}
	else
		ob_key.len = strlen(ob_key.s);

	if (ob_key.len != 20)
	{
		LM_ERR("flow_token_key wrong length. Expected 20 got %d\n",
			ob_key.len);
		return -1;
	}

	return 0;
}

/* Structure of flow-token

   <HMAC-SHA1-80><protocol><dst_ip><dst_port><src_ip><src_port>
          10     +    1    + 4or16 +    2    +  16   +    2
	   = 35 bytes minimum and 47 bytes maximum

   <protocol> specifies whether the addresses are IPv4 or IPv6 and the
   transport.
		Bits 0-6:	transport (see sip_protos enum from ip_addr.h)
		Bit 7:		IPv6 if set, IPv4 if unset	

   IP addresses will be 4 (for IPv6) or 16 (for IPv6) bytes.

   Minimum base64 encoded size: ceiling((35+2)/3)*4 = 52 bytes
   Maximum base64 encoded size: ceiling((47+2)/3)*4 = 68 bytes
*/

#define UNENC_FLOW_TOKEN_MIN_LENGTH	35
#define UNENC_FLOW_TOKEN_MAX_LENGTH	47
#define SHA1_LENGTH			20
#define SHA1_80_LENGTH			10
#define FLOW_TOKEN_START_POS		(SHA1_80_LENGTH)
static unsigned char unenc_flow_token[UNENC_FLOW_TOKEN_MAX_LENGTH];
static unsigned char hmac_sha1[EVP_MAX_MD_SIZE];

int encode_flow_token(str *flow_token, struct receive_info rcv)
{
	int pos = FLOW_TOKEN_START_POS, i;

	if (flow_token == NULL)
	{
		LM_ERR("bad string pointer\n");
		return -1;
	}

	/* Encode protocol information */
	unenc_flow_token[pos++] = 
		(rcv.dst_ip.af == AF_INET6 ? 0x80 : 0x00) | rcv.proto;

	/* Encode destination address */
	for (i = 0; i < (rcv.dst_ip.af == AF_INET6 ? 16 : 4); i++)
		unenc_flow_token[pos++] = rcv.dst_ip.u.addr[i];
	unenc_flow_token[pos++] = (rcv.dst_port >> 8) & 0xff;
	unenc_flow_token[pos++] =  rcv.dst_port       & 0xff;

	/* Encode source address */
	for (i = 0; i < (rcv.src_ip.af == AF_INET6 ? 16 : 4); i++)
		unenc_flow_token[pos++] = rcv.src_ip.u.addr[i];
	unenc_flow_token[pos++] = (rcv.dst_port >> 8) & 0xff;
	unenc_flow_token[pos++] =  rcv.dst_port       & 0xff;

	/* HMAC-SHA1 the calculated flow-token, truncate to 80 bits, and
	   prepend onto the flow-token */
	if (HMAC(EVP_sha1(), ob_key.s, ob_key.len,
		&unenc_flow_token[FLOW_TOKEN_START_POS],
		pos - FLOW_TOKEN_START_POS,
		hmac_sha1, NULL) == NULL)
	{
		LM_ERR("HMAC-SHA1 failed\n");
		return -1;
	}
	memcpy(unenc_flow_token, &hmac_sha1[SHA1_LENGTH - SHA1_80_LENGTH],
		SHA1_80_LENGTH);

	/* base64 encode the entire flow-token and store for the caller to
 	   use */
	flow_token->s = pkg_malloc(base64_enc_len(pos));
	if (flow_token->s == NULL)
	{
		LM_ERR("allocating package memory\n");
		return -1;
	}
	flow_token->len = base64_enc(unenc_flow_token, pos,
				(unsigned char *) flow_token->s,
				base64_enc_len(pos));

	return 0;
}

int decode_flow_token(struct receive_info *rcv, str flow_token)
{
	int pos = FLOW_TOKEN_START_POS, flow_length, i;

	if (rcv == NULL)
	{
		LM_ERR("bad receive_info structure provided\n");
		return -1;
	}

	if (flow_token.s == NULL)
	{
		LM_INFO("no flow-token provided\n");
		return -2;
	}

	if (flow_token.len != base64_enc_len(UNENC_FLOW_TOKEN_MIN_LENGTH)
	    && flow_token.len != base64_enc_len(UNENC_FLOW_TOKEN_MAX_LENGTH))
	{
		LM_INFO("bad flow-token length.  Length is %d, expected %d"
			" or %d.\n", flow_token.len,
			UNENC_FLOW_TOKEN_MIN_LENGTH,
			UNENC_FLOW_TOKEN_MAX_LENGTH);
		return -2;
	}

	/* base64 decode the flow-token */
	flow_length = base64_dec((unsigned char *) flow_token.s, flow_token.len,
			unenc_flow_token, UNENC_FLOW_TOKEN_MAX_LENGTH);
	if (flow_length == 0)
	{
		LM_INFO("not a valid base64 encoded string\n");
		return -2;
	}

	/* At this point the string is the correct length and a valid
	   base64 string.  It is highly unlikely that this is not meant to be
	   a flow-token.

	   HMAC-SHA1 the flow-token (after the hash) and compare with the
	   truncated hash at the start of the flow-token. */
	if (HMAC(EVP_sha1(), ob_key.s, ob_key.len,
		&unenc_flow_token[FLOW_TOKEN_START_POS],
		flow_length - FLOW_TOKEN_START_POS,
		hmac_sha1, NULL) == NULL)
	{
		LM_ERR("HMAC-SHA1 failed\n");
		return -1;
	}
	if (memcmp(unenc_flow_token, &hmac_sha1[SHA1_LENGTH - SHA1_80_LENGTH],
		SHA1_80_LENGTH) != 0)
	{
		LM_ERR("flow-token failed validation\n");
		return -1;
	}

	/* Decode protocol information */
	if (unenc_flow_token[pos] & 0x80)
	{
		rcv->dst_ip.af = rcv->src_ip.af = AF_INET6;
		rcv->dst_ip.len = rcv->src_ip.len = 16;
	}
	else
	{
		rcv->dst_ip.af = rcv->src_ip.af = AF_INET;
		rcv->dst_ip.len = rcv->src_ip.len = 4;
	}
	rcv->proto = unenc_flow_token[pos++] & 0x7f;

	/* Decode destination address */
	for (i = 0; i < (rcv->dst_ip.af == AF_INET6 ? 16 : 4); i++)
		rcv->dst_ip.u.addr[i] = unenc_flow_token[pos++];
	rcv->dst_port = ((unenc_flow_token[pos++] << 8) & 0xff)
				| (unenc_flow_token[pos++] & 0xff);

	/* Decode source address */
	for (i = 0; i < (rcv->src_ip.af == AF_INET6 ? 16 : 4); i++)
		rcv->src_ip.u.addr[i] = unenc_flow_token[pos++];
	rcv->src_port = ((unenc_flow_token[pos++] << 8) & 0xff)
				| (unenc_flow_token[pos++] & 0xff);

	return 0;
}

int use_outbound(struct sip_msg *msg)
{
	contact_t *contact;
	rr_t *rt;
	struct sip_uri puri;
	param_hooks_t hooks;
	param_t *params;
	int ret;

	/* If Outbound is forced return success without any further checks */
	if (isbflagset(0, ob_force_bflag) > 0)
	{
		LM_INFO("outbound forced\n");
		return 1;
	}

	/* Use Outbound when there is a single Via: header and:
	    # It's a REGISTER request with a Contact-URI containing a ;reg-id
	      parameter, or
	    # The Contact-URI has an ;ob parameter, or
	    # The top Route-URI points to use and has an ;ob parameter
	*/

	/* Check there is a single Via: */
	if (!(parse_headers(msg, HDR_VIA2_F, 0) == -1 || msg->via2 == 0
		|| msg->via2->error != PARSE_OK))
	{
		LM_INFO("second Via: found - outbound not used\n");
		return 0;
	}

	/* Look for ;reg-id in REGISTER Contact-URIs and ;ob in any
	   Contact-URIs */
	if (parse_headers(msg, HDR_CONTACT_F, 0) >= 0 && msg->contact)
	{
		if (parse_contact(msg->contact) < 0)
		{
			LM_ERR("parsing Contact: header body\n");
			return 0;
		}
		contact = ((contact_body_t *) msg->contact->parsed)->contacts;
		if (!contact)
		{
			LM_ERR("empty Contact:\n");
			return 0;
		}
		if (parse_uri(contact->uri.s, contact->uri.len, &puri) < 0)
		{
			LM_ERR("parsing Contact-URI\n");
			return 0;
		}
		if (parse_params(&puri.params, CLASS_CONTACT, &hooks,
			&params) != 0)
		{
			LM_ERR("parsing Contact-URI parameters\n");
			return 0;
		}

		if (msg->REQ_METHOD == METHOD_REGISTER && hooks.contact.reg_id)
		{
			LM_INFO("found REGISTER with ;reg_id paramter on"
				"Contact-URI - outbound used\n");
			return 1;
		}

		if (hooks.contact.ob)
		{
			LM_INFO("found ;ob parameter on Contact-URI - outbound"
				" used\n");
			return 1;
		}
	}

	/* Check to see if the top Route-URI is me and has a ;ob parameter */
	if (parse_headers(msg, HDR_ROUTE_F, 0) >= 0 && msg->route)
	{
		if (parse_rr(msg->route) < 0)
		{
			LM_ERR("parsing Route: header body\n");
			return 0;
		}
		rt = (rr_t *) msg->route->parsed;
		if (!rt)
		{
			LM_ERR("empty Route:\n");
			return 0;
		}
		if (parse_uri(rt->nameaddr.uri.s, rt->nameaddr.uri.len,
				&puri) < 0)
		{
			LM_ERR("parsing Route-URI\n");
			return 0;
		}
		if (parse_params(&puri.params, CLASS_URI, &hooks,
			&params) != 0)
		{
			LM_ERR("parsing Route-URI parameters\n");
			return 0;
		}

		ret = check_self(&puri.host,
				puri.port_no ? puri.port_no : SIP_PORT, 0);
		if (ret < 1 || (ret == 1 && puri.gr.s != NULL))
		{
			/* If the host:port doesn't match, or does but it's
			   gruu */
			LM_INFO("top Route-URI is not me - outbound not"
				" used\n");
			return 0;
		}

		if (hooks.uri.ob)
		{
			LM_INFO("found ;ob parameter on Route-URI - outbound"
				" used\n");
			return 1;
		}
	}

	LM_INFO("outbound not used\n");
	return 0;
}

int bind_ob(struct ob_binds *pxb)
{
	if (pxb == NULL)
	{
		LM_WARN("bind_outbound: Cannot load outbound API into NULL "
			"pointer\n");
		return -1;
	}

	pxb->encode_flow_token = encode_flow_token;
	pxb->decode_flow_token = decode_flow_token;
	pxb->use_outbound = use_outbound;

	return 0;
}
