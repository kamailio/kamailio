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

#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../basex.h"
#include "../../hashes.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "../../usr_avp.h"
#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "../cdp/diameter.h"
#include "../cdp/diameter_ims_code_result.h"

#include "cxdx_mar.h"
#include "cxdx_avp.h"


#include "../../lib/ims/ims_getters.h"
#include "../tm/tm_load.h"
#include "api.h"
#include "authims_mod.h"
#include "authorize.h"
#include "utils.h"
#include "../../action.h" /* run_actions */

extern unsigned char registration_default_algorithm_type; /**< fixed default algorithm for registration (if none present)	*/
extern struct tm_binds tmb;
extern struct cdp_binds cdpb;

extern str registration_qop_str; /**< the qop options to put in the authorization challenges */
extern int av_request_at_sync; /**< how many auth vectors to request in a sync MAR 		*/
extern int av_request_at_once; /**< how many auth vectors to request in a MAR 				*/
extern int auth_vector_timeout;
extern int auth_data_timeout; /**< timeout for a hash entry to expire when empty in sec 	*/
extern int auth_used_vector_timeout;
extern int add_authinfo_hdr;
extern int max_nonce_reuse;
extern str scscf_name_str;
extern int ignore_failed_auth;
extern int av_check_only_impu;

auth_hash_slot_t *auth_data; /**< Authentication vector hash table */
static int act_auth_data_hash_size = 0; /**< authentication vector hash table size */

static str empty_s = {0, 0};

str S_WWW = {"WWW", 3};
str S_Proxy = {"Proxy", 5};
str S_Authorization_AKA = {"%.*s-Authenticate: Digest realm=\"%.*s\","
    " nonce=\"%.*s\", algorithm=%.*s, ck=\"%.*s\", ik=\"%.*s\"%.*s\r\n", 107};
str S_Authorization_MD5 = {"%.*s-Authenticate: Digest realm=\"%.*s\","
    " nonce=\"%.*s\", algorithm=%.*s%.*s\r\n", 102};

str algorithm_types[] = {
    {"unknown", 7},
    {"AKAv1-MD5", 9},
    {"AKAv2-MD5", 9},
    {"Early-IMS", 9},
    {"MD5", 3},
    {"CableLabs-Digest", 16},
    {"3GPP-Digest", 11},
    {"TISPAN-HTTP_DIGEST_MD5", 22},
    {"NASS-Bundled", 12},
    {0, 0}
};

str auth_scheme_types[] = {
    {"unknown", 7},
    {"Digest-AKAv1-MD5", 16},
    {"Digest-AKAv2-MD5", 16},
    {"Early-IMS-Security", 18},
    {"Digest-MD5", 10},
    {"Digest", 6},
    {"SIP Digest", 10},
    {"HTTP_DIGEST_MD5", 15},
    {"NASS-Bundled", 12},
    {0, 0}
};

/**
 * Convert the SIP Algorithm to its type
 * @param algorithm - the SIP Algorithm
 * @returns the algorithm type
 */
unsigned char get_algorithm_type(str algorithm) {
    int i;
    for (i = 0; algorithm_types[i].len > 0; i++)
        if (algorithm_types[i].len == algorithm.len
                && strncasecmp(algorithm_types[i].s, algorithm.s, algorithm.len)
                == 0)
            return i;
    return AUTH_UNKNOWN;
}

/**
 * Convert the Diameter Authorization Scheme to its type
 * @param scheme - the Diameter Authorization Scheme
 * @returns the SIP Algorithm
 */
unsigned char get_auth_scheme_type(str scheme) {
    int i;
    for (i = 0; auth_scheme_types[i].len > 0; i++)
        if (auth_scheme_types[i].len == scheme.len &&
                strncasecmp(auth_scheme_types[i].s, scheme.s, scheme.len) == 0)
            return i;
    return AUTH_UNKNOWN;
}

static inline int get_ha1(struct username* _username, str* _domain,
        const str* _table, char* _ha1, db1_res_t** res) {

    return 0;
}

/*
 * Authorize digest credentials
 */
static int digest_authenticate(struct sip_msg* msg, str *realm,
        str *table, hdr_types_t hftype) {
    return 0;
}

/**
 * Starts the reg_await_timer for an authentication vector.
 * @param av - the authentication vector
 */
inline void start_reg_await_timer(auth_vector *av) {
    av->expires = get_ticks() + auth_vector_timeout;
    av->status = AUTH_VECTOR_SENT;
}

/**
 * Timer callback for reg await timers.
 * Drops the auth vectors that have been sent and are expired
 * Also drops the useless auth vectors - used and no longer needed
 * @param ticks - what's the time
 * @param param - a given parameter to be called with
 */
void reg_await_timer(unsigned int ticks, void* param) {
    auth_userdata *aud, *aud_next;
    auth_vector *av, *av_next;
    int i;

    LM_DBG("Looking for expired/useless at %d\n", ticks);
    for (i = 0; i < act_auth_data_hash_size; i++) {
        auth_data_lock(i);
        aud = auth_data[i].head;
        while (aud) {
            LM_DBG("Slot %4d <%.*s>\n",
                    aud->hash, aud->private_identity.len, aud->private_identity.s);
            aud_next = aud->next;
            av = aud->head;
            while (av) {
                LM_DBG(".. AV %4d - %d Exp %3d  %p\n",
                        av->item_number, av->status, (int) av->expires, av);
                av_next = av->next;
                if (av->status == AUTH_VECTOR_USELESS ||
                        ((av->status == AUTH_VECTOR_USED || av->status == AUTH_VECTOR_SENT) && av->expires < ticks)
                        ) {
                    LM_DBG("... dropping av %d - %d\n",
                            av->item_number, av->status);
                    if (av->prev) av->prev->next = av->next;
                    else aud->head = av->next;
                    if (av->next) av->next->prev = av->prev;
                    else aud->tail = av->prev;
                    free_auth_vector(av);
                }
                av = av_next;
            }
            if (!aud->head) {
                if (aud->expires == 0) {
                    LM_DBG("... started empty aud drop timer\n");
                    aud->expires = ticks + auth_data_timeout;
                } else
                    if (aud->expires < ticks) {
                    LM_DBG("... dropping aud \n");
                    if (aud->prev) aud->prev->next = aud->next;
                    else auth_data[i].head = aud->next;
                    if (aud->next) aud->next->prev = aud->prev;
                    else auth_data[i].tail = aud->prev;
                    free_auth_userdata(aud);
                }
            } else aud->expires = 0;

            aud = aud_next;
        }
        auth_data_unlock(i);
    }
    LM_DBG("[DONE] Looking for expired/useless at %d\n", ticks);
}

/*
 * Authenticate using Proxy-Authorize header field
 */

/*
int proxy_authenticate(struct sip_msg* _m, char* _realm, char* _table) {
    str srealm;
    str stable;

    if (_table == NULL) {
        LM_ERR("invalid table parameter\n");
        return -1;
    }

    stable.s = _table;
    stable.len = strlen(stable.s);

    if (get_str_fparam(&srealm, _m, (fparam_t*) _realm) < 0) {
        LM_ERR("failed to get realm value\n");
        return -1; //AUTH_ERROR;
    }

    if (srealm.len == 0) {
        LM_ERR("invalid realm parameter - empty value\n");
        return -1; //AUTH_ERROR;
    }
    LM_DBG("realm value [%.*s]\n", srealm.len, srealm.s);

    return digest_authenticate(_m, &srealm, &stable, HDR_PROXYAUTH_T);
}
 */
