/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "routing.h"
#include "config.h"
#include "peermanager.h"
#include "diameter_api.h"

#define LB_MAX_PEERS 20			/**< maximum peers that can be loadbalanced accross i.e. same metric */

extern dp_config *config; /**< Configuration for this diameter peer 	*/
int gcount = 0;

/**
 * Returns if the peer advertised support for an Application ID
 * @param p - the peer to check
 * @param app_id - the application id to look for
 * @param vendor_id - the vendor id to look for, 0 if not vendor specific 
 * @returns 0 if not found, 1 if found
 */
int peer_handles_application(peer *p, int app_id, int vendor_id) {
    int i;
    LM_DBG("Checking if peer %.*s handles application %d for vendord %d\n", p->fqdn.len, p->fqdn.s, app_id, vendor_id);
    if (!p || !p->applications || !p->applications_cnt) return 0;
    for (i = 0; i < p->applications_cnt; i++)
        if (p->applications[i].id == app_id && p->applications[i].vendor == vendor_id) return 1;
    return 0;
}

/**
 * Get the first peer that is connected from the list of routing entries.
 * @param r - the list of routing entries to look into
 * @returns - the peer or null if none connected
 */
peer* get_first_connected_route(cdp_session_t* cdp_session, routing_entry *r, int app_id, int vendor_id) {
    peer * peers[LB_MAX_PEERS];
    int peer_count = 0;
    int prev_metric = 0;
    routing_entry *i;
    peer *p;
    int j;
    time_t least_recent_time;

    if (cdp_session) {
        /*try and find an already used peer for this session - sticky*/
        if ((cdp_session->sticky_peer_fqdn.len > 0) && cdp_session->sticky_peer_fqdn.s) {
            //we have an old sticky peer. let's make sure it's up and connected before we use it.
            p = get_peer_by_fqdn(&cdp_session->sticky_peer_fqdn);
            if (p && !p->disabled && (p->state == I_Open || p->state == R_Open) && peer_handles_application(p, app_id, vendor_id)) {
                p->last_selected = time(NULL);
                LM_DBG("Found a sticky peer [%.*s] for this session - re-using\n", p->fqdn.len, p->fqdn.s);
                return p;
            }
        }
    }

    for (i = r; i; i = i->next) {
        if (peer_count >= LB_MAX_PEERS)
            break;
        p = get_peer_by_fqdn(&(i->fqdn));
        if (!p)
            LM_DBG("The peer %.*s does not seem to be connected or configured\n",
                i->fqdn.len, i->fqdn.s);
        else
            LM_DBG("The peer %.*s state is %s\n", i->fqdn.len, i->fqdn.s,
                (p->state == I_Open || p->state == R_Open) ? "opened" : "closed");
        if (p && !p->disabled && (p->state == I_Open || p->state == R_Open) && peer_handles_application(p, app_id, vendor_id)) {
            LM_DBG("The peer %.*s matches - will forward there\n", i->fqdn.len, i->fqdn.s);
            if (peer_count != 0) {//check the metric
                if (i->metric != prev_metric)
                    break;
                //metric must be the same
                peers[peer_count++] = p;
            } else {//we're first
                prev_metric = i->metric;
                peers[peer_count++] = p;
            }
        }
    }

    if (peer_count == 0) {
        return 0;
    }

    least_recent_time = peers[0]->last_selected;
    p = peers[0];
    for (j = 1; j < peer_count; j++) {
        if (peers[j]->last_selected < least_recent_time) {
            least_recent_time = peers[j]->last_selected;
            p = peers[j];
        }
    }

    if (cdp_session) {
        if (cdp_session->sticky_peer_fqdn_buflen <= p->fqdn.len) {
            LM_DBG("not enough storage for sticky peer - allocating more\n");
            if (cdp_session->sticky_peer_fqdn.s)
                shm_free(cdp_session->sticky_peer_fqdn.s);

            cdp_session->sticky_peer_fqdn.s = (char*) shm_malloc(p->fqdn.len + 1);
            if (!cdp_session->sticky_peer_fqdn.s) {
                LM_ERR("no more shm memory\n");
                return 0;
            }
            cdp_session->sticky_peer_fqdn_buflen = p->fqdn.len + 1;
            memset(cdp_session->sticky_peer_fqdn.s, 0, p->fqdn.len + 1);
        }
        cdp_session->sticky_peer_fqdn.len = p->fqdn.len;
        memcpy(cdp_session->sticky_peer_fqdn.s, p->fqdn.s, p->fqdn.len);
    }
    p->last_selected = time(NULL);
    return p;
}

