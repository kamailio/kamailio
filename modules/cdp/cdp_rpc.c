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
extern peer_list_t *peer_list;
extern gen_lock_t *peer_list_lock;
extern char *dp_states[];

static const char* cdp_rpc_disable_peer_doc[2] 	= 	{"disable diameter peer", 0 };
static const char* cdp_rpc_enable_peer_doc[2] 	= 	{"enable diameter peer", 0 };
static const char* cdp_rpc_list_peers_doc[2] 	= 	{"list diameter peers and their state", 0 };

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

static void cdp_rpc_list_peers(rpc_t* rpc, void* ctx)
{
    void *peers_header;
    void *peers_container;
    void *peerdetail_container;
    void *peerapplication_container;
    peer *i, *j;
    int c;
    char buf[100];

    if (rpc->add(ctx, "{", &peers_header) < 0) {
            rpc->fault(ctx, 500, "Internal error creating top rpc");
            return;
    }

    if (rpc->struct_add(peers_header, "SSddddddd{",
                            "Realm", &config->realm,
                            "Identity", &config->identity,
                            "Accept unknown peers", config->accept_unknown_peers,
                            "Connect timeout", config->connect_timeout,
                            "Transaction timeout", config->transaction_timeout,
                            "Default auth session timeout", config->default_auth_session_timeout,
                            "Queue length", config->queue_length,
                            "Workers", config->workers,
                            "Peer count", config->peers_cnt,
                            "Peers", &peers_container) < 0) {
            rpc->fault(ctx, 500, "Internal error creating peers header struct");
            return;
    }

    lock_get(peer_list_lock);
    i = peer_list->head;
    while (i) {
    		lock_get(i->lock);
    		if (rpc->struct_add(peers_container, "S{",
                            "FQDN", &i->fqdn,
                            "Details", &peerdetail_container) < 0) {
                    rpc->fault(ctx, 500, "Internal error creating peers container struct");
                    lock_release(i->lock);
                    return;
            }
            if (rpc->struct_add(peerdetail_container, "ssd",
                    "State", dp_states[(int)i->state],
                    "Disabled", i->disabled?"True":"False",
            		"Last used", i->last_selected) < 0) {
                    rpc->fault(ctx, 500, "Internal error creating peer detail container struct");
                    lock_release(i->lock);
                    return;
            }
            if (rpc->struct_add(peerdetail_container, "{", "Applications", &peerapplication_container) < 0) {
            	rpc->fault(ctx, 500, "Internal error creating peer application container struct");
            	lock_release(i->lock);
            	return;
            }

            for (c = 0; c < i->applications_cnt; c++) {
            	snprintf(buf, 100, "%d:%d", i->applications[c].id, i->applications[c].vendor);
            	if (rpc->struct_add(peerapplication_container, "s",
						"appid:vendorid", buf) < 0) {
					rpc->fault(ctx, 500, "Internal error creating appid/vendorid information");
					lock_release(i->lock);
					return;
				}
            }
            j=i;
            i = i->next;
            lock_release(j->lock);
    }
    lock_release(peer_list_lock);
}

rpc_export_t cdp_rpc[] = {
	{"cdp.disable_peer",	cdp_rpc_disable_peer,   cdp_rpc_disable_peer_doc,   0},
	{"cdp.enable_peer",   	cdp_rpc_enable_peer,   	cdp_rpc_enable_peer_doc,   	0},
	{"cdp.list_peers",   	cdp_rpc_list_peers,   	cdp_rpc_list_peers_doc,   	0},
	{0, 0, 0, 0}
};