int challenge(struct sip_msg* msg, char* str1, char* alg, int is_proxy_auth, char *route) {

    str realm = {0, 0}, algo = {0,0};
    unsigned int aud_hash;
    str private_identity, public_identity, auts = {0, 0}, nonce = {0, 0};
    auth_vector *av = 0;
    int algo_type = 0;
    str route_name;

    saved_transaction_t* saved_t;
    tm_cell_t *t = 0;
    cfg_action_t* cfg_action;

    if (fixup_get_svalue(msg, (gparam_t*) route, &route_name) != 0) {
        LM_ERR("no async route block for assign_server_unreg\n");
        return -1;
    }
    
    if (!alg) {
	LM_DBG("no algorithm specified in cfg... using default\n");
    } else {
	if (get_str_fparam(&algo, msg, (fparam_t*) alg) < 0) {
	    LM_ERR("failed to get auth algorithm\n");
	    return -1;
	}
    }
    
    LM_DBG("Looking for route block [%.*s]\n", route_name.len, route_name.s);
    int ri = route_get(&main_rt, route_name.s);
    if (ri < 0) {
        LM_ERR("unable to find route block [%.*s]\n", route_name.len, route_name.s);
        return -1;
    }
    cfg_action = main_rt.rlist[ri];
    if (cfg_action == NULL) {
        LM_ERR("empty action lists in route block [%.*s]\n", route_name.len, route_name.s);
        return -1;
    }

    if (get_str_fparam(&realm, msg, (fparam_t*) str1) < 0) {
        LM_ERR("failed to get realm value\n");
        return CSCF_RETURN_ERROR;
    }

    if (realm.len == 0) {
        LM_ERR("invalid realm value - empty content\n");
        return CSCF_RETURN_ERROR;
    }

    create_return_code(CSCF_RETURN_ERROR);

    LM_DBG("Need to challenge for realm [%.*s]\n", realm.len, realm.s);

    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("This message is not a request\n");
        return CSCF_RETURN_ERROR;
    }
    if (!is_proxy_auth) {
        LM_DBG("Checking if REGISTER is authorized for realm [%.*s]...\n", realm.len, realm.s);

        /* First check the parameters */
        if (msg->first_line.u.request.method.len != 8 ||
                memcmp(msg->first_line.u.request.method.s, "REGISTER", 8) != 0) {
            LM_ERR("This message is not a REGISTER request\n");
            return CSCF_RETURN_ERROR;
        }
    }

    /* get the private_identity */
    private_identity = cscf_get_private_identity(msg, realm);
    if (!private_identity.len) {
        LM_ERR("No private identity specified (Authorization: username)\n");
        stateful_request_reply(msg, 403, MSG_403_NO_PRIVATE);
        return CSCF_RETURN_BREAK;
    }
    /* get the public_identity */
    public_identity = cscf_get_public_identity(msg);
    if (!public_identity.len) {
        LM_ERR("No public identity specified (To:)\n");
        stateful_request_reply(msg, 403, MSG_403_NO_PUBLIC);
        return CSCF_RETURN_BREAK;
    }

    if (algo.len > 0) {
	algo_type = get_algorithm_type(algo);
    } else {
	algo_type = registration_default_algorithm_type;
    }
    
//    /* check if it is a synchronization request */
//    //TODO this is MAR syncing - have removed it currently - TOD maybe put back in
//    auts = ims_get_auts(msg, realm, is_proxy_auth);
//    if (auts.len) {
//        LM_DBG("IMS Auth Synchronization requested <%.*s>\n", auts.len, auts.s);
//
//        nonce = ims_get_nonce(msg, realm);
//        if (nonce.len == 0) {
//            LM_DBG("Nonce not found (Authorization: nonce)\n");
//            stateful_request_reply(msg, 403, MSG_403_NO_NONCE);
//            return CSCF_RETURN_BREAK;
//        }
//        av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_USED, &nonce, &aud_hash);
//        if (!av)
//            av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_SENT, &nonce, &aud_hash);
//
//        if (!av) {
//            LM_ERR("nonce not recognized as sent, no sync!\n");
//            auts.len = 0;
//            auts.s = 0;
//        } else {
//            av->status = AUTH_VECTOR_USELESS;
//            auth_data_unlock(aud_hash);
//            av = 0;
//            resync = 1;
//        }
//    }

    //RICHARD changed this
    //Previous approach sent MAR, got MAA then put auth vectors into queue
    //Then we try and get that auth vector out the queue (it might be used by someone else so we loop)
    //new approach
    //we do MAR get MAA (asynchronously) get auth vector use it to pack the vector etc.
    //set it to sent and set an expires on it
    //then add it to the queue!

    /* loop because some other process might steal the auth_vector that we just retrieved */
    //while (!(av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_UNUSED, 0, &aud_hash))) {

    if ((av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_UNUSED, 0, &aud_hash))) {
        if (!av) {
            LM_ERR("Error retrieving an auth vector\n");
            return CSCF_RETURN_ERROR;
        }

        if (!pack_challenge(msg, realm, av, is_proxy_auth)) {
            stateful_request_reply(msg, 500, MSG_500_PACK_AV);
            auth_data_unlock(aud_hash);
            return CSCF_RETURN_ERROR;
        }

        start_reg_await_timer(av); //start the timer to remove stale or unused Auth Vectors
        if (is_proxy_auth) {
            stateful_request_reply(msg, 407, MSG_407_CHALLENGE);
        } else {
            stateful_request_reply(msg, 401, MSG_401_CHALLENGE);
        }
        auth_data_unlock(aud_hash);

    } else {

        //before we send lets suspend the transaction
        t = tmb.t_gett();
        if (t == NULL || t == T_UNDEFINED) {
            if (tmb.t_newtran(msg) < 0) {
                LM_ERR("cannot create the transaction for MAR async\n");
                stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
                return CSCF_RETURN_BREAK;
            }
            t = tmb.t_gett();
            if (t == NULL || t == T_UNDEFINED) {
                LM_ERR("cannot lookup the transaction\n");
                stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
                return CSCF_RETURN_BREAK;
            }
        }

        saved_t = shm_malloc(sizeof (saved_transaction_t));
        if (!saved_t) {
            LM_ERR("no more memory trying to save transaction state\n");
            return CSCF_RETURN_ERROR;

        }
        memset(saved_t, 0, sizeof (saved_transaction_t));
        saved_t->act = cfg_action;

        saved_t->realm.s = (char*) shm_malloc(realm.len + 1);
        if (!saved_t->realm.s) {
            LM_ERR("no more memory trying to save transaction state : callid\n");
            shm_free(saved_t);
            return CSCF_RETURN_ERROR;
        }
        memset(saved_t->realm.s, 0, realm.len + 1);
        memcpy(saved_t->realm.s, realm.s, realm.len);
        saved_t->realm.len = realm.len;

        saved_t->is_proxy_auth = is_proxy_auth;

        LM_DBG("Suspending SIP TM transaction\n");
        if (tmb.t_suspend(msg, &saved_t->tindex, &saved_t->tlabel) < 0) {
            LM_ERR("failed to suspend the TM processing\n");
            free_saved_transaction_data(saved_t);

            stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
            return CSCF_RETURN_BREAK;
        }

        if (multimedia_auth_request(msg, public_identity, private_identity, av_request_at_once,
                auth_scheme_types[algo_type], nonce, auts, scscf_name_str, saved_t)!=0) {
            LM_ERR("ERR:I_MAR: Error sending MAR or MAR time-out\n");
            tmb.t_cancel_suspend(saved_t->tindex, saved_t->tlabel);
            free_saved_transaction_data(saved_t);
            stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
            return CSCF_RETURN_BREAK;
        }
    }
    return CSCF_RETURN_BREAK;
}
int www_challenge2(struct sip_msg* msg, char* _route, char* str1, char* str2) {
    return challenge(msg, str1, 0, 0, _route);
}

