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

#include "stats.h"
#include "../cdp/cdp_load.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "api.h"
#include "cxdx_avp.h"

#include "cxdx_mar.h"
#include "authorize.h"
#include "../../lib/ims/ims_getters.h"
#include "utils.h"

static str empty_s = {0, 0};

static str s_empty = {0, 0};
extern str auth_scheme_types[];

extern str scscf_name_str;

//we use pseudo variables to communicate back to config file this takes the result and converys to a return code, publishes it a pseudo variable

int create_return_code(int result) {
    int rc;
    int_str avp_val, avp_name;
    avp_name.s.s = "maa_return_code";
    avp_name.s.len = 15;

    //build avp spec for uaa_return_code
    avp_val.n = result;

    rc = add_avp(AVP_NAME_STR, avp_name, avp_val);

    if (rc < 0)
        LM_ERR("couldnt create AVP\n");
    else
        LM_INFO("created AVP successfully : [%.*s] - [%d]\n", avp_name.s.len, avp_name.s.s, result);

    return 1;
}

void free_saved_transaction_data(saved_transaction_t* data) {
    if (!data)
        return;
    LM_DBG("Freeing saved transaction data: async\n");
    if (data->realm.s && data->realm.len) {
        shm_free(data->realm.s);
        data->realm.len = 0;
    }

    shm_free(data);
}

