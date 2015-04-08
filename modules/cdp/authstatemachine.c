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

#include <time.h>

#include "authstatemachine.h"
#include "diameter_ims.h"

// all this 4 includes are here because of what i do in Send_ASA
#include "peer.h"
#include "peermanager.h"
#include "routing.h"
#include "receiver.h"
#include "common.h"

char *auth_states[] = {"Idle", "Pending", "Open", "Discon"};
char *auth_events[] = {};

extern dp_config *config; // because i want to use tc for the expire times...

/*
 * Alberto Diez changes the default behaviour on error is going to be to return the default state
 * that is  STATE_MAINTAINED
 * this is because in the Rx specification 3GPP TS 29214 v7.1.0 (2007-06)
 * the AVP Auth Session State is not included in any message exchange,
 * therefor we could as well not even check for it, but diameter rfc says to look at this
 *
 */

int get_auth_session_state(AAAMessage* msg) {
    if (!msg) goto error;
    AAA_AVP* rc = AAAFindMatchingAVP(msg, 0, AVP_Auth_Session_State, 0, 0);
    if (!rc) goto error;
    return get_4bytes(rc->data.s);

error:
    LM_DBG("get_auth_session_state(): no AAAMessage or Auth Session State not found\n");
    return STATE_MAINTAINED;
}

/**
 * Retrieve the Session-Timeout, Auth-Lifetime and Auth-Grace-Period AVPs and update the session timers accordingly
 * @param x
 * @param msg
 */
void update_auth_session_timers(cdp_auth_session_t *x, AAAMessage *msg) {
    AAA_AVP *avp;
    uint32_t session_timeout = 0, grace_period = 0, auth_lifetime = 0;

    avp = AAAFindMatchingAVP(msg, 0, AVP_Auth_Grace_Period, 0, 0);
    if (avp && avp->data.len == 4) {
        grace_period = get_4bytes(avp->data.s);
        x->grace_period = grace_period;
    }
    avp = AAAFindMatchingAVP(msg, 0, AVP_Authorization_Lifetime, 0, 0);
    if (avp && avp->data.len == 4) {
        auth_lifetime = get_4bytes(avp->data.s);
        switch (auth_lifetime) {
            case 0:
                x->lifetime = time(0);
                break;
            case 0xFFFFFFFF:
                x->lifetime = -1;
                break;
            default:
                x->lifetime = time(0) + auth_lifetime;
        }
        if (x->timeout != -1 && x->timeout < x->lifetime) x->timeout = x->lifetime + x->grace_period;
    }
    avp = AAAFindMatchingAVP(msg, 0, AVP_Session_Timeout, 0, 0);
    if (avp && avp->data.len == 4) {
        session_timeout = get_4bytes(avp->data.s);
        switch (session_timeout) {
            case 0:
                x->timeout = time(0) + config->default_auth_session_timeout;
                break;
            case 0xFFFFFFFF:
                x->timeout = -1;
                break;
            default:
                x->timeout = time(0) + session_timeout;
        }
        if (!x->lifetime) x->lifetime = x->timeout;
    }
}

/**
 * Add Session-Timeout, Auth-Lifetime and Auth-Grace-Period AVPs to outgoing messages in case they are missing
 * @param x
 * @param msg
 */
void add_auth_session_timers(cdp_auth_session_t *x, AAAMessage *msg) {
    AAA_AVP *avp;
    char data[4];
    uint32_t v;

    avp = AAAFindMatchingAVP(msg, 0, AVP_Authorization_Lifetime, 0, 0);
    if (!avp) {
        if (x->lifetime == -1) v = 0xFFFFFFFF;
        else {
            v = x->lifetime - time(0);
            if (v < 0) v = 0;
        }
        set_4bytes(data, v);
        avp = AAACreateAVP(AVP_Authorization_Lifetime, AAA_AVP_FLAG_MANDATORY, 0, data, 4, AVP_DUPLICATE_DATA);
        if (avp) AAAAddAVPToMessage(msg, avp, msg->avpList.tail);
    }
    if (x->lifetime != -1) {
        avp = AAAFindMatchingAVP(msg, 0, AVP_Auth_Grace_Period, 0, 0);
        if (!avp) {
            v = x->grace_period;
            set_4bytes(data, v);
            avp = AAACreateAVP(AVP_Auth_Grace_Period, AAA_AVP_FLAG_MANDATORY, 0, data, 4, AVP_DUPLICATE_DATA);
            if (avp) AAAAddAVPToMessage(msg, avp, msg->avpList.tail);
        }
    }
    avp = AAAFindMatchingAVP(msg, 0, AVP_Session_Timeout, 0, 0);
    if (!avp) {
        if (x->timeout == -1) v = 0xFFFFFFFF;
        else {
            v = x->timeout - time(0);
            if (v < 0) v = 0;
        }
        set_4bytes(data, v);
        avp = AAACreateAVP(AVP_Session_Timeout, AAA_AVP_FLAG_MANDATORY, 0, data, 4, AVP_DUPLICATE_DATA);
        if (avp) AAAAddAVPToMessage(msg, avp, msg->avpList.tail);
    }
}