int www_challenge3(struct sip_msg* msg, char* _route, char* str1, char* str2) {
    return challenge(msg, str1, str2, 0, _route);
}

int www_resync_auth(struct sip_msg* msg, char* _route, char* str1, char* str2) {

    str realm = {0, 0};
    unsigned int aud_hash;
    str private_identity, public_identity, auts = {0, 0}, nonce = {0, 0};
    auth_vector *av = 0;
    int algo_type;
    int is_proxy_auth=0;
    str route_name;

    saved_transaction_t* saved_t;
    tm_cell_t *t = 0;
    cfg_action_t* cfg_action;

    if (fixup_get_svalue(msg, (gparam_t*) _route, &route_name) != 0) {
        LM_ERR("no async route block for assign_server_unreg\n");
        return -1;
    }

    LM_DBG("Looking for route block [%.*s]\n", route_name.len, route_name.s);
    int ri = route_get(&main_rt, route_name.s);
    if (ri < 0) {
        LM_ERR("unable to find route block [%.*s]\n", route_name.len, route_name.s);
        return -1;
    }
    cfg_action = main_rt.rlist[ri];
    if (cfg_action == NULL) {
        LM_ERR("empty action lists in route block [%.*s]\n", route_name.len, route_name.s);
        return -1;
    }

    if (get_str_fparam(&realm, msg, (fparam_t*) str1) < 0) {
        LM_ERR("failed to get realm value\n");
        return CSCF_RETURN_ERROR;
    }

    if (realm.len == 0) {
        LM_ERR("invalid realm value - empty content\n");
        return CSCF_RETURN_ERROR;
    }

    create_return_code(CSCF_RETURN_ERROR);

    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("This message is not a request\n");
        return CSCF_RETURN_ERROR;
    }

    /* get the private_identity */
    private_identity = cscf_get_private_identity(msg, realm);
    if (!private_identity.len) {
        LM_ERR("No private identity specified (Authorization: username)\n");
        stateful_request_reply(msg, 403, MSG_403_NO_PRIVATE);
        return CSCF_RETURN_BREAK;
    }
    /* get the public_identity */
    public_identity = cscf_get_public_identity(msg);
    if (!public_identity.len) {
        LM_ERR("No public identity specified (To:)\n");
        stateful_request_reply(msg, 403, MSG_403_NO_PUBLIC);
        return CSCF_RETURN_BREAK;
    }

    algo_type = registration_default_algorithm_type;

    /* check if it is a synchronization request */
    //TODO this is MAR syncing - have removed it currently - TOD maybe put back in
    auts = ims_get_auts(msg, realm, is_proxy_auth);
    if (auts.len) {
        LM_DBG("IMS Auth Synchronization requested <%.*s>\n", auts.len, auts.s);

        nonce = ims_get_nonce(msg, realm);
        if (nonce.len == 0) {
            LM_DBG("Nonce not found (Authorization: nonce)\n");
            stateful_request_reply(msg, 403, MSG_403_NO_NONCE);
            return CSCF_RETURN_BREAK;
        }
        av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_USED, &nonce, &aud_hash);
        if (!av)
            av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_SENT, &nonce, &aud_hash);

        if (!av) {
            LM_ERR("nonce not recognized as sent, no sync!\n");
            auts.len = 0;
            auts.s = 0;
        } else {
            av->status = AUTH_VECTOR_USELESS;
            auth_data_unlock(aud_hash);
            av = 0;
        }
    }

	//before we send lets suspend the transaction
	t = tmb.t_gett();
	if (t == NULL || t == T_UNDEFINED) {
		if (tmb.t_newtran(msg) < 0) {
			LM_ERR("cannot create the transaction for MAR async\n");
			stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
			return CSCF_RETURN_BREAK;
		}
		t = tmb.t_gett();
		if (t == NULL || t == T_UNDEFINED) {
			LM_ERR("cannot lookup the transaction\n");
			stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
			return CSCF_RETURN_BREAK;
		}
	}

	saved_t = shm_malloc(sizeof(saved_transaction_t));
	if (!saved_t) {
		LM_ERR("no more memory trying to save transaction state\n");
		return CSCF_RETURN_ERROR;

	}
	memset(saved_t, 0, sizeof(saved_transaction_t));
	saved_t->act = cfg_action;

	saved_t->realm.s = (char*) shm_malloc(realm.len + 1);
	if (!saved_t->realm.s) {
		LM_ERR("no more memory trying to save transaction state : callid\n");
		shm_free(saved_t);
		return CSCF_RETURN_ERROR;
	}
	memset(saved_t->realm.s, 0, realm.len + 1);
	memcpy(saved_t->realm.s, realm.s, realm.len);
	saved_t->realm.len = realm.len;

	saved_t->is_proxy_auth = is_proxy_auth;
	saved_t->is_resync = 1;

	LM_DBG("Suspending SIP TM transaction\n");
	if (tmb.t_suspend(msg, &saved_t->tindex, &saved_t->tlabel) < 0) {
		LM_ERR("failed to suspend the TM processing\n");
		free_saved_transaction_data(saved_t);

		stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
		return CSCF_RETURN_BREAK;
	}

	if (multimedia_auth_request(msg, public_identity, private_identity,
			av_request_at_sync, auth_scheme_types[algo_type], nonce, auts,
			scscf_name_str, saved_t) != 0) {
		LM_ERR("ERR:I_MAR: Error sending MAR or MAR time-out\n");
		tmb.t_cancel_suspend(saved_t->tindex, saved_t->tlabel);
		free_saved_transaction_data(saved_t);
		stateful_request_reply(msg, 480, MSG_480_DIAMETER_ERROR);
		return CSCF_RETURN_BREAK;
	}

	return CSCF_RETURN_BREAK;
}

int proxy_challenge(struct sip_msg* msg, char* _route, char* str1, char* str2) {
    return challenge(msg, str1, str2, 1, _route);
}

/**
 * Replies to a REGISTER and also adds the need headers
 * Path and Service-Route are added.
 * @param msg - the SIP message to operator on
 * @param code - Reason Code for the response
 * @param text - Reason Phrase for the response
 * @returns #CSCF_RETURN_TRUE on success or #CSCF_RETURN_FALSE if not added
 */