void async_cdp_callback(int is_timeout, void *param, AAAMessage *maa, long elapsed_msecs) {
    int i, j;
    int rc = -1, experimental_rc = -1;
    saved_transaction_t* data = (saved_transaction_t*) param;
    struct cell *t = 0;
    int result = CSCF_RETURN_TRUE;
    int sip_number_auth_items;
    struct auth_data_item_list *adi_list = 0;
    AAA_AVP *auth_data;
    auth_data = 0;
    int item_number;
    str authenticate = {0, 0}, authorization2 = {0, 0}, ck = {0, 0}, ik = {0, 0}, ip = {0, 0}, ha1 = {0, 0};
    str line_identifier = {0, 0};
    str response_auth = {0, 0}, digest_realm = {0, 0};
    auth_vector *av = 0, **avlist = 0;
    HASHHEX ha1_hex;
    HASHHEX result_hex;
    str etsi_nonce = {0, 0};
    str private_identity, public_identity;
    str algorithm;

    if (is_timeout) {
    	update_stat(stat_mar_timeouts, 1);
        LM_ERR("Transaction timeout - did not get MAA\n");
        result = CSCF_RETURN_ERROR;
        goto error;
    }
    if (!maa) {
        LM_ERR("Error sending message via CDP\n");
        result = CSCF_RETURN_ERROR;
        goto error;
    }

    update_stat(mar_replies_received, 1);
    update_stat(mar_replies_response_time, elapsed_msecs);

    if (tmb.t_lookup_ident(&t, data->tindex, data->tlabel) < 0) {
        LM_ERR("t_continue: transaction not found\n");
        result = CSCF_RETURN_ERROR;
        goto error;
    }

    /* get the private_identity */
    private_identity = cscf_get_private_identity(t->uas.request, data->realm);
    if (!private_identity.len) {
        LM_ERR("No private identity specified (Authorization: username)\n");
        stateful_request_reply_async(t, t->uas.request, 403, MSG_403_NO_PRIVATE);
        result = CSCF_RETURN_FALSE;
        goto error;
    }
    /* get the public_identity */
    public_identity = cscf_get_public_identity(t->uas.request);
    if (!public_identity.len) {
        LM_ERR("No public identity specified (To:)\n");
        stateful_request_reply_async(t, t->uas.request, 403, MSG_403_NO_PUBLIC);
        result = CSCF_RETURN_FALSE;
        goto error;
    }

    //get each individual element from the MAA
    cxdx_get_result_code(maa, &rc);
    cxdx_get_experimental_result_code(maa, &experimental_rc);
    cxdx_get_sip_number_auth_items(maa, &sip_number_auth_items);

    //now assign the auth_data_item elements
    //there can be many of these in the MAA
    struct auth_data_item *adi;
    int adi_len;
    char *p;
    while ((cxdx_get_auth_data_item_answer(maa, &auth_data, &item_number,
            &algorithm, &authenticate, &authorization2,
            &ck, &ik,
            &ip,
            &ha1, &response_auth, &digest_realm,
            &line_identifier))) {

        //create an auth_data_item for each entry in the MAA
        adi_len = sizeof (struct auth_data_item) +authenticate.len + authorization2.len + ck.len + ik.len + ip.len + ha1.len + line_identifier.len + response_auth.len + digest_realm.len + algorithm.len;
        adi = (struct auth_data_item*) shm_malloc(adi_len);
        if (!adi) {
            LM_CRIT("Out of memory!\n");
            result = CSCF_RETURN_ERROR;
            goto done;
        }

        memset(adi, 0, adi_len);

        //put all elements in the auth_data_item entry
        p = (char*) (adi + 1);

        adi->authenticate.s = p;
        adi->authenticate.len = authenticate.len;
        memcpy(p, authenticate.s, authenticate.len);
        p += authenticate.len;

        adi->authorization.s = p;
        adi->authorization.len = authorization2.len;
        memcpy(p, authorization2.s, authorization2.len);
        p += authorization2.len;

        adi->auth_scheme.s = p;
        adi->auth_scheme.len = algorithm.len;
        memcpy(p, algorithm.s, algorithm.len);
        p += algorithm.len;

        adi->ck.s = p;
        adi->ck.len = ck.len;
        memcpy(p, ck.s, ck.len);
        p += ck.len;

        adi->ik.s = p;
        adi->ik.len = ik.len;
        memcpy(p, ik.s, ik.len);
        p += ik.len;

        adi->ip.s = p;
        adi->ip.len = ip.len;
        memcpy(p, ip.s, ip.len);
        p += ip.len;

        adi->ha1.s = p;
        adi->ha1.len = ha1.len;
        memcpy(p, ha1.s, ha1.len);
        p += ha1.len;

        adi->line_identifier.s = p;
        adi->line_identifier.len = line_identifier.len;
        memcpy(p, line_identifier.s, line_identifier.len);
        p += line_identifier.len;

        adi->response_auth.s = p;
        adi->response_auth.len = response_auth.len;
        memcpy(p, response_auth.s, response_auth.len);
        p += response_auth.len;

        adi->digest_realm.s = p;
        adi->digest_realm.len = digest_realm.len;
        memcpy(p, digest_realm.s, digest_realm.len);
        p += digest_realm.len;

        if (p != (((char*) adi) + adi_len)) {
            LM_CRIT("buffer overflow\n");
            shm_free(adi);
            adi = 0;
            result = CSCF_RETURN_ERROR;
            goto done;
        }
        auth_data->code = -auth_data->code;
        adi->item_number = item_number;

        int len = sizeof (struct auth_data_item_list);
        adi_list = (struct auth_data_item_list*) shm_malloc(len);
        memset(adi_list, 0, len);

        if (adi_list->first == 0) {
            adi_list->first = adi_list->last = adi;
        } else {
            adi_list->last->next = adi;
            adi->previous = adi_list->last;
            adi_list->last = adi;
        }
    }

    if (!(rc) && !(experimental_rc)) {
        stateful_request_reply_async(t, t->uas.request, 480, MSG_480_DIAMETER_MISSING_AVP);
        result = CSCF_RETURN_FALSE;
        goto done;
    }

    switch (rc) {
        case -1:
            switch (experimental_rc) {
                case RC_IMS_DIAMETER_ERROR_USER_UNKNOWN:
                    stateful_request_reply_async(t, t->uas.request, 403, MSG_403_USER_UNKNOWN);
                    result = CSCF_RETURN_FALSE;
                    break;
                case RC_IMS_DIAMETER_ERROR_IDENTITIES_DONT_MATCH:
                    stateful_request_reply_async(t, t->uas.request, 403, MSG_403_IDENTITIES_DONT_MATCH);
                    result = CSCF_RETURN_FALSE;
                    break;
                case RC_IMS_DIAMETER_ERROR_AUTH_SCHEME_NOT_SUPPORTED:
                    stateful_request_reply_async(t, t->uas.request, 403, MSG_403_AUTH_SCHEME_UNSOPPORTED);
                    result = CSCF_RETURN_FALSE;
                    break;

                default:
                    stateful_request_reply_async(t, t->uas.request, 403, MSG_403_UNKOWN_EXPERIMENTAL_RC);
                    result = CSCF_RETURN_FALSE;
            }
            break;

        case AAA_UNABLE_TO_COMPLY:
            stateful_request_reply_async(t, t->uas.request, 403, MSG_403_UNABLE_TO_COMPLY);
            result = CSCF_RETURN_FALSE;
            break;

        case AAA_SUCCESS:
            goto success;
            break;

        default:
            stateful_request_reply_async(t, t->uas.request, 403, MSG_403_UNKOWN_RC);
            result = CSCF_RETURN_FALSE;
    }

    goto done;

success:

    if (!sip_number_auth_items) {
        stateful_request_reply_async(t, t->uas.request, 403, MSG_403_NO_AUTH_DATA);
        result = CSCF_RETURN_FALSE;
        goto done;
    }

    avlist = shm_malloc(sizeof (auth_vector *) * sip_number_auth_items);
    if (!avlist) {
        stateful_request_reply_async(t, t->uas.request, 403, MSG_480_HSS_ERROR);
        result = CSCF_RETURN_FALSE;
        goto done;
    }

    sip_number_auth_items = 0;

    struct auth_data_item *tmp;
    tmp = adi_list->first;

    while (tmp) {

        if (tmp->ip.len)
            av = new_auth_vector(tmp->item_number, tmp->auth_scheme, empty_s,
                tmp->ip, empty_s, empty_s);
        else if (tmp->line_identifier.len)
            av = new_auth_vector(tmp->item_number, tmp->auth_scheme, empty_s,
                line_identifier, empty_s, empty_s);
        else if (tmp->ha1.len) {
            if (tmp->response_auth.len) //HSS check
            {
                memset(ha1_hex, 0, HASHHEXLEN + 1);
                memcpy(ha1_hex, tmp->ha1.s,
                        tmp->ha1.len > HASHHEXLEN ? 32 : tmp->ha1.len);

                etsi_nonce.len = tmp->authenticate.len / 2;
                etsi_nonce.s = pkg_malloc(etsi_nonce.len);
                if (!etsi_nonce.s) {
                    LM_ERR("error allocating %d bytes\n", etsi_nonce.len);
                    goto done;
                }
                etsi_nonce.len = base16_to_bin(tmp->authenticate.s,
                        tmp->authenticate.len, etsi_nonce.s);

                calc_response(ha1_hex, &etsi_nonce, &empty_s, &empty_s,
                        &empty_s, 0, &(t->uas.request->first_line.u.request.method),
                        &scscf_name_str, 0, result_hex);
                pkg_free(etsi_nonce.s);

                if (!tmp->response_auth.len == 32
                        || strncasecmp(tmp->response_auth.s, result_hex, 32)) {
                    LM_ERR("The HSS' Response-Auth is different from what we compute locally!\n"
                            " BUT! If you sent an MAR with auth scheme unknown (HSS-Selected Authentication), this is normal.\n"
                            "HA1=\t|%s|\nNonce=\t|%.*s|\nMethod=\t|%.*s|\nuri=\t|%.*s|\nxresHSS=\t|%.*s|\nxresSCSCF=\t|%s|\n",
                            ha1_hex,
                            tmp->authenticate.len, tmp->authenticate.s,
                            t->uas.request->first_line.u.request.method.len, t->uas.request->first_line.u.request.method.s,
                            scscf_name_str.len, scscf_name_str.s,
                            tmp->response_auth.len, tmp->response_auth.s,
                            result_hex);
                    //stateful_register_reply(msg,514,MSG_514_HSS_AUTH_FAILURE);
                    //goto done;
                }
            }
            av = new_auth_vector(tmp->item_number, tmp->auth_scheme,
                    tmp->authenticate, tmp->ha1, empty_s, empty_s);
        } else
            av = new_auth_vector(tmp->item_number, tmp->auth_scheme,
                tmp->authenticate, tmp->authorization, tmp->ck, tmp->ik);

        if (sip_number_auth_items == 0)
            avlist[sip_number_auth_items++] = av;
        else {
            i = sip_number_auth_items;
            while (i > 0 && avlist[i - 1]->item_number > av->item_number)
                i--;
            for (j = sip_number_auth_items; j > i; j--)
                avlist[j] = avlist[j - 1];
            avlist[i] = av;
            sip_number_auth_items++;
        }

        //TODO need to confirm that removing this has done no problems
        //tmp->auth_data->code = -tmp->auth_data->code;

        tmp = tmp->next;
    }

    //MAA returns a whole list of av! Which should we use?
    //right now we take the first and put the rest in the AV queue
    //then we use the first one and then add it to the queue as sent!

    for (i = 1; i < sip_number_auth_items; i++) {
        if (!add_auth_vector(private_identity, public_identity, avlist[i]))
            free_auth_vector(avlist[i]);
    }

    if (!data->is_resync) {
		if (!pack_challenge(t->uas.request, data->realm, avlist[0], data->is_proxy_auth)) {
			stateful_request_reply_async(t, t->uas.request, 500, MSG_500_PACK_AV);
			result = CSCF_RETURN_FALSE;
			goto done;
		}

		if (data->is_proxy_auth)
			stateful_request_reply_async(t, t->uas.request, 407, MSG_407_CHALLENGE);
		else
			stateful_request_reply_async(t, t->uas.request, 401, MSG_401_CHALLENGE);
    }

done:

	if (avlist) {
		if (!data->is_resync)	//only start the timer if we used the vector above - we dont use it resync mode
		start_reg_await_timer(avlist[0]); //start the timer to remove stale or unused Auth Vectors

		//now we add it to the queue as sent as we have already sent the challenge and used it and set the status to SENT
		if (!add_auth_vector(private_identity, public_identity, avlist[0]))
			free_auth_vector(avlist[0]);
	}

    //free memory
    if (maa) cdpb.AAAFreeMessage(&maa);
    if (avlist) {
        shm_free(avlist);
        avlist = 0;
    }

    if (adi_list) {
        struct auth_data_item *tmp1 = adi_list->first;
        while (tmp1) {
            struct auth_data_item *tmp2 = tmp1->next;
            shm_free(tmp1);
            tmp1 = tmp2;
        }
        shm_free(adi_list);
        adi_list = 0;
    }

    LM_DBG("DBG:UAR Async CDP callback: ... Done resuming transaction\n");
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

    //make sure we delete any private lumps we created
    create_return_code(result);
    if (t) {
        del_nonshm_lump_rpl(&t->uas.request->reply_lump);
        tmb.unref_cell(t);
    }
    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_transaction_data(data);
    return;

error:
    //don't need to set result code as by default it is ERROR!

    if (t) {
        del_nonshm_lump_rpl(&t->uas.request->reply_lump);
        tmb.unref_cell(t);
    }
    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_transaction_data(data);
}