/**
 * Get the first connect peer that matches the routing mechanisms.
 * - First the Destination-Host AVP value is tried if connected (the peer does not have to
 * be in the routing table at all).
 * - Then we look for a connected peer in the specific realm for the Destination-Realm AVP
 * - Then we look for the first connected peer in the default routes
 * @param m - the Diameter message to find the destination peer for
 * @returns - the connected peer or null if none connected found
 */
peer* get_routing_peer(cdp_session_t* cdp_session, AAAMessage *m) {
    str destination_realm = {0, 0}, destination_host = {0, 0};
    AAA_AVP *avp, *avp_vendor, *avp2;
    AAA_AVP_LIST group;
    peer *p;
    routing_realm *rr;
    int app_id = 0, vendor_id = 0;

    LM_DBG("getting diameter routing peer for realm: [%.*s]\n", m->dest_realm->data.len, m->dest_realm->data.s);

    app_id = m->applicationId;
    avp = AAAFindMatchingAVP(m, 0, AVP_Vendor_Specific_Application_Id, 0, AAA_FORWARD_SEARCH);
    if (avp) {
        group = AAAUngroupAVPS(avp->data);
        avp_vendor = AAAFindMatchingAVPList(group, group.head, AVP_Vendor_Id, 0, 0);
        avp2 = AAAFindMatchingAVPList(group, group.head, AVP_Auth_Application_Id, 0, 0);
        if (avp_vendor && avp2) {
            vendor_id = get_4bytes(avp_vendor->data.s);
            app_id = get_4bytes(avp2->data.s);
        }
        avp2 = AAAFindMatchingAVPList(group, group.head, AVP_Acct_Application_Id, 0, 0);
        if (avp_vendor && avp2) {
            vendor_id = get_4bytes(avp_vendor->data.s);
            app_id = get_4bytes(avp2->data.s);
        }
        AAAFreeAVPList(&group);
    }

    avp_vendor = AAAFindMatchingAVP(m, 0, AVP_Vendor_Id, 0, AAA_FORWARD_SEARCH);
    avp = AAAFindMatchingAVP(m, 0, AVP_Auth_Application_Id, 0, AAA_FORWARD_SEARCH);
    if (avp) {
        if (avp_vendor) vendor_id = get_4bytes(avp_vendor->data.s);
        else vendor_id = 0;
        app_id = get_4bytes(avp->data.s);
    }

    avp = AAAFindMatchingAVP(m, 0, AVP_Acct_Application_Id, 0, AAA_FORWARD_SEARCH);
    if (avp) {
        if (avp_vendor) vendor_id = get_4bytes(avp_vendor->data.s);
        else vendor_id = 0;
        app_id = get_4bytes(avp->data.s);
    }

    avp = AAAFindMatchingAVP(m, 0, AVP_Destination_Host, 0, AAA_FORWARD_SEARCH);
    if (avp) destination_host = avp->data;

    if (destination_host.len) {
        /* There is a destination host present in the message try and route directly there */
        p = get_peer_by_fqdn(&destination_host);
        if (p && (p->state == I_Open || p->state == R_Open) && peer_handles_application(p, app_id, vendor_id)) {
            p->last_selected = time(NULL);
            return p;
        }
        /* the destination host peer is not connected at the moment, try a normal route then */
    }

    avp = AAAFindMatchingAVP(m, 0, AVP_Destination_Realm, 0, AAA_FORWARD_SEARCH);
    if (avp) destination_realm = avp->data;

    if (!config->r_table) {
        LM_ERR("get_routing_peer(): Empty routing table.\n");
        return 0;
    }

    if (destination_realm.len) {
        /* first search for the destination realm */
        for (rr = config->r_table->realms; rr; rr = rr->next)
            if (rr->realm.len == destination_realm.len &&
                    strncasecmp(rr->realm.s, destination_realm.s, destination_realm.len) == 0)
                break;
        if (rr) {
            p = get_first_connected_route(cdp_session, rr->routes, app_id, vendor_id);
            if (p) return p;
            else LM_ERR("get_routing_peer(): No connected Route peer found for Realm <%.*s>. Trying DefaultRoutes next...\n",
                    destination_realm.len, destination_realm.s);
        }
    }
    /* if not found in the realms or no destination_realm, 
     * get the first connected host in default routes */
    LM_DBG("no routing peer found, trying default route\n");
    p = get_first_connected_route(cdp_session, config->r_table->routes, app_id, vendor_id);
    if (!p) {
        LM_ERR("get_routing_peer(): No connected DefaultRoute peer found for app_id %d and vendor id %d.\n",
                app_id, vendor_id);
    }
    return p;
}