int stateful_request_reply(struct sip_msg *msg, int code, char *text) {
    unsigned int hash, label;
    struct hdr_field *h;
    str t = {0, 0};
    if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
        LM_ERR("Error parsing headers\n");
        return -1;
    }
    h = msg->headers;
    while (h) {
        if (h->name.len == 4 &&
                strncasecmp(h->name.s, "Path", 4) == 0) {
            t.s = h->name.s;
            t.len = h->len;
            ims_add_header_rpl(msg, &(t));
        }
        h = h->next;
    }

    /*if (code==200){
            ims_add_header_rpl(msg,&scscf_service_route);
    }*/ //TODO: need to get the service route from somewhere - registrar?


    if (tmb.t_get_trans_ident(msg, &hash, &label) < 0) {
        if (tmb.t_newtran(msg) < 0)
            LM_INFO("Failed creating SIP transaction\n");
    }
    return tmb.t_reply(msg, code, text);

}

/**
 * Replies to a REGISTER and also adds the need headers
 * Path and Service-Route are added.
 * @param msg - the SIP message to operator on
 * @param code - Reason Code for the response
 * @param text - Reason Phrase for the response
 * @returns #CSCF_RETURN_TRUE on success or #CSCF_RETURN_FALSE if not added
 */

int stateful_request_reply_async(struct cell* t_cell, struct sip_msg *msg, int code, char *text) {
    struct hdr_field *h;
    str t = {0, 0};
    if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
        LM_ERR("Error parsing headers\n");
        return -1;
    }
    h = msg->headers;
    while (h) {
        if (h->name.len == 4 &&
                strncasecmp(h->name.s, "Path", 4) == 0) {
            t.s = h->name.s;
            t.len = h->len;
            ims_add_header_rpl(msg, &(t));
        }
        h = h->next;
    }

    return tmb.t_reply_trans(t_cell, msg, code, text);

}

