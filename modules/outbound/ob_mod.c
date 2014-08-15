/*
 * $Id$
 *
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */
#include <openssl/hmac.h>
#include <openssl/rand.h>

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
#include "../../parser/parse_supported.h"

#include "api.h"
#include "config.h"

MODULE_VERSION

#define OB_KEY_LEN	20

static int mod_init(void);
static void destroy(void);

static unsigned int ob_force_flag = (unsigned int) -1;
static unsigned int ob_force_no_flag = (unsigned int) -1;
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
	{ "force_outbound_flag",	INT_PARAM, &ob_force_flag },
	{ "force_no_outbound_flag",     INT_PARAM, &ob_force_no_flag },
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
	destroy,		/* destroy function */
	0			/* per-child initialization function */
};

static int mod_init(void)
{
	if (ob_force_flag != -1 && !flag_in_range(ob_force_flag))
	{
		LM_ERR("bad force_outbound_flag value (%d)\n", ob_force_flag);
		return -1;
	}

	if (ob_force_no_flag != -1 && !flag_in_range(ob_force_no_flag))
	{
		LM_ERR("bad no_outbound_flag value (%d)\n", ob_force_no_flag);
		return -1;
	}

	if ((ob_key.s = shm_malloc(OB_KEY_LEN)) == NULL)
	{
		LM_ERR("Failed to allocate memory for flow-token key\n");
		return -1;
	}
	ob_key.len = OB_KEY_LEN;
	if (RAND_bytes((unsigned char *) ob_key.s, ob_key.len) == 0)
	{
		LM_ERR("unable to get %d cryptographically strong pseudo-"
		       "random bytes\n", ob_key.len);
	}

	if (cfg_declare("outbound", outbound_cfg_def, &default_outbound_cfg,
			cfg_sizeof(outbound), &outbound_cfg))
	{
		LM_ERR("declaring config framework variable\n");
		return -1;
	}
	default_outbound_cfg.outbound_active = 1;

	if (!module_loaded("stun"))
	{
		LM_WARN("\"stun\" module is not loaded. STUN is required to use"
			" outbound with UDP.\n");
	}

	return 0;
}

static void destroy(void)
{
	if (ob_key.s)
		shm_free(ob_key.s);
}

/* Structure of flow-token

   <HMAC-SHA1-80><protocol><dst_ip><dst_port><src_ip><src_port>
          10     +    1    + 4or16 +    2    + 4or16 +    2
	   = 23 bytes minimum and 47 bytes maximum

   <protocol> specifies whether the addresses are IPv4 or IPv6 and the
   transport.
		Bits 0-6:	transport (see sip_protos enum from ip_addr.h)
		Bit 7:		IPv6 if set, IPv4 if unset	

   IP addresses will be 4 (for IPv6) or 16 (for IPv6) bytes.

   Minimum base64 encoded size: ceiling((23+2)/3)*4 = 52 bytes
   Maximum base64 encoded size: ceiling((47+2)/3)*4 = 68 bytes
*/

#define UNENC_FLOW_TOKEN_MIN_LENGTH	23
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
	unenc_flow_token[pos++] = (rcv.src_port >> 8) & 0xff;
	unenc_flow_token[pos++] =  rcv.src_port       & 0xff;

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