/**
 * Create and send a Multimedia-Authentication-Request and returns the parsed Answer structure.
 * This function retrieves authentication vectors from the HSS.
 * @param msg - the SIP message to send for
 * @parma public_identity - the public identity of the user
 * @param private_identity - the private identity of the user
 * @param count - how many authentication vectors to ask for
 * @param algorithm - for which algorithm
 * @param authorization - the authorization value
 * @param server_name - local name of the S-CSCF to save on the HSS
 * @returns the parsed maa struct
 */
int cxdx_send_mar(struct sip_msg *msg, str public_identity, str private_identity,
        unsigned int count, str algorithm, str authorization, str server_name, saved_transaction_t* transaction_data) {
    AAAMessage *mar = 0;
    AAASession *session = 0;

    session = cdpb.AAACreateSession(0);

    mar = cdpb.AAACreateRequest(IMS_Cx, IMS_MAR, Flag_Proxyable, session);
    if (session) {
        cdpb.AAADropSession(session);
        session = 0;
    }
    if (!mar) goto error1;

    if (!cxdx_add_destination_realm(mar, cxdx_dest_realm)) goto error1;

    if (!cxdx_add_vendor_specific_appid(mar, IMS_vendor_id_3GPP, IMS_Cx, 0 /*IMS_Cx*/)) goto error1;
    if (!cxdx_add_auth_session_state(mar, 1)) goto error1;

    if (!cxdx_add_public_identity(mar, public_identity)) goto error1;
    if (!cxdx_add_user_name(mar, private_identity)) goto error1;
    if (!cxdx_add_sip_number_auth_items(mar, count)) goto error1;
    if (algorithm.len == auth_scheme_types[AUTH_HTTP_DIGEST_MD5].len &&
            strncasecmp(algorithm.s, auth_scheme_types[AUTH_HTTP_DIGEST_MD5].s, algorithm.len) == 0) {
        if (!cxdx_add_sip_auth_data_item_request(mar, algorithm, authorization, private_identity, cxdx_dest_realm,
                msg->first_line.u.request.method, server_name)) goto error1;
    } else {
        if (!cxdx_add_sip_auth_data_item_request(mar, algorithm, authorization, private_identity, cxdx_dest_realm,
                msg->first_line.u.request.method, s_empty)) goto error1;
    }
    if (!cxdx_add_server_name(mar, server_name)) goto error1;

    if (cxdx_forced_peer.len)
        cdpb.AAASendMessageToPeer(mar, &cxdx_forced_peer, (void*) async_cdp_callback, (void*) transaction_data);
    else
        cdpb.AAASendMessage(mar, (void*) async_cdp_callback, (void*) transaction_data);


    LM_DBG("Successfully sent async diameter\n");

    return 0;

error1: //Only free MAR IFF is has not been passed to CDP
    if (mar) cdpb.AAAFreeMessage(&mar);
    LM_ERR("Error occurred trying to send MAR\n");
    return -1;
}

