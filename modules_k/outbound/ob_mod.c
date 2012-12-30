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
#include "../../ip_addr.h"
#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/mi.h"

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

	return 0;
}

/* Structure of flow token

   <HMAC-SHA1-80><protocol><dst_ip><dst_port><src_ip><src_port>
          10     +    1    +  16   +    2    +  16   +    2
	   = 47 bytes maximum

   <protocol> specifies whether the addresses are IPv4 or IPv6 and the
   transport.
		Bits 0-6:	transport (see sip_protos enum from ip_addr.h)
		Bit 7:		IPv6 if set, IPv4 if unset	

   IP addresses will be 4 (for IPv6) or 16 (for IPv6) bytes.

   Maximum base64 encoded size: ceiling((47+2)/3)*4 = 68 bytes
*/

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

	/* HMAC-SHA1 the calculated flow token, truncate to 80 bits, and
	   prepend onto the flow token */
	HMAC(EVP_sha1(), ob_key.s, ob_key.len,
		&unenc_flow_token[FLOW_TOKEN_START_POS],
		pos - FLOW_TOKEN_START_POS,
		hmac_sha1, NULL);
	memcpy(unenc_flow_token, &hmac_sha1[SHA1_LENGTH - SHA1_80_LENGTH],
		SHA1_80_LENGTH);

	/* base64 encode the entire flow token and store for the caller to
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

	if (flow_token.len > base64_enc_len(UNENC_FLOW_TOKEN_MAX_LENGTH))
	{
		LM_INFO("bad flow token length.  Length is %d, expected <= %d\n",
			flow_token.len, UNENC_FLOW_TOKEN_MAX_LENGTH);
		return -1;
	}

	/* base64 decode the flow token */
	flow_length = base64_dec((unsigned char *) flow_token.s, flow_token.len,
			unenc_flow_token, UNENC_FLOW_TOKEN_MAX_LENGTH);

	/* HMAC-SHA1 the flow token (after the hash) and compare with the
	   truncated hash at the start of the flow token. */
	HMAC(EVP_sha1(), ob_key.s, ob_key.len,
		&unenc_flow_token[FLOW_TOKEN_START_POS],
		flow_length - FLOW_TOKEN_START_POS,
		hmac_sha1, NULL);
	if (memcmp(unenc_flow_token, &hmac_sha1[SHA1_LENGTH - SHA1_80_LENGTH],
		SHA1_80_LENGTH) != 0)
	{
		LM_INFO("flow token failed validation\n");
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
	/* If Outbound is forced return success without any further checks */
	if (isbflagset(0, ob_force_bflag) > 0)
		return 1;

	/* Use Outbound when:
	    # It's an initial request (out-of-dialog INVITE, REGISTER,
	      SUBSCRIBE, or REFER), with
	    # A single Via:, and
	    # Top Route: points to us and has ;ob parameter _OR_ Contact: has
	      ;ob parameter _OR_ it's a REGISTER with ;+sip.instance
	*/

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