int authenticate(struct sip_msg* msg, char* _realm, char* str2, int is_proxy_auth) {
    int ret = -1; //CSCF_RETURN_FALSE;
    unsigned int aud_hash = 0;
    str realm;
    str private_identity, public_identity;
    str nonce, response16, nc, cnonce, qop_str = {0, 0}, auts = {0, 0}, body, *next_nonce = &empty_s;
    enum qop_type qop = QOP_UNSPEC;
    str uri = {0, 0};
    HASHHEX expected, ha1, hbody, rspauth;
    int expected_len = 32;
    int expires = 0;
    auth_vector *av = 0;
    uint32_t nc_parsed = 0; /* the numerical representation of nc */

    ret = AUTH_ERROR;

    if (get_str_fparam(&realm, msg, (fparam_t*) _realm) < 0) {
        LM_ERR("failed to get realm value\n");
        return AUTH_NO_CREDENTIALS;
    }

    if (realm.len == 0) {
        LM_ERR("invalid realm value - empty content\n");
        return AUTH_NO_CREDENTIALS;
    }

    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("This message is not a request\n");
        ret = AUTH_ERROR;
        goto end;
    }
    if (!is_proxy_auth) {
        LM_DBG("Checking if REGISTER is authorized for realm [%.*s]...\n", realm.len, realm.s);

        /* First check the parameters */
        if (msg->first_line.u.request.method.len != 8 ||
                memcmp(msg->first_line.u.request.method.s, "REGISTER", 8) != 0) {
            LM_ERR("This message is not a REGISTER request\n");
            ret = AUTH_ERROR;
            goto end;
        }
    }

    if (!realm.len) {
        LM_ERR("No realm found\n");
        return 0; //CSCF_RETURN_BREAK;
    }

    private_identity = cscf_get_private_identity(msg, realm);
    if (!private_identity.len) {
        LM_ERR("private identity missing\n");
        return AUTH_NO_CREDENTIALS;
    }

    public_identity = cscf_get_public_identity(msg);
    if (!public_identity.len) {
        LM_ERR("public identity missing\n");
        return AUTH_NO_CREDENTIALS;
    }

    if (!get_nonce_response(msg, realm, &nonce, &response16, &qop, &qop_str, &nc, &cnonce, &uri, is_proxy_auth) ||
            !nonce.len || !response16.len) {
        LM_DBG("Nonce or response missing: nonce len [%i], response16 len[%i]\n", nonce.len, response16.len);
        return AUTH_ERROR;
    }

    if (qop == QOP_AUTHINT) {
        body = ims_get_body(msg);
        calc_H(&body, hbody);
    }

    /* first, look for an already used vector (if nonce reuse is enabled) */
    if (max_nonce_reuse > 0) {
        LM_DBG("look for an already used vector for %.*s\n",
                private_identity.len, private_identity.s);
        av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_USED, &nonce, &aud_hash);
    }

    if (!av) {
        /* if none found, or nonce reuse is disabled, look for a fresh vector
         * We should also drop every other used vector at this point
         * (there souldn't be more than one) */
    	LM_DBG("Looking for auth vector based on IMPI: [%.*s] and IMPU: [%.*s]\n", private_identity.len, private_identity.s, public_identity.len, public_identity.s);
        auth_userdata *aud;
        auth_vector *av_it;
        aud = get_auth_userdata(private_identity, public_identity);
        if (aud) {
            av_it = aud->head;
            while (av_it) {
                if (av_it->status == AUTH_VECTOR_USED) {
                    LM_DBG("vector %p is marked for deletion\n", av_it);
                    av_it->status = AUTH_VECTOR_USELESS;
                }
                av_it = av_it->next;
            }
            auth_data_unlock(aud->hash);
        }

        LM_DBG("look for a fresh vector for %.*s\n",
                private_identity.len, private_identity.s);
        av = get_auth_vector(private_identity, public_identity, AUTH_VECTOR_SENT, &nonce, &aud_hash);
    }

    LM_INFO("uri=%.*s nonce=%.*s response=%.*s qop=%.*s nc=%.*s cnonce=%.*s hbody=%.*s\n",
            uri.len, uri.s,
            nonce.len, nonce.s,
            response16.len, response16.s,
            qop_str.len, qop_str.s,
            nc.len, nc.s,
            cnonce.len, cnonce.s,
            32, hbody);

    if (!av) {
        LM_DBG("no matching auth vector found - maybe timer expired\n");

        if (ignore_failed_auth) {
            LM_WARN("NB: Ignoring all failed auth - check your config if you don't expect this\n");
            ret = AUTH_OK;
        }

        goto end;
    }

    if (qop != QOP_UNSPEC) {
        /* if QOP is sent, nc must be specified */
        /* the expected nc is the last used one plus 1 */
        int p;
        for (p = 0; p < 8; ++p) { /* nc is 8LHEX (RFC 2617 §3.2.2) */
            nc_parsed = (nc_parsed << 4) | UNHEX((int) nc.s[p]);
        }
        LM_DBG("nc is %08x, expected: %08x\n",
                nc_parsed, av->use_nb + 1);
        if (nc_parsed <= av->use_nb) { /* nc is lower than expected */
            ret = AUTH_NONCE_REUSED;
            av->status = AUTH_VECTOR_USELESS; /* invalidate this vector if any mistake/error occurs */
            goto cleanup;
        } else if (nc_parsed > av->use_nb + 1) { /* nc is bigger than expected */
            ret = AUTH_ERROR;
            av->status = AUTH_VECTOR_USELESS;
            goto cleanup;
        }
    }

    switch (av->type) {
        case AUTH_AKAV1_MD5:
        case AUTH_AKAV2_MD5:
        case AUTH_MD5:
            calc_HA1(HA_MD5, &private_identity, &realm, &(av->authorization), &(av->authenticate), &cnonce, ha1);
            calc_response(ha1, &(av->authenticate),
                    &nc,
                    &cnonce,
                    &qop_str,
                    qop == QOP_AUTHINT,
                    &msg->first_line.u.request.method, &uri, hbody, expected);
            LM_INFO("UE said: %.*s and we expect %.*s ha1 %.*s (%.*s)\n",
                    response16.len, response16.s, 
                    /*av->authorization.len,av->authorization.s,*/32, expected,
                    32, ha1,
                    msg->first_line.u.request.method.len, msg->first_line.u.request.method.s);
            break;
        case AUTH_SIP_DIGEST:
        case AUTH_DIGEST:
            // memcpy of received HA1
            memcpy(ha1, av->authorization.s, HASHHEXLEN); 
            calc_response(ha1, &(av->authenticate),
                    &nc,
                    &cnonce,
                    &qop_str,
                    qop == QOP_AUTHINT,
                    &msg->first_line.u.request.method, &uri, hbody, expected);
            LM_INFO("UE said: %.*s and we expect %.*s ha1 %.*s (%.*s)\n",
                    response16.len, response16.s, 
                    32,expected, 
                    32,ha1, 
                    msg->first_line.u.request.method.len, msg->first_line.u.request.method.s);
            break;
        default:
            LM_ERR("algorithm %.*s is not handled.\n",
                    algorithm_types[av->type].len, algorithm_types[av->type].s);
            ret = AUTH_ERROR;
            goto cleanup; /* release aud before returning */
    }

    expires = cscf_get_max_expires(msg, 0);

    if (response16.len == expected_len && strncasecmp(response16.s, expected, response16.len) == 0) {
        if (max_nonce_reuse > 0 && av->status == AUTH_VECTOR_SENT) {
            /* first use of a reusable vector */
            /* set the vector's new timeout */
            LM_DBG("vector %p now expires in %d seconds\n", av, auth_used_vector_timeout);
            av->expires = get_ticks() + auth_used_vector_timeout;
        }
        av->use_nb++;
        LM_DBG("vector %p successfully used %d time(s)\n", av, av->use_nb);

        if (av->use_nb == max_nonce_reuse + 1) {
            LM_DBG("vector %p isn't fresh anymore, recycle it with a new nonce\n", av);

            int i;
            char y[NONCE_LEN];
            for (i = 0; i < NONCE_LEN; i++)
                y[i] = (unsigned char) ((int) (256.0 * rand() / (RAND_MAX + 1.0)));

            if (unlikely((av->authenticate.len < 2 * NONCE_LEN))) {
                if (av->authenticate.s) {
                    shm_free(av->authenticate.s);
                }
                av->authenticate.len = 2 * NONCE_LEN;
                av->authenticate.s = shm_malloc(av->authenticate.len);
            }

            if (!av->authenticate.s) {
                LM_ERR("new_auth_vector: failed allocating %d bytes!\n", av->authenticate.len);
                av->authenticate.len = 0;
                goto cleanup;
            }

            av->authenticate.len = bin_to_base16(y, NONCE_LEN, av->authenticate.s);

            next_nonce = &(av->authenticate);
            av->status = AUTH_VECTOR_USED;
            av->use_nb = 0;
            av->expires = get_ticks() + auth_used_vector_timeout; /* reset the timer */

        } else if (expires == 0) { /* de-registration */
            LM_DBG("de-registration, vector %p isn't needed anymore\n", av);
            av->status = AUTH_VECTOR_USELESS;
        } else {
            av->status = AUTH_VECTOR_USED;
            /* nextnonce is the current nonce */
            next_nonce = &nonce;
        }
        ret = AUTH_OK;

        if (add_authinfo_hdr && expires != 0 /* don't add auth. info if de-registation */) {
            /* calculate rspauth */
            calc_response(ha1, &nonce,
                    &nc,
                    &cnonce,
                    &qop_str,
                    qop == QOP_AUTHINT,
                    0, &uri, hbody, rspauth);

            add_authinfo_resp_hdr(msg, *next_nonce, qop_str, rspauth, cnonce, nc);
        }


    } else {
    	char authorise[200];
    	char authenticate_bin[200];
    	char authenticate_hex[200];
    	memset(authorise, 0, 200);
    	memset(authenticate_bin, 0, 200);
    	memset(authenticate_hex, 0, 200);

    	int authorise_len = bin_to_base16(av->authorization.s, av->authorization.len, authorise);
    	int authenticate_len = base64_to_bin(av->authenticate.s, av->authenticate.len, authenticate_bin);
    	int authenticate_hex_len = bin_to_base16(authenticate_bin, authenticate_len, authenticate_hex);
    	av->status = AUTH_VECTOR_USELESS; /* first mistake, you're out! (but maybe it's synchronization) */
        LM_DBG("UE said: %.*s, but we expect %.*s : authenticate(b64) is [%.*s], authenticate(hex) is [%.*s], authorise is [%d] [%.*s]\n",
                response16.len, response16.s,
                32, expected,
                av->authenticate.len, av->authenticate.s,
                authenticate_hex_len,authenticate_hex,
                authorise_len,
                authorise_len, authorise);
//        /* check for auts in authorization header - if it is then we need to resync */
		auts = ims_get_auts(msg, realm, is_proxy_auth);
		if (auts.len) {
			LM_DBG("IMS Auth Synchronization requested <%.*s>\n", auts.len, auts.s);
			ret = AUTH_RESYNC_REQUESTED;
			av->status = AUTH_VECTOR_SENT;
		} else {
			ret = AUTH_INVALID_PASSWORD;
		}
    }

    if (ignore_failed_auth) {
        LM_WARN("NB: Ignoring all failed auth - check your config if you don't expect this\n");
        ret = AUTH_OK;
    }

cleanup:
    auth_data_unlock(aud_hash);
end:
    return ret;
}

/*
 * Authenticate using WWW-Authorize header field
 */
int www_authenticate(struct sip_msg* msg, char* _realm, char* str2) {
    return authenticate(msg, _realm, str2, 0);
}

/*
 * Authenticate using WWW-Authorize header field
 */
int proxy_authenticate(struct sip_msg* msg, char* _realm, char* str2) {
    return authenticate(msg, _realm, str2, 1);
}

/**
 * @brief bind functions to IMS AUTH API structure
 */
int bind_ims_auth(ims_auth_api_t * api) {
    if (!api) {
        ERR("Invalid parameter value\n");
        return -1;
    }
    api->digest_authenticate = digest_authenticate;

    return 0;
}

/**
 * Retrieve an authentication vector.
 * \note returns with a lock, so unlock it when done
 * @param private_identity - the private identity
 * @param public_identity - the public identity
 * @param status - the status of the authentication vector
 * @param nonce - the nonce in the auth vector
 * @param hash - the hash to unlock when done
 * @returns the auth_vector* if found or NULL if not
 */