int decode_flow_token(struct sip_msg *msg, struct receive_info **rcv, str flow_token)
{
	int pos = FLOW_TOKEN_START_POS, flow_length, i;

	if (msg->ldv.flow.decoded)
		goto end;

	if (flow_token.s == NULL)
	{
		LM_DBG("no flow-token provided\n");
		return -2;
	}

	if (flow_token.len == 0)
	{
		LM_DBG("no flow-token found\n");
		return -2;
	}

	/* base64 decode the flow-token */
	flow_length = base64_dec((unsigned char *) flow_token.s, flow_token.len,
			unenc_flow_token, UNENC_FLOW_TOKEN_MAX_LENGTH);
	if (flow_length != UNENC_FLOW_TOKEN_MIN_LENGTH
		&& flow_length != UNENC_FLOW_TOKEN_MAX_LENGTH)
	{
		LM_DBG("no flow-token found - bad length (%d)\n", flow_length);
		return -2;
	}

	/* At this point the string is a valid base64 string and the correct
	   length.  It is highly unlikely that this is not meant to be a
	   flow-token.

	   HMAC-SHA1 the flow-token (after the hash) and compare with the
	   truncated hash at the start of the flow-token. */
	if (HMAC(EVP_sha1(), ob_key.s, ob_key.len,
		&unenc_flow_token[FLOW_TOKEN_START_POS],
		flow_length - FLOW_TOKEN_START_POS,
		hmac_sha1, NULL) == NULL)
	{
		LM_INFO("HMAC-SHA1 failed\n");
		return -1;
	}
	if (memcmp(unenc_flow_token, &hmac_sha1[SHA1_LENGTH - SHA1_80_LENGTH],
		SHA1_80_LENGTH) != 0)
	{
		LM_INFO("flow-token failed validation\n");
		return -1;
	}

	/* Decode protocol information */
	if (unenc_flow_token[pos] & 0x80)
	{
		msg->ldv.flow.rcv.dst_ip.af = msg->ldv.flow.rcv.src_ip.af = AF_INET6;
		msg->ldv.flow.rcv.dst_ip.len = msg->ldv.flow.rcv.src_ip.len = 16;
	}
	else
	{
		msg->ldv.flow.rcv.dst_ip.af = msg->ldv.flow.rcv.src_ip.af = AF_INET;
		msg->ldv.flow.rcv.dst_ip.len = msg->ldv.flow.rcv.src_ip.len = 4;
	}
	msg->ldv.flow.rcv.proto = unenc_flow_token[pos++] & 0x7f;

	/* Decode destination address */
	for (i = 0; i < (msg->ldv.flow.rcv.dst_ip.af == AF_INET6 ? 16 : 4); i++)
		msg->ldv.flow.rcv.dst_ip.u.addr[i] = unenc_flow_token[pos++];
	msg->ldv.flow.rcv.dst_port = unenc_flow_token[pos++] << 8;
	msg->ldv.flow.rcv.dst_port |= unenc_flow_token[pos++];

	/* Decode source address */
	for (i = 0; i < (msg->ldv.flow.rcv.src_ip.af == AF_INET6 ? 16 : 4); i++)
		msg->ldv.flow.rcv.src_ip.u.addr[i] = unenc_flow_token[pos++];
	msg->ldv.flow.rcv.src_port = unenc_flow_token[pos++] << 8;
	msg->ldv.flow.rcv.src_port |= unenc_flow_token[pos++];
	msg->ldv.flow.decoded = 1;

end:
	*rcv = &msg->ldv.flow.rcv;
	return 0;
}

static int use_outbound_register(struct sip_msg *msg)
{
	contact_t *contact;
	
	/* Check there is a single Via: */
	if (!(parse_headers(msg, HDR_VIA2_F, 0) == -1 || msg->via2 == 0
		|| msg->via2->error != PARSE_OK))
	{
		LM_DBG("second Via: found - outbound not used\n");
		return 0;
	}

	/* Look for ;reg-id in Contact-URIs */
	if (msg->contact
		|| (parse_headers(msg, HDR_CONTACT_F, 0) != -1 && msg->contact))
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
		
		if (contact->reg_id)
		{
			LM_DBG("found REGISTER with ;reg-id parameter on"
				" Contact-URI - outbound used\n");
			return 1;
		}

	}

	LM_DBG("outbound not used\n");
	return 0;
}

