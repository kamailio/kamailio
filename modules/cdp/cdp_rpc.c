/*
 * rpc.c
 *
 *  Created on: 27 May 2014
 *      Author: jaybeepee
 */
#include "cdp_rpc.h"
#include "peermanager.h"
#include "peerstatemachine.h"
#include "receiver.h"
#include "../../str.h"
#include "../../dprint.h"

extern dp_config *config;

static const char* cdp_rpc_disable_peer_doc[2] 	= 	{"disable diameter peer", 0 };
static const char* cdp_rpc_enable_peer_doc[2] 	= 	{"enable diameter peer", 0 };


static void cdp_rpc_enable_peer(rpc_t* rpc, void* ctx)
{
	peer *cdp_peer;
	str peer_fqdn;

	if (rpc->scan(ctx, "S", &peer_fqdn) < 1) {
		rpc->fault(ctx, 400, "required peer fqdn argument");
		return;
	}

	cdp_peer = get_peer_by_fqdn(&peer_fqdn);
	if (cdp_peer != NULL) {
		LM_DBG("Enabling CDP Peer: [%.*s]\n", peer_fqdn.len, peer_fqdn.s);
		cdp_peer->disabled = 0;
		return;
	}
	rpc->fault(ctx, 400, "peer not found");
	return;
}

static void cdp_rpc_disable_peer(rpc_t* rpc, void* ctx)
{
	peer *cdp_peer;
	str peer_fqdn;

	if (rpc->scan(ctx, "S", &peer_fqdn) < 1) {
		rpc->fault(ctx, 400, "required peer fqdn argument");
		return;
	}
	cdp_peer = get_peer_by_fqdn(&peer_fqdn);
	if (cdp_peer != NULL) {
		LM_DBG("Disabling CDP peer: [%.*s]\n", peer_fqdn.len, peer_fqdn.s);
		cdp_peer->disabled = 1;
		return;
	}

	rpc->fault(ctx, 400, "peer not found");
	return;

}

rpc_export_t cdp_rpc[] = {
	{"cdp.disable_peer",	cdp_rpc_disable_peer,   cdp_rpc_disable_peer_doc,   0},
	{"cdp.enable_peer",   	cdp_rpc_enable_peer,   	cdp_rpc_enable_peer_doc,   	0},
	{0, 0, 0, 0}
};