auth_vector * get_auth_vector(str private_identity, str public_identity, int status, str *nonce, unsigned int *hash) {
    auth_userdata *aud;
    auth_vector *av;
    aud = get_auth_userdata(private_identity, public_identity);
    if (!aud) {
        LM_ERR("no auth userdata\n");
        goto error;
    }

    av = aud->head;
    while (av) {
        LM_DBG("looping through AV status is %d and were looking for %d\n", av->status, status);
        if (av->status == status && (nonce == 0 || (nonce->len == av->authenticate.len && memcmp(nonce->s, av->authenticate.s, nonce->len) == 0))) {
            LM_DBG("Found result\n");
            *hash = aud->hash;
            return av;
        }
        av = av->next;
    }

error:
    if (aud) auth_data_unlock(aud->hash);
    return 0;
}

/**
 * Locks the required slot of the auth_data.
 * @param hash - the index of the slot
 */
inline void auth_data_lock(unsigned int hash) {
    lock_get(auth_data[(hash)].lock);
}

/**
 * UnLocks the required slot of the auth_data
 * @param hash - the index of the slot
 */
inline void auth_data_unlock(unsigned int hash) {
    lock_release(auth_data[(hash)].lock);
}

/**
 * Initializes the Authorization Data structures.
 * @param size - size of the hash table
 * @returns 1 on success or 0 on error
 */
int auth_data_init(int size) {
    int i;
    auth_data = shm_malloc(sizeof (auth_hash_slot_t) * size);
    if (!auth_data) {
        LM_ERR("error allocating mem\n");
        return 0;
    }
    memset(auth_data, 0, sizeof (auth_hash_slot_t) * size);
    for (i = 0; i < size; i++) {
        auth_data[i].lock = lock_alloc();
        lock_init(auth_data[i].lock);
    }
    act_auth_data_hash_size = size;
    return 1;
}

/**
 * Destroy the Authorization Data structures */
void auth_data_destroy() {
    int i;
    auth_userdata *aud, *next;
    for (i = 0; i < act_auth_data_hash_size; i++) {
        auth_data_lock(i);
        lock_destroy(auth_data[i].lock);
        lock_dealloc(auth_data[i].lock);
        aud = auth_data[i].head;
        while (aud) {
            next = aud->next;
            free_auth_userdata(aud);
            aud = next;
        }
    }
    if (auth_data) shm_free(auth_data);
}

/**
 * Create new authorization vector
 * @param item_number - number to index it in the vectors list
 * @param auth_scheme - Diameter Authorization Scheme
 * @param authenticate - the challenge
 * @param authorization - the expected response
 * @param ck - the cypher key
 * @param ik - the integrity key
 * @returns the new auth_vector* or NULL on error
 */
auth_vector * new_auth_vector(int item_number, str auth_scheme, str authenticate,
        str authorization, str ck, str ik) {
    auth_vector *x = 0;
    x = shm_malloc(sizeof (auth_vector));
    if (!x) {
        LM_ERR("error allocating mem\n");
        goto done;
    }
    memset(x, 0, sizeof (auth_vector));
    x->item_number = item_number;
    x->type = get_auth_scheme_type(auth_scheme);
    switch (x->type) {
        case AUTH_AKAV1_MD5:
        case AUTH_AKAV2_MD5:
            /* AKA */
            x->authenticate.len = authenticate.len * 4 / 3 + 4;
            x->authenticate.s = shm_malloc(x->authenticate.len);
            if (!x->authenticate.s) {
                LM_ERR("error allocating mem\n");
                goto done;
            }
            x->authenticate.len = bin_to_base64(authenticate.s, authenticate.len,
                    x->authenticate.s);

            x->authorization.len = authorization.len;
            x->authorization.s = shm_malloc(x->authorization.len);
            if (!x->authorization.s) {
                LM_ERR("error allocating mem\n");
                goto done;
            }
            memcpy(x->authorization.s, authorization.s, authorization.len);
            x->ck.len = ck.len;
            x->ck.s = shm_malloc(ck.len);
            if (!x->ck.s) {
                LM_ERR("error allocating mem\n");
                goto done;
            }
            memcpy(x->ck.s, ck.s, ck.len);

            x->ik.len = ik.len;
            x->ik.s = shm_malloc(ik.len);
            if (!x->ik.s) {
                LM_ERR("error allocating mem\n");
                goto done;
            }
            memcpy(x->ik.s, ik.s, ik.len);
            break;

        case AUTH_MD5:
            /* MD5 */
            x->authenticate.len = authenticate.len * 2;
            x->authenticate.s = shm_malloc(x->authenticate.len);
            if (!x->authenticate.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                goto done;
            }
            x->authenticate.len = bin_to_base16(authenticate.s, authenticate.len,
                    x->authenticate.s);

            x->authorization.len = authorization.len;
            x->authorization.s = shm_malloc(x->authorization.len);
            if (!x->authorization.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                goto done;
            }
            memcpy(x->authorization.s, authorization.s, authorization.len);
            x->authorization.len = authorization.len;
            break;
        case AUTH_DIGEST:
        case AUTH_SIP_DIGEST:
        {
            int i;
            char y[NONCE_LEN];
            for (i = 0; i < NONCE_LEN; i++)
                y[i] = (unsigned char) ((int) (256.0 * rand() / (RAND_MAX + 1.0)));
            x->authenticate.len = 2 * NONCE_LEN;
            x->authenticate.s = shm_malloc(x->authenticate.len);
            if (!x->authenticate.s) {
                LM_ERR("new_auth_vector: failed allocating %d bytes!\n", x->authenticate.len);
                x->authenticate.len = 0;
                goto done;
            }
            x->authenticate.len = bin_to_base16(y, NONCE_LEN, x->authenticate.s);
        }

            x->authorization.len = authorization.len;
            x->authorization.s = shm_malloc(x->authorization.len);
            if (!x->authorization.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                x->authorization.len = 0;
                goto done;
            }
            memcpy(x->authorization.s, authorization.s, authorization.len);
            x->authorization.len = authorization.len;

            break;
        case AUTH_HTTP_DIGEST_MD5:
            x->authenticate.len = authenticate.len;
            x->authenticate.s = shm_malloc(x->authenticate.len);
            if (!x->authenticate.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                x->authenticate.len = 0;
                goto done;
            }
            memcpy(x->authenticate.s, authenticate.s, authenticate.len);

            x->authorization.len = authorization.len;
            x->authorization.s = shm_malloc(x->authorization.len);
            if (!x->authorization.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                x->authorization.len = 0;
                goto done;
            }
            memcpy(x->authorization.s, authorization.s, authorization.len);
            break;
        case AUTH_EARLY_IMS:
            /* early IMS */
            x->authenticate.len = 0;
            x->authenticate.s = 0;
            x->authorization.len = authorization.len;
            x->authorization.s = shm_malloc(x->authorization.len);
            if (!x->authorization.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                goto done;
            }
            memcpy(x->authorization.s, authorization.s, authorization.len);
            x->authorization.len = authorization.len;
            break;
        case AUTH_NASS_BUNDLED:
            /* NASS-Bundled */
            x->authenticate.len = 0;
            x->authenticate.s = 0;
            x->authorization.len = authorization.len;
            x->authorization.s = shm_malloc(x->authorization.len);
            if (!x->authorization.s) {
                LM_ERR("new_auth_vector: error allocating mem\n");
                goto done;
            }
            memcpy(x->authorization.s, authorization.s, authorization.len);
            x->authorization.len = authorization.len;
            break;

        default:
            /* all else */
            x->authenticate.len = 0;
            x->authenticate.s = 0;

    }

    x->use_nb = 0;

    x->next = 0;
    x->prev = 0;
    x->status = AUTH_VECTOR_UNUSED;
    x->expires = 0;

    LM_DBG("new auth-vector with ck [%.*s] with status %d\n", x->ck.len, x->ck.s, x->status);

done:
    return x;
}