/**
 * stateful client state machine
 * \Note - should be called with a lock on the session and will unlock it - do not use it after!
 * @param auth - AAAAuthSession which uses this state machine
 * @param ev   - Event
 * @param msg  - AAAMessage
 * @returns 0 if msg should be given to the upper layer 1 if not
 */
inline int auth_client_statefull_sm_process(cdp_session_t* s, int event, AAAMessage* msg) {

    cdp_auth_session_t *x;
    int rc;
    int rv = 0; //return value

    if (!s) {
        switch (event) {
            case AUTH_EV_RECV_ASR:
                Send_ASA(0, msg);
                break;
            default:
                LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d with no session!\n",
                        event);
        }
        return rv;
    }
    x = &(s->u.auth);

    if (s->cb) (s->cb)(event, s);
    LM_INFO("after callback of event %i\n", event);

    //if (x && x->state && msg) LM_ERR("auth_client_statefull_sm_process [event %i] [state %i] endtoend %u hopbyhop %u\n",event,x->state,msg->endtoendId,msg->hopbyhopId);

    switch (x->state) {
        case AUTH_ST_IDLE:
            switch (event) {
                case AUTH_EV_SEND_REQ:
                    s->application_id = msg->applicationId;
                    s->u.auth.state = AUTH_ST_PENDING;
                    update_auth_session_timers(x, msg);
                    add_auth_session_timers(x, msg);

                    //Richard add this add: add destination realm to cdp session
                    //use msg origin realm as the destination realm
                    //we do this here as this is were the state changes to open
                    //Where must we free this?
                    s->dest_realm.s = (char*) shm_malloc(msg->dest_realm->data.len);
                    memcpy(s->dest_realm.s, msg->dest_realm->data.s, msg->dest_realm->data.len);
                    s->dest_realm.len = msg->dest_realm->data.len;


                    //LM_INFO("state machine: i was in idle and i am going to pending\n");
                    break;
                default:
                    LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d while in state %s!(data %p)\n",
                            event, auth_states[x->state], x->generic_data);
            }
            break;

        case AUTH_ST_PENDING:
            if (event == AUTH_EV_RECV_ANS && msg && !is_req(msg)) {
                rc = get_result_code(msg);
                if (rc >= 2000 && rc < 3000 && get_auth_session_state(msg) == STATE_MAINTAINED)
                    event = AUTH_EV_RECV_ANS_SUCCESS;
                else
                    event = AUTH_EV_RECV_ANS_UNSUCCESS;
            }

            switch (event) {
                case AUTH_EV_RECV_ANS_SUCCESS:
                    x->state = AUTH_ST_OPEN;
                    update_auth_session_timers(x, msg);
                    //LM_INFO("state machine: i was in pending and i am going to open\n");
                    break;
                case AUTH_EV_RECV_ANS_UNSUCCESS:
		    LM_DBG("In state AUTH_ST_PENDING and received AUTH_EV_RECV_ANS_UNSUCCESS - nothing to do but clean up session\n");
                case AUTH_EV_SESSION_TIMEOUT:
                case AUTH_EV_SERVICE_TERMINATED:
                case AUTH_EV_SESSION_GRACE_TIMEOUT:
                    cdp_session_cleanup(s, NULL);
		    s=0;
                    break;

                default:
                    LM_ERR("auth_client_stateless_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        case AUTH_ST_OPEN:
            if (event == AUTH_EV_RECV_ANS && msg && !is_req(msg)) {
                rc = get_result_code(msg);
                if (rc >= 2000 && rc < 3000 && get_auth_session_state(msg) == STATE_MAINTAINED)
                    event = AUTH_EV_RECV_ANS_SUCCESS;
                else
                    event = AUTH_EV_RECV_ANS_UNSUCCESS;
            }

            switch (event) {
                case AUTH_EV_SEND_REQ:
                    // if the request is STR i should move to Discon ..
                    // this is not in the state machine but I (Alberto Diez) need it
                    if (msg->commandCode == IMS_STR)
                        s->u.auth.state = AUTH_ST_DISCON;
                    else {
                        s->u.auth.state = AUTH_ST_OPEN;
                        add_auth_session_timers(x, msg);
                    }
                    break;

                case AUTH_EV_RECV_ANS_SUCCESS:
                    x->state = AUTH_ST_OPEN;
                    update_auth_session_timers(x, msg);
                    //LM_INFO("state machine: i was in open and i am going to open\n");
                    break;

                case AUTH_EV_RECV_ANS_UNSUCCESS:
                    x->state = AUTH_ST_DISCON;
                    //LM_INFO("state machine: i was in open and i am going to discon\n");
                    break;

                case AUTH_EV_SESSION_TIMEOUT:
                case AUTH_EV_SERVICE_TERMINATED:
                case AUTH_EV_SESSION_GRACE_TIMEOUT:
                    x->state = AUTH_ST_DISCON;
                    //LM_INFO("state machine: i was in open and i am going to discon\n");

                    Send_STR(s, msg);
                    break;

                case AUTH_EV_SEND_ASA_SUCCESS:
                    x->state = AUTH_ST_DISCON;
                    //LM_INFO("state machine: i was in open and i am going to discon\n");
                    Send_STR(s, msg);
                    break;

                case AUTH_EV_SEND_ASA_UNSUCCESS:
                    x->state = AUTH_ST_OPEN;
                    update_auth_session_timers(x, msg);
                    //LM_INFO("state machine: i was in open and i am going to open\n");
                    break;

                case AUTH_EV_RECV_ASR:
                    // two cases , client will comply or will not
                    // our client is very nice and always complys.. because
                    // our brain is in the PCRF... if he says to do this , we do it
                    // Alberto Diez , (again this is not Diameter RFC)
                    x->state = AUTH_ST_DISCON;
                    Send_ASA(s, msg);
                    Send_STR(s, msg);
                    break;

                default:
                    LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        case AUTH_ST_DISCON:
            switch (event) {
                case AUTH_EV_RECV_ASR:
                    x->state = AUTH_ST_DISCON;
                    //LM_INFO("state machine: i was in discon and i am going to discon\n");
                    Send_ASA(s, msg);
                    break;

                    // Just added this because it might happen if the other peer doesnt
                    // send a valid STA, then the session stays open forever...
                    // We dont accept that... we have lifetime+grace_period for that
                    // This is not in the Diameter RFC ...
                case AUTH_EV_SESSION_TIMEOUT:
                case AUTH_EV_SESSION_GRACE_TIMEOUT:
                    // thats the addition
                case AUTH_EV_RECV_STA:
                    x->state = AUTH_ST_IDLE;
                    LM_INFO("state machine: AUTH_EV_RECV_STA about to clean up\n");
                    if (msg) AAAFreeMessage(&msg); // if might be needed in frequency
                    // If I register a ResponseHandler then i Free the STA there not here..
                    // but i dont have interest in that now..
                    cdp_session_cleanup(s, NULL);
                    s = 0;
                    rv = 1;
                    break;

                default:
                    LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;
        default:
            LM_ERR("auth_client_statefull_sm_process(): Received event %d while in invalid state %d!\n",
                    event, x->state);
    }
    if (s) {
        if (s->cb) (s->cb)(AUTH_EV_SESSION_MODIFIED, s);
        AAASessionsUnlock(s->hash);
    }
    return rv;
}

/**
 * Authorization Server State-Machine - Statefull
 * \Note - should be called with a lock on the session and will unlock it - do not use it after!
 * @param s
 * @param event
 * @param msg
 */
inline void auth_server_statefull_sm_process(cdp_session_t* s, int event, AAAMessage* msg) {
    cdp_auth_session_t *x;

    if (!s) return;
    x = &(s->u.auth);

    if (s->cb) (s->cb)(event, s);
    LM_DBG("after callback for event %i\n", event);

    switch (x->state) {
        case AUTH_ST_IDLE:
            switch (event) {
                case AUTH_EV_RECV_STR:
                    break;
                case AUTH_EV_RECV_REQ:
                    // The RequestHandler will generate a Send event for the answer
                    // and we will only then now if the user is authorised or not
                    // if its not authorised it will move back to idle and cleanup the session
                    // so no big deal
                    // but this is not the Diameter RFC...
                    x->state = AUTH_ST_OPEN;
                    // execute the cb here because we won't have a chance later
                    if (s->cb) (s->cb)(AUTH_EV_SESSION_MODIFIED, s);
                    // Don't unlock the session hash table because the session is returned to the user
                    // This can only be called from the AAACreateServerAuthSession()!
                    s = 0;
                    break;
                case AUTH_EV_SEND_STA:
                    x->state = AUTH_ST_IDLE;
                    cdp_session_cleanup(s, msg);
                    s = 0;
                    break;

                    /* Just in case we have some lost sessions */
                case AUTH_EV_SESSION_TIMEOUT:
                case AUTH_EV_SESSION_GRACE_TIMEOUT:
                	cdp_session_cleanup(s, msg);
                    s = 0;
                    break;

                default:
                    LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        case AUTH_ST_OPEN:


            if (event == AUTH_EV_SEND_ANS && msg && !is_req(msg)) {
                int rc = get_result_code(msg);
                if (rc >= 2000 && rc < 3000)
                    event = AUTH_EV_SEND_ANS_SUCCESS;
                else
                    event = AUTH_EV_SEND_ANS_UNSUCCESS;
            }


            switch (event) {
                case AUTH_EV_RECV_STR:
                    break;
                case AUTH_EV_SEND_ANS_SUCCESS:
                    x->state = AUTH_ST_OPEN;
                    update_auth_session_timers(x, msg);
                    add_auth_session_timers(x, msg);
                    break;
                case AUTH_EV_SEND_ANS_UNSUCCESS:
                    x->state = AUTH_ST_IDLE;
                    cdp_session_cleanup(s, msg);
                    s = 0;
                    break;
                case AUTH_EV_SEND_ASR:
                    x->state = AUTH_ST_DISCON;
                    break;
                case AUTH_EV_SESSION_TIMEOUT:
                case AUTH_EV_SESSION_GRACE_TIMEOUT:
                    x->state = AUTH_ST_IDLE;
                    LM_DBG("before session cleanup\n");
                    cdp_session_cleanup(s, msg);
                    s = 0;
                    break;
                case AUTH_EV_SEND_STA:
                    LM_ERR("SENDING STA!!!\n");
                    x->state = AUTH_ST_IDLE;
                    cdp_session_cleanup(s, msg);
                    s = 0;
                    break;
                default:
                    LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        case AUTH_ST_DISCON:
            switch (event) {
                case AUTH_EV_RECV_STR:
                    break;
                case AUTH_EV_RECV_ASA:
                case AUTH_EV_RECV_ASA_SUCCESS:
                    x->state = AUTH_ST_IDLE;
                    //cdp_session_cleanup(s,msg);
                    break;
                case AUTH_EV_RECV_ASA_UNSUCCESS:
                    Send_ASR(s, msg);
                    // how many times will this be done?
                    x->state = AUTH_ST_DISCON;
                    break;
                case AUTH_EV_SEND_STA:
                    x->state = AUTH_ST_IDLE;
                    cdp_session_cleanup(s, msg);
                    s = 0;
                    break;
                default:
                    LM_ERR("auth_client_statefull_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        default:
            LM_ERR("auth_client_statefull_sm_process(): Received event %d while in invalid state %d!\n",
                    event, x->state);
    }
    if (s) {
        if (s->cb) (s->cb)(AUTH_EV_SESSION_MODIFIED, s);
        AAASessionsUnlock(s->hash);
    }
}

/**
 * Authorization Client State-Machine - Stateless
 * \Note - should be called with a lock on the session and will unlock it - do not use it after!
 * @param s
 * @param event
 * @param msg
 */
inline void auth_client_stateless_sm_process(cdp_session_t* s, int event, AAAMessage *msg) {
    cdp_auth_session_t *x;
    int rc;
    if (!s) return;
    x = &(s->u.auth);
    switch (x->state) {
        case AUTH_ST_IDLE:
            switch (event) {
                case AUTH_EV_SEND_REQ:
                    x->state = AUTH_ST_PENDING;
                    break;
                default:
                    LM_ERR("auth_client_stateless_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        case AUTH_ST_PENDING:
            if (!is_req(msg)) {
                rc = get_result_code(msg);
                if (rc >= 2000 && rc < 3000 && get_auth_session_state(msg) == NO_STATE_MAINTAINED)
                    event = AUTH_EV_RECV_ANS_SUCCESS;
                else
                    event = AUTH_EV_RECV_ANS_UNSUCCESS;
            }
            switch (event) {
                case AUTH_EV_RECV_ANS_SUCCESS:
                    x->state = AUTH_ST_OPEN;
                    break;
                case AUTH_EV_RECV_ANS_UNSUCCESS:
                    x->state = AUTH_ST_IDLE;
                    break;
                default:
                    LM_ERR("auth_client_stateless_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        case AUTH_ST_OPEN:
            switch (event) {
                case AUTH_EV_SESSION_TIMEOUT:
                    x->state = AUTH_ST_IDLE;
                    break;
                case AUTH_EV_SERVICE_TERMINATED:
                    x->state = AUTH_ST_IDLE;
                    break;
                default:
                    LM_ERR("auth_client_stateless_sm_process(): Received invalid event %d while in state %s!\n",
                            event, auth_states[x->state]);
            }
            break;

        default:
            LM_ERR("auth_client_stateless_sm_process(): Received event %d while in invalid state %d!\n",
                    event, x->state);
    }
    if (s) AAASessionsUnlock(s->hash);
}

/**
 * Authorization Server State-Machine - Stateless
 * \Note - should be called with a lock on the session and will unlock it - do not use it after!
 *
 * @param auth
 * @param event
 * @param msg
 */
inline void auth_server_stateless_sm_process(cdp_session_t* s, int event, AAAMessage* msg) {
    /* empty - no state change, anyway */
    /*
            cdp_auth_session_t *x;
            int rc;
            if (!s) return;
            x = &(s->u.auth);
            switch(x->state){
                    case AUTH_ST_IDLE:
                            switch(event){
                                    default:
                                            LM_ERR("auth_server_stateless_sm_process(): Received invalid event %d while in state %s!\n",
                                                    event,auth_state[x->state]);
                            }
                            break;
                    default:
                            LM_ERR("auth_server_stateless_sm_process(): Received event %d while in invalid state %d!\n",
                                    event,x->state);
            }
     */
    if (s) AAASessionsUnlock(s->hash);
}

/* copies the Origin-Host AVP from the src message in a Destination-Host AVP in the dest message
 * copies the Origin-Realm AVP from the src message in a Destination-Realm AVP in the dest message
 *
 */
int dup_routing_avps(AAAMessage* src, AAAMessage *dest) {

    AAA_AVP * avp;
    str dest_realm;

    if (!src)
        return 1;

    /* Removed By Jason to facilitate use of Diameter clustering (MUX) in SLEE architecture (Realm-routing only) - TODO - check spec */
    /*avp = AAAFindMatchingAVP(src,src->avpList.head,AVP_Origin_Host,0,AAA_FORWARD_SEARCH);
    if(avp && avp->data.s && avp->data.len) {
            LM_DBG("dup_routing_avps: Origin Host AVP present, duplicating %.*s\n",
                            avp->data.len, avp->data.s);
            dest_host = avp->data;
            avp = AAACreateAVP(AVP_Destination_Host,AAA_AVP_FLAG_MANDATORY,0,
                    dest_host.s,dest_host.len,AVP_DUPLICATE_DATA);
            if (!avp) {
                    LM_ERR("dup_routing_avps: Failed creating Destination Host avp\n");
                    goto error;
            }
            if (AAAAddAVPToMessage(dest,avp,dest->avpList.tail)!=AAA_ERR_SUCCESS) {
                    LM_ERR("dup_routing_avps: Failed adding Destination Host avp to message\n");
                    AAAFreeAVP(&avp);
                    goto error;
            }
    }*/

    avp = AAAFindMatchingAVP(src, src->avpList.head, AVP_Origin_Realm, 0, AAA_FORWARD_SEARCH);
    if (avp && avp->data.s && avp->data.len) {
        LM_DBG("dup_routing_avps: Origin Realm AVP present, duplicating %.*s\n",
                avp->data.len, avp->data.s);
        dest_realm = avp->data;
        avp = AAACreateAVP(AVP_Destination_Realm, AAA_AVP_FLAG_MANDATORY, 0,
                dest_realm.s, dest_realm.len, AVP_DUPLICATE_DATA);
        if (!avp) {
            LM_ERR("dup_routing_avps: Failed creating Destination Host avp\n");
            goto error;
        }
        if (AAAAddAVPToMessage(dest, avp, dest->avpList.tail) != AAA_ERR_SUCCESS) {
            LM_ERR("dup_routing_avps: Failed adding Destination Host avp to message\n");
            AAAFreeAVP(&avp);
            goto error;
        }
    }

    return 1;
error:
    return 0;

}

void Send_ASA(cdp_session_t* s, AAAMessage* msg) {
    AAAMessage *asa;
    char x[4];
    AAA_AVP *avp;
    LM_INFO("Send_ASA():  sending ASA\n");
    if (!s) {
        //send an ASA for UNKNOWN_SESSION_ID - use AAASendMessage()
        // msg is the ASR received
        asa = AAANewMessage(IMS_ASA, 0, 0, msg);
        if (!asa) return;

        set_4bytes(x, AAA_SUCCESS);
        AAACreateAndAddAVPToMessage(asa, AVP_Result_Code, AAA_AVP_FLAG_MANDATORY, 0, x, 4);

        AAASendMessage(asa, 0, 0);
    } else {
        // send... many cases... maybe not needed.
        // for now we do the same
        asa = AAANewMessage(IMS_ASA, 0, 0, msg);
        if (!asa) return;

        set_4bytes(x, AAA_SUCCESS);
        AAACreateAndAddAVPToMessage(asa, AVP_Result_Code, AAA_AVP_FLAG_MANDATORY, 0, x, 4);

        avp = AAAFindMatchingAVP(msg, 0, AVP_Origin_Host, 0, 0);
        if (avp) {
            // This is because AAASendMessage is not going to find a route to the
            // the PCRF because TS 29.214 says no Destination-Host and no Auth-Application-Id
            // in the ASA
            LM_INFO("sending ASA to peer %.*s\n", avp->data.len, avp->data.s);
            peer *p;
            p = get_peer_by_fqdn(&avp->data);
            if (!peer_send_msg(p, asa)) {
                if (asa) AAAFreeMessage(&asa); //needed in frequency
            } else
                LM_INFO("success sending ASA\n");
        } else if (!AAASendMessage(asa, 0, 0)) {
            LM_ERR("Send_ASA() : error sending ASA\n");
        }
    }
}

int add_vendor_specific_application_id_group(AAAMessage * msg, unsigned int vendor_id, unsigned int auth_app_id) {
    char x[4];
    AAA_AVP_LIST list_grp = {0, 0};
    AAA_AVP *avp;
    str group = {0, 0};

    set_4bytes(x, vendor_id);
    if (!(avp = AAACreateAVP(AVP_Vendor_Id, AAA_AVP_FLAG_MANDATORY, 0,
            x, 4, AVP_DUPLICATE_DATA))) goto error;
    AAAAddAVPToList(&list_grp, avp);

    set_4bytes(x, auth_app_id);
    if (!(avp = AAACreateAVP(AVP_Auth_Application_Id, AAA_AVP_FLAG_MANDATORY, 0,
            x, 4, AVP_DUPLICATE_DATA))) goto error;
    AAAAddAVPToList(&list_grp, avp);

    group = AAAGroupAVPS(list_grp);
    if (!group.s || !group.len) goto error;

    if (!(avp = AAACreateAVP(AVP_Vendor_Specific_Application_Id, AAA_AVP_FLAG_MANDATORY, 0,
            group.s, group.len, AVP_DUPLICATE_DATA))) goto error;

    if (AAAAddAVPToMessage(msg, avp, msg->avpList.tail) != AAA_ERR_SUCCESS) goto error;

    AAAFreeAVPList(&list_grp);
    shm_free(group.s);
    group.s = NULL;

    return 1;

error:

    AAAFreeAVPList(&list_grp);
    if (group.s) shm_free(group.s);
    return 0;
}

void Send_STR(cdp_session_t* s, AAAMessage* msg) {
    AAAMessage *str = 0;
    AAA_AVP *avp = 0;
    peer *p = 0;
    char x[4];
    LM_DBG("sending STR\n");
    //if (msg) LM_DBG("Send_STR() : sending STR for %d, flags %#1x endtoend %u hopbyhop %u\n",msg->commandCode,msg->flags,msg->endtoendId,msg->hopbyhopId);
    //else LM_DBG("Send_STR() called from AAATerminateAuthSession or some other event\n");
    str = AAACreateRequest(s->application_id, IMS_STR, Flag_Proxyable, s);

    if (!str) {
        LM_ERR("Send_STR(): error creating STR!\n");
        return;
    }
    if (!dup_routing_avps(msg, str)) {
        LM_ERR("Send_STR(): error duplicating routing AVPs!\n");
        AAAFreeMessage(&str);
        return;
    }
    if (s->vendor_id != 0 && !add_vendor_specific_application_id_group(str, s->vendor_id, s->application_id)) {
        LM_ERR("Send_STR(): error adding Vendor-Id-Specific-Application-Id Group!\n");
        AAAFreeMessage(&str);
        return;
    }

    //Richard added this - if timers expire dest realm is not here!
    LM_DBG("Adding dest realm if not there already...\n");
    LM_DBG("Destination realm: [%.*s] \n", s->dest_realm.len, s->dest_realm.s);
    /* Add Destination-Realm AVP, if not already there */
    avp = AAAFindMatchingAVP(str, str->avpList.head, AVP_Destination_Realm, 0, AAA_FORWARD_SEARCH);
    if (!avp) {
        avp = AAACreateAVP(AVP_Destination_Realm, AAA_AVP_FLAG_MANDATORY, 0,
                s->dest_realm.s, s->dest_realm.len, AVP_DUPLICATE_DATA);
        AAAAddAVPToMessage(str, avp, str->avpList.tail);
    }




    set_4bytes(x, s->application_id);
    avp = AAACreateAVP(AVP_Auth_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, x, 4, AVP_DUPLICATE_DATA);
    AAAAddAVPToMessage(str, avp, str->avpList.tail);

    set_4bytes(x, 4); // Diameter_administrative
    avp = AAACreateAVP(AVP_Termination_Cause, AAA_AVP_FLAG_MANDATORY, 0, x, 4, AVP_DUPLICATE_DATA);
    AAAAddAVPToMessage(str, avp, str->avpList.tail);
    //todo - add all the other avps

    /* we are already locked on the auth session*/
    p = get_routing_peer(s, str);

    if (!p) {
        LM_ERR("unable to get routing peer in Send_STR \n");
        if (str) AAAFreeMessage(&str); //needed in frequency
        return;
    }
    //if (str) LM_CRIT("Send_STR() : sending STR  %d, flags %#1x endtoend %u hopbyhop %u\n",str->commandCode,str->flags,str->endtoendId,str->hopbyhopId);
    if (!peer_send_msg(p, str)) {
        LM_DBG("Send_STR peer_send_msg return error!\n");
        if (str) AAAFreeMessage(&str); //needed in frequency
    } else {
        LM_DBG("success sending STR\n");
    }
}

void Send_ASR(cdp_session_t* s, AAAMessage* msg) {
    AAAMessage *asr = 0;
    AAA_AVP *avp = 0;
    peer *p = 0;
    char x[4];
    LM_DBG("Send_ASR() : sending ASR\n");
    asr = AAACreateRequest(s->application_id, IMS_ASR, Flag_Proxyable, s);

    if (!asr) {
        LM_ERR("Send_ASR(): error creating ASR!\n");
        return;
    }

    set_4bytes(x, s->application_id);
    avp = AAACreateAVP(AVP_Auth_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, x, 4, AVP_DUPLICATE_DATA);
    AAAAddAVPToMessage(asr, avp, asr->avpList.tail);

    set_4bytes(x, 3); // Not specified
    avp = AAACreateAVP(AVP_IMS_Abort_Cause, AAA_AVP_FLAG_MANDATORY, 0, x, 4, AVP_DUPLICATE_DATA);
    AAAAddAVPToMessage(asr, avp, asr->avpList.tail);
    //todo - add all the other avps

    p = get_routing_peer(s, asr);
    if (!p) {
        LM_ERR("unable to get routing peer in Send_ASR \n");
        if (asr) AAAFreeMessage(&asr); //needed in frequency
    }

    if (!peer_send_msg(p, asr)) {
        if (asr) AAAFreeMessage(&asr); //needed in frequency
    } else
        LM_DBG("success sending ASR\n");
}