static int use_outbound_non_reg(struct sip_msg *msg)
{
	contact_t *contact;
	rr_t *rt;
	struct sip_uri puri;
	param_hooks_t hooks;
	param_t *params;
	int ret;
	struct receive_info *rcv = NULL;

	/* Check to see if the top Route-URI is me and has a ;ob parameter */
	if (msg->route
		|| (parse_headers(msg, HDR_ROUTE_F, 0) != -1 && msg->route))
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
		ret = check_self(&puri.host,
				puri.port_no ? puri.port_no : SIP_PORT, 0);
		if (ret < 1 || (ret == 1 && puri.gr.s != NULL))
		{
			/* If the host:port doesn't match, or does but it's
			   gruu */
			LM_DBG("top Route-URI is not me - outbound not"
				" used\n");
			return 0;
		}

		if (parse_params(&puri.params, CLASS_URI, &hooks,
			&params) != 0)
		{
			LM_ERR("parsing Route-URI parameters\n");
			return 0;
		}
		/* Not interested in param body - just the hooks */
		free_params(params);

		if (hooks.uri.ob)
		{
			LM_DBG("found ;ob parameter on Route-URI - outbound"
				" used\n");

			if (decode_flow_token(msg, &rcv, puri.user) == 0)
			{
				if (!ip_addr_cmp(&rcv->src_ip, &msg->rcv.src_ip)
					|| rcv->src_port != msg->rcv.src_port)
				{
					LM_DBG("\"incoming\" request found\n");
					return 2;
				}
			}

			LM_DBG("\"outgoing\" request found\n");
			return 1;
		}
	}

	/* Check if Supported: outbound is included */
	if (parse_supported(msg) == 0) {
                if (!(get_supported(msg) & F_OPTION_TAG_OUTBOUND)) {
		        LM_DBG("outbound is not supported and thus not used\n");
		        return 0;
		}
	}

	/* Check there is a single Via: */
	if (!(parse_headers(msg, HDR_VIA2_F, 0) == -1 || msg->via2 == 0
		|| msg->via2->error != PARSE_OK))
	{
		LM_DBG("second Via: found - outbound not used\n");
		return 0;
	}

	/* Look for ;ob in Contact-URIs */
	if (msg->contact
		|| (parse_headers(msg, HDR_CONTACT_F, 0) != -1 && msg->contact))
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
	
		if (parse_uri(contact->uri.s, contact->uri.len, &puri)
			< 0)
		{
			LM_ERR("parsing Contact-URI\n");
			return 0;
		}
		if (parse_params(&puri.params, CLASS_CONTACT, &hooks, &params)
			!= 0)
		{
			LM_ERR("parsing Contact-URI parameters\n");
			return 0;
		}
		/* Not interested in param body - just the hooks */
		free_params(params);

		if (hooks.contact.ob)
		{
			LM_DBG("found ;ob parameter on Contact-URI - outbound"
				" used\n");
			return 1;
		}
	}

	LM_DBG("outbound not used\n");
	return 0;
}

int use_outbound(struct sip_msg *msg)
{
	if (msg->first_line.type != SIP_REQUEST)
	{
		LM_ERR("use_outbound called for something that isn't a SIP"
			" request\n");
		return 0;
	}

	/* If Outbound is forced return success without any further checks */
	if (ob_force_flag != -1 && isflagset(msg, ob_force_flag) > 0)
	{
		LM_DBG("outbound used by force\n");
		return 1;
	}

	/* If Outbound is turned off, return failure without any further
	   checks */
	if (ob_force_no_flag != -1 && isflagset(msg, ob_force_no_flag) > 0)
	{
		LM_DBG("outbound not used by force\n");
		return 0;
	}

	LM_DBG("Analysing %.*s for outbound markers\n",
		msg->first_line.u.request.method.len,
		msg->first_line.u.request.method.s);

	if (msg->REQ_METHOD == METHOD_REGISTER)
		return use_outbound_register(msg);
	else
		return use_outbound_non_reg(msg);

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