/**
 * Frees the memory taken by a authentication vector
 * @param av - the vector to be freed
 */
void free_auth_vector(auth_vector * av) {
    if (av) {
        if (av->authenticate.s) shm_free(av->authenticate.s);
        if (av->authorization.s) shm_free(av->authorization.s);
        if (av->ck.s) shm_free(av->ck.s);
        if (av->ik.s) shm_free(av->ik.s);
        shm_free(av);
    }
}

/**
 * Creates a new Authorization Userdata structure.
 * @param private_identity - the private identity to attach to
 * @param public_identity - the public identity to attach to
 * @returns the new auth_userdata* on success or NULL on error
 */
auth_userdata * new_auth_userdata(str private_identity, str public_identity) {
    auth_userdata *x = 0;

    x = shm_malloc(sizeof (auth_userdata));
    if (!x) {
        LM_ERR("error allocating mem\n");
        goto done;
    }

    x->private_identity.len = private_identity.len;
    x->private_identity.s = shm_malloc(private_identity.len);
    if (!x) {
        LM_ERR("error allocating mem\n");
        goto done;
    }
    memcpy(x->private_identity.s, private_identity.s, private_identity.len);

    x->public_identity.len = public_identity.len;
    x->public_identity.s = shm_malloc(public_identity.len);
    if (!x) {
        LM_ERR("error allocating mem\n");
        goto done;
    }
    memcpy(x->public_identity.s, public_identity.s, public_identity.len);

    x->head = 0;
    x->tail = 0;

    x->next = 0;
    x->prev = 0;

done:
    return x;
}

/**
 * Deallocates the auth_userdata.
 * @param aud - the auth_userdata to be deallocated
 */
void free_auth_userdata(auth_userdata * aud) {
    auth_vector *av, *next;
    if (aud) {
        if (aud->private_identity.s) shm_free(aud->private_identity.s);
        if (aud->public_identity.s) shm_free(aud->public_identity.s);
        av = aud->head;
        while (av) {
            next = av->next;
            free_auth_vector(av);
            av = next;
        }
        shm_free(aud);
    }
}

/**
 * Computes a hash based on the private and public identities
 * @param private_identity - the private identity
 * @param public_identity - the public identity
 * @returns the hash % Auth_data->size
 */
inline unsigned int get_hash_auth(str private_identity, str public_identity) {
if (av_check_only_impu)
	return core_hash(&public_identity, 0, act_auth_data_hash_size);
else
	return core_hash(&public_identity, 0, act_auth_data_hash_size);
/*


#define h_inc h+=v^(v>>3)
    char* p;
    register unsigned v;
    register unsigned h;

    h = 0;
    for (p = private_identity.s; p <= (private_identity.s + private_identity.len - 4); p += 4) {
        v = (*p << 24)+(p[1] << 16)+(p[2] << 8) + p[3];
        h_inc;
    }
    v = 0;
    for (; p < (private_identity.s + private_identity.len); p++) {
        v <<= 8;
        v += *p;
    }
    h_inc;
    for (p = public_identity.s; p <= (public_identity.s + public_identity.len - 4); p += 4) {
        v = (*p << 24)+(p[1] << 16)+(p[2] << 8) + p[3];
        h_inc;
    }
    v = 0;
    for (; p < (public_identity.s + public_identity.len); p++) {
        v <<= 8;
        v += *p;
    }

    h = ((h)+(h >> 11))+((h >> 13)+(h >> 23));
    return (h) % auth_data_hash_size;
#undef h_inc
*/
}

/**
 * Retrieve the auth_userdata for a user.
 * \note you will return with lock on the hash slot, so release it!
 * @param private_identity - the private identity
 * @param public_identity - the public identity
 * @returns the auth_userdata* found or newly created on success, NULL on error
 */
auth_userdata * get_auth_userdata(str private_identity, str public_identity) {

    unsigned int hash = 0;
    auth_userdata *aud = 0;

    hash = get_hash_auth(private_identity, public_identity);
    auth_data_lock(hash);
    aud = auth_data[hash].head;
    if (av_check_only_impu)
      LM_DBG("Searching auth_userdata for IMPU %.*s (Hash %d)\n", public_identity.len, public_identity.s, hash);
    else
      LM_DBG("Searching auth_userdata for IMPU %.*s / IMPI %.*s (Hash %d)\n", public_identity.len, public_identity.s,
        private_identity.len, private_identity.s, hash);

    while (aud) {
	if (av_check_only_impu) {
		if (aud->public_identity.len == public_identity.len &&
		        memcmp(aud->public_identity.s, public_identity.s, public_identity.len) == 0) {
                    LM_DBG("Found auth_userdata\n");
		    return aud;
		}
	} else {
		if (aud->private_identity.len == private_identity.len &&
		        aud->public_identity.len == public_identity.len &&
		        memcmp(aud->private_identity.s, private_identity.s, private_identity.len) == 0 &&
		        memcmp(aud->public_identity.s, public_identity.s, public_identity.len) == 0) {
                    LM_DBG("Found auth_userdata\n");
		    return aud;
		}
	}

        aud = aud->next;
    }
    /* if we get here, there is no auth_userdata for this user */
    aud = new_auth_userdata(private_identity, public_identity);
    if (!aud) {
        auth_data_unlock(hash);
        return 0;
    }

    aud->prev = auth_data[hash].tail;
    aud->next = 0;
    aud->hash = hash;

    if (!auth_data[hash].head) auth_data[hash].head = aud;
    if (auth_data[hash].tail) auth_data[hash].tail->next = aud;
    auth_data[hash].tail = aud;

    return aud;
}

/**
 * Sends a Multimedia-Authentication-Response to retrieve some authentication vectors and maybe synchronize.
 * Must respond with a SIP reply every time it returns 0
 * @param msg - the SIP REGISTER message
 * @param public_identity - the public identity
 * @param private_identity - the private identity
 * @param count - how many vectors to request
 * @param algorithm - which algorithm to request
 * @param nonce - the challenge that will be sent
 * @param auts - the AKA synchronization or empty string if not a synchronization
 * @param server_name - the S-CSCF name to be saved on the HSS
 * @returns 1 on success, 0 on failure
 */
int multimedia_auth_request(struct sip_msg *msg, str public_identity, str private_identity,
        int count, str auth_scheme, str nonce, str auts, str servername, saved_transaction_t* transaction_data) {


    str authorization = {0, 0};
    int result = -1;

    int is_sync = 0;
    if (auts.len) {
        authorization.s = pkg_malloc(nonce.len * 3 / 4 + auts.len * 3 / 4 + 8);
        if (!authorization.s)  {
        	LM_ERR("no more pkg mem\n");
        	return result;
        }
        authorization.len = base64_to_bin(nonce.s, nonce.len, authorization.s);
        authorization.len = RAND_LEN;
        authorization.len += base64_to_bin(auts.s, auts.len, authorization.s + authorization.len);
        is_sync = 1;
    }

    if (is_sync) {
    	drop_auth_userdata(private_identity, public_identity);
    }


    LM_DBG("Sending MAR\n");
    result = cxdx_send_mar(msg, public_identity, private_identity, count, auth_scheme, authorization, servername, transaction_data);
    if (authorization.s) pkg_free(authorization.s);

    return result;
}

/**
 * Adds the WWW-Authenticate header for challenge, based on the authentication vector.
 * @param msg - SIP message to add the header to
 * @param realm - the realm
 * @param av - the authentication vector
 * @returns 1 on success, 0 on error
 */
int pack_challenge(struct sip_msg *msg, str realm, auth_vector *av, int is_proxy_auth) {
    str x = {0, 0};
    char ck[32], ik[32];
    int ck_len, ik_len;
    str *auth_prefix = is_proxy_auth ? &S_Proxy : &S_WWW;
    switch (av->type) {
        case AUTH_AKAV1_MD5:
        case AUTH_AKAV2_MD5:
            /* AKA */
            ck_len = bin_to_base16(av->ck.s, 16, ck);
            ik_len = bin_to_base16(av->ik.s, 16, ik);
            x.len = S_Authorization_AKA.len + auth_prefix->len + realm.len + av->authenticate.len
                    + algorithm_types[av->type].len + ck_len + ik_len
                    + registration_qop_str.len;
            x.s = pkg_malloc(x.len);
            if (!x.s) {
                LM_ERR("Error allocating %d bytes\n",
                        x.len);
                goto error;
            }
            sprintf(x.s, S_Authorization_AKA.s, auth_prefix->len, auth_prefix->s, realm.len, realm.s,
                    av->authenticate.len, av->authenticate.s,
                    algorithm_types[av->type].len, algorithm_types[av->type].s,
                    ck_len, ck, ik_len, ik, registration_qop_str.len,
                    registration_qop_str.s);
            x.len = strlen(x.s);
            break;
        case AUTH_HTTP_DIGEST_MD5:
            /* ETSI HTTP_DIGEST MD5 */
            /* this one continues into the next one */
        case AUTH_DIGEST:
            /* Cable-Labs MD5 */
            /* this one continues into the next one */
        case AUTH_SIP_DIGEST:
            /* 3GPP MD5 */
            /* this one continues into the next one */
        case AUTH_MD5:
            /* FOKUS MD5 */
            x.len = S_Authorization_MD5.len + auth_prefix->len + realm.len + av->authenticate.len
                    + algorithm_types[av->type].len + registration_qop_str.len;
            x.s = pkg_malloc(x.len);
            if (!x.s) {
                LM_ERR("pack_challenge: Error allocating %d bytes\n", x.len);
                goto error;
            }
            sprintf(x.s, S_Authorization_MD5.s, auth_prefix->len, auth_prefix->s, realm.len, realm.s,
                    av->authenticate.len, av->authenticate.s,
                    algorithm_types[AUTH_MD5].len, algorithm_types[AUTH_MD5].s,
                    registration_qop_str.len, registration_qop_str.s);
            x.len = strlen(x.s);
            break;

        default:
            LM_CRIT("not implemented for algorithm %.*s\n",
                    algorithm_types[av->type].len, algorithm_types[av->type].s);
            goto error;
    }

    if (ims_add_header_rpl(msg, &x)) {
        pkg_free(x.s);
        return 1;
    }

error:
    if (x.s)
        pkg_free(x.s);

    return 0;
}

/**
 * Adds the Authentication-Info header for, based on the credentials sent by a successful REGISTER.
 * @param msg - SIP message to add the header to
 * @returns 1 on success, 0 on error
 */
int add_authinfo_resp_hdr(struct sip_msg *msg, str nextnonce, str qop, HASHHEX rspauth, str cnonce, str nc) {

    str authinfo_hdr;
    static const char authinfo_fmt[] = "Authentication-Info: "
            "nextnonce=\"%.*s\","
            "qop=%.*s,"
            "rspauth=\"%.*s\","
            "cnonce=\"%.*s\","
            "nc=%.*s\r\n";

    authinfo_hdr.len = sizeof (authinfo_fmt) + nextnonce.len + qop.len + HASHHEXLEN + cnonce.len + nc.len - 20 /* format string parameters */ - 1 /* trailing \0 */;
    authinfo_hdr.s = pkg_malloc(authinfo_hdr.len + 1);

    if (!authinfo_hdr.s) {
        LM_ERR("add_authinfo_resp_hdr: Error allocating %d bytes\n", authinfo_hdr.len);
        goto error;
    }
    snprintf(authinfo_hdr.s, authinfo_hdr.len + 1, authinfo_fmt,
            nextnonce.len, nextnonce.s,
            qop.len, qop.s,
            HASHHEXLEN, rspauth,
            cnonce.len, cnonce.s,
            nc.len, nc.s);
    LM_DBG("authinfo hdr built: %.*s", authinfo_hdr.len, authinfo_hdr.s);
    if (ims_add_header_rpl(msg, &authinfo_hdr)) {
        LM_DBG("authinfo hdr added");
        pkg_free(authinfo_hdr.s);
        return 1;
    }
error:
    if (authinfo_hdr.s) pkg_free(authinfo_hdr.s);

    return 0;
}

/**
 * Add an authentication vector to the authentication userdata storage.
 * @param private_identity - the private identity
 * @param public_identity - the public identity
 * @param av - the authentication vector
 * @returns 1 on success or 0 on error
 */
int add_auth_vector(str private_identity, str public_identity, auth_vector * av) {
    auth_userdata *aud;
    aud = get_auth_userdata(private_identity, public_identity);
    if (!aud) goto error;

     LM_DBG("Adding auth_vector (status %d) for IMPU %.*s / IMPI %.*s (Hash %d)\n", av->status,
	public_identity.len, public_identity.s,
        private_identity.len, private_identity.s, aud->hash);


    av->prev = aud->tail;
    av->next = 0;

    if (!aud->head) aud->head = av;
    if (aud->tail) aud->tail->next = av;
    aud->tail = av;

    auth_data_unlock(aud->hash);
    return 1;
error:

    return 0;
}

/**
 * Declares all auth vectors as useless when we do a synchronization
 * @param private_identity - the private identity
 * @param public_identity - the public identity
 * @returns 1 on sucess, 0 on error
 */
int drop_auth_userdata(str private_identity, str public_identity) {
    auth_userdata *aud;
    auth_vector *av;
    aud = get_auth_userdata(private_identity, public_identity);
    if (!aud) goto error;

    av = aud->head;
    while (av) {
    	LM_DBG("dropping auth vector that was in status %d\n", av->status);
        av->status = AUTH_VECTOR_USELESS;
        av = av->next;
    }
    auth_data_unlock(aud->hash);
    return 1;
error:
	LM_DBG("no authdata to drop any auth vectors\n");
    if (aud) auth_data_unlock(aud->hash);
    return 0;
}
