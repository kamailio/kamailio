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

#include "../../str.h"
#include "../../socket_info.h"
#include "../../parser/parse_allow.h"
#include "../../parser/parse_methods.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../mod_fix.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../lib/kcore/statistics.h"

#ifdef USE_TCP
#include "../../tcp_server.h"
#endif

#include "../ims_usrloc_scscf/usrloc.h"
#include "common.h"
#include "sip_msg.h"
#include "rerrno.h"
#include "reply.h"
#include "reg_mod.h"
#include "regtime.h"
#include "path.h"
#include "save.h"
#include "config.h"
#include "server_assignment.h"
#include "userdata_parser.h"
#include "../../lib/ims/ims_getters.h"
#include "registrar_notify.h"

#include "cxdx_sar.h"

extern struct tm_binds tmb;
extern int store_data_on_dereg; /**< should we store SAR user data on de-registration  */

extern int ue_unsubscribe_on_dereg;
extern int user_data_always;


/* \brief
 * Return randomized expires between expires-range% and expires.
 * RFC allows only value less or equal to the one provided by UAC.
 */
static inline int randomize_expires( int expires, int range )
{
	/* if no range is given just return expires */
	if(range == 0) return expires;

	int range_min = expires - (float)range/100 * expires;

	return range_min + (float)(rand()%100)/100 * ( expires - range_min );
}


/*! \brief
 * Calculate absolute expires value per contact as follows:
 * 1) If the contact has expires value, use the value. If it
 *    is not zero, add actual time to it
 * 2) If the contact has no expires parameter, use expires
 *    header field in the same way
 * 3) If the message contained no expires header field, use
 *    the default value
 */
static inline int calc_contact_expires(contact_t *c, unsigned int expires_hdr, int sos_reg) {
    unsigned int r;

    if (expires_hdr >= 0)
        r = expires_hdr;
    else {
        r = (sos_reg > 0) ? default_registrar_cfg.em_default_expires : default_registrar_cfg.default_expires;
        goto end;
    }
    if (c && c->expires)
        str2int(&(c->expires->body), (unsigned int*) &r);
    if (r > 0) {
        if (!sos_reg && r < default_registrar_cfg.min_expires) {
            r = default_registrar_cfg.min_expires;
            goto end;
        }
        if (sos_reg && r < default_registrar_cfg.em_min_expires) {
            r = default_registrar_cfg.em_min_expires;
            goto end;
        }
    }
    if (!sos_reg && r > default_registrar_cfg.max_expires) {
        r = default_registrar_cfg.max_expires;
        goto end;
    }
    if (sos_reg && r > default_registrar_cfg.em_max_expires)
        r = default_registrar_cfg.em_min_expires;

end:
	    
    r = randomize_expires(r, default_registrar_cfg.default_expires_range);
	    
    LM_DBG("Calculated expires for contact is %d\n", r);
    return time(NULL) + r;
}

/*! \brief
 * Process request that contained a star, in that case, 
 * we will remove all bindings with the given impu
 * from the usrloc and return 200 OK response
 */
static inline int star(udomain_t* _d, str* _a) {
    impurecord_t* r;

    ul.lock_udomain(_d, _a);

    if (ul.delete_impurecord(_d, _a, r) < 0) {
        LM_ERR("failed to remove record from usrloc\n");

        /* Delete failed, try to get corresponding
         * record structure and send back all existing
         * contacts
         */
        rerrno = R_UL_DEL_R;

        if (!ul.get_impurecord(_d, _a, &r)) {
            contact_for_header_t** contact_header = 0;
            build_contact(r, contact_header);
            free_contact_buf(*contact_header);
        }
        ul.unlock_udomain(_d, _a);
        return -1;
    }
    ul.unlock_udomain(_d, _a);
    return 0;
}

/*! \brief
 */
static struct socket_info *get_sock_hdr(struct sip_msg *msg) {
    struct socket_info *sock;
    struct hdr_field *hf;
    str socks;
    str hosts;
    int port;
    int proto;
    char c;

    if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
        LM_ERR("failed to parse message\n");
        return 0;
    }

    for (hf = msg->headers; hf; hf = hf->next) {
        if (cmp_hdrname_str(&hf->name, &sock_hdr_name) == 0)
            break;
    }

    /* hdr found? */
    if (hf == 0)
        return 0;

    trim_len(socks.len, socks.s, hf->body);
    if (socks.len == 0)
        return 0;

    /*FIXME: This is a hack */
    c = socks.s[socks.len];
    socks.s[socks.len] = '\0';
    if (parse_phostport(socks.s, &hosts.s, &hosts.len, &port, &proto) != 0) {
        socks.s[socks.len] = c;
        LM_ERR("bad socket <%.*s> in \n",
                socks.len, socks.s);
        return 0;
    }
    socks.s[socks.len] = c;
    sock = grep_sock_info(&hosts, (unsigned short) port,
            (unsigned short) proto);
    if (sock == 0) {
        LM_ERR("non-local socket <%.*s>\n", socks.len, socks.s);
        return 0;
    }

    LM_DBG("%d:<%.*s>:%d -> p=%p\n", proto, socks.len, socks.s, port_no, sock);

    return sock;
}

/*! \brief
 * Fills the common part (for all contacts) of the info structure
 */
static inline ucontact_info_t* pack_ci(struct sip_msg* _m, contact_t* _c, unsigned int _e, unsigned int _f) {
    static ucontact_info_t ci;
    static str no_ua = str_init("n/a");
    static str callid;
    static str path_received = {0, 0};
    static str path;
    static str received = {0, 0};
    static int received_found;
    static unsigned int allowed, allow_parsed;
    static struct sip_msg *m = 0;
    int_str val;

    if (_m != 0) {
        memset(&ci, 0, sizeof (ucontact_info_t));

        /* Get callid of the message */
        callid = _m->callid->body;
        trim_trailing(&callid);
        if (callid.len > CALLID_MAX_SIZE) {
            rerrno = R_CALLID_LEN;
            LM_ERR("callid too long\n");
            goto error;
        }
        ci.callid = &callid;

        /* Get CSeq number of the message */
        if (str2int(&get_cseq(_m)->number, (unsigned int*) &ci.cseq) < 0) {
            rerrno = R_INV_CSEQ;
            LM_ERR("failed to convert cseq number\n");
            goto error;
        }

        /* set received socket */
        if (_m->flags & sock_flag) {
            ci.sock = get_sock_hdr(_m);
            if (ci.sock == 0)
                ci.sock = _m->rcv.bind_address;
        } else {
            ci.sock = _m->rcv.bind_address;
        }

        /* additional info from message */
        if (parse_headers(_m, HDR_USERAGENT_F, 0) != -1 && _m->user_agent
                && _m->user_agent->body.len > 0 && _m->user_agent->body.len < MAX_UA_SIZE) {
            ci.user_agent = &_m->user_agent->body;
        } else {
            ci.user_agent = &no_ua;
        }

        /* extract Path headers */
        if (path_enabled) {
            if (build_path_vector(_m, &path, &path_received) < 0) {
                rerrno = R_PARSE_PATH;
                goto error;
            }
            if (path.len && path.s) {
                ci.path = &path;
                if (path_mode != PATH_MODE_OFF) {
                    /* save in msg too for reply */
                    if (set_path_vector(_m, &path) < 0) {
                        rerrno = R_PARSE_PATH;
                        goto error;
                    }
                }
            }
        }
	
	if(_c->params) {
	    ci.params = _c->params;
	}

        /* set flags */
        ci.flags = _f;
        getbflagsval(0, &ci.cflags);

        /* get received */
        if (path_received.len && path_received.s) {
            ci.cflags |= ul.nat_flag;
            ci.received = path_received;
        }

        allow_parsed = 0; /* not parsed yet */
        received_found = 0; /* not found yet */
        m = _m; /* remember the message */
    }

    if (_c != 0) {
        /* Calculate q value of the contact */
        if (calc_contact_q(_c->q, &ci.q) < 0) {
            rerrno = R_INV_Q;
            LM_ERR("failed to calculate q\n");
            goto error;
        }

        /* set expire time */
        ci.expires = _e;

        /* Get methods of contact */
        if (_c->methods) {
            if (parse_methods(&(_c->methods->body), &ci.methods) < 0) {
                rerrno = R_PARSE;
                LM_ERR("failed to parse contact methods\n");
                goto error;
            }
        } else {
            /* check on Allow hdr */
            if (allow_parsed == 0) {
                if (m && parse_allow(m) != -1) {
                    allowed = get_allow_methods(m);
                } else {
                    allowed = ALL_METHODS;
                }
                allow_parsed = 1;
            }
            ci.methods = allowed;
        }

        /* get received */
        if (ci.received.len == 0) {
            if (_c->received) {
                ci.received = _c->received->body;
            } else {
                if (received_found == 0) {
                    memset(&val, 0, sizeof (int_str));
                    if (rcv_avp_name.n != 0
                            && search_first_avp(rcv_avp_type, rcv_avp_name,
                            &val, 0) && val.s.len > 0) {
                        if (val.s.len > RECEIVED_MAX_SIZE) {
                            rerrno = R_CONTACT_LEN;
                            LM_ERR("received too long\n");
                            goto error;
                        }
                        received = val.s;
                    } else {
                        received.s = 0;
                        received.len = 0;
                    }
                    received_found = 1;
                }
                ci.received = received;
            }
        }

    }

    return &ci;
error:
    return 0;
}

/**
 * Deallocates memory used by a subscription.
 * \note Must be called with the lock got to avoid races
 * @param s - the ims_subscription to free
 */
void free_ims_subscription_data(ims_subscription *s) {
    int i, j, k;
    if (!s)
        return;
    /*	lock_get(s->lock); - must be called with the lock got */
    for (i = 0; i < s->service_profiles_cnt; i++) {
        for (j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
            if (s->service_profiles[i].public_identities[j].public_identity.s)
                shm_free(
                    s->service_profiles[i].public_identities[j].public_identity.s);
            if (s->service_profiles[i].public_identities[j].wildcarded_psi.s)
                shm_free(
                    s->service_profiles[i].public_identities[j].wildcarded_psi.s);

        }
        if (s->service_profiles[i].public_identities)
            shm_free(s->service_profiles[i].public_identities);

        for (j = 0; j < s->service_profiles[i].filter_criteria_cnt; j++) {
            if (s->service_profiles[i].filter_criteria[j].trigger_point) {
                for (k = 0;
                        k
                        < s->service_profiles[i].filter_criteria[j].trigger_point->spt_cnt;
                        k++) {
                    switch (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].type) {
                        case IFC_REQUEST_URI:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].request_uri.s)
                                shm_free(
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].request_uri.s);
                            break;
                        case IFC_METHOD:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].method.s)
                                shm_free(
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].method.s);
                            break;
                        case IFC_SIP_HEADER:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.header.s)
                                shm_free(
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.header.s);
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.content.s)
                                shm_free(
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].sip_header.content.s);
                            break;
                        case IFC_SESSION_CASE:
                            break;
                        case IFC_SESSION_DESC:
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.line.s)
                                shm_free(
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.line.s);
                            if (s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.content.s)
                                shm_free(
                                    s->service_profiles[i].filter_criteria[j].trigger_point->spt[k].session_desc.content.s);
                            break;

                    }
                }
                if (s->service_profiles[i].filter_criteria[j].trigger_point->spt)
                    shm_free(
                        s->service_profiles[i].filter_criteria[j].trigger_point->spt);
                shm_free(
                        s->service_profiles[i].filter_criteria[j].trigger_point);
            }
            if (s->service_profiles[i].filter_criteria[j].application_server.server_name.s)
                shm_free(
                    s->service_profiles[i].filter_criteria[j].application_server.server_name.s);
            if (s->service_profiles[i].filter_criteria[j].application_server.service_info.s)
                shm_free(
                    s->service_profiles[i].filter_criteria[j].application_server.service_info.s);
            if (s->service_profiles[i].filter_criteria[j].profile_part_indicator)
                shm_free(
                    s->service_profiles[i].filter_criteria[j].profile_part_indicator);
        }
        if (s->service_profiles[i].filter_criteria)
            shm_free(s->service_profiles[i].filter_criteria);

        if (s->service_profiles[i].cn_service_auth)
            shm_free(s->service_profiles[i].cn_service_auth);

        if (s->service_profiles[i].shared_ifc_set)
            shm_free(s->service_profiles[i].shared_ifc_set);
    }
    if (s->service_profiles)
        shm_free(s->service_profiles);
    if (s->private_identity.s)
        shm_free(s->private_identity.s);
    lock_release(s->lock);
    lock_destroy(s->lock);
    lock_dealloc(s->lock);
    shm_free(s);

}

/** Check if an impu record exists.
 * 1. must be in registered state (impurecord)
 * 2. must have at least one valid contact
 */
static inline int is_impu_registered(udomain_t* _d, str* public_identity) {
    int res, ret = 1;
    impurecord_t* impu;

    ul.lock_udomain(_d, public_identity);
    res = ul.get_impurecord(_d, public_identity, &impu);
    if (res != 0) {
        ul.unlock_udomain(_d, public_identity);
        return 0;
    } else {
        //check reg status
        if (impu->reg_state != IMPU_REGISTERED) {
            LM_DBG("IMPU <%.*s> is not currently registered\n", public_identity->len, public_identity->s);
            ret = 0;
        }
	
	//check valid contacts
        if((impu->num_contacts <=0) || (impu->newcontacts[0]==0))
	{
	    LM_DBG("IMPU <%.*s> has no valid contacts\n", public_identity->len, public_identity->s);
            ret = 0;
	}
    }
    ul.unlock_udomain(_d, public_identity);
    return ret;

}

struct sip_msg* get_request_from_reply(struct sip_msg *reply) {
    struct cell *t;
    t = tmb.t_gett();
    if (!t || t == (void*) - 1) {
        LM_ERR("get_request_from_reply: Reply without transaction\n");
        return 0;
    }
    if (t)
        return t->uas.request;
    else
        return 0;
}

/**
 * update the contacts for a public identity. Make sure you have the lock on the domain before calling this
 * returns 0 on success, -1 on failure
 */
static inline int update_contacts_helper(struct sip_msg* msg, impurecord_t* impu_rec, int assignment_type, unsigned int expires_hdr) {
    struct hdr_field* h;
    contact_t* chi; //contact header information
    ucontact_info_t* ci; //ucontact info
    qvalue_t qvalue;
    int sos = 0, expires;
    struct ucontact* ucontact;

    LM_DBG("updating the contacts for IMPU <%.*s>\n", impu_rec->public_identity.len, impu_rec->public_identity.s);

    switch (assignment_type) {

        case AVP_IMS_SAR_USER_DEREGISTRATION:
            LM_DBG("update_contacts_helper: doing de-reg\n");
            break;

        case AVP_IMS_SAR_REGISTRATION:
        case AVP_IMS_SAR_RE_REGISTRATION:

            for (h = msg->contact; h; h = h->next) {
                if (h->type == HDR_CONTACT_T && h->parsed) {

                    for (chi = ((contact_body_t*) h->parsed)->contacts; chi; chi =
                            chi->next) {
                        if (calc_contact_q(chi->q, &qvalue) != 0) {
                            LM_ERR("error on <%.*s>\n", chi->uri.len, chi->uri.s);
                            goto error;
                        }
                        sos = cscf_get_sos_uri_param(chi->uri);
                        if (sos < 0) {
                            LM_ERR("Error trying to determine if this is a sos contact <%.*s>\n", chi->uri.len, chi->uri.s);
                            goto error;
                        }
                        expires = calc_contact_expires(chi, expires_hdr, sos);
                        //TODO: this next line will fail if the expires is in the main body and not the contact body //FIXED
                        LM_DBG("Need to update contact: <%.*s>: "
                                "q_value [%d],"
                                "sos: [%d],"
                                "expires [%ld]\n", chi->uri.len, chi->uri.s, qvalue, sos, expires - time(NULL));

			LM_DBG("packing contact information\n");
                        if ((ci = pack_ci(msg, chi, expires, 0)) == 0) {
                            LM_ERR("Failed to extract contact info\n");
                            goto error;
                        }

                        LM_DBG("adding/updating contact based on prior existence\n");
                        //stick the contacts into usrloc
//			ul.lock_contact_slot(&chi->uri);
                        if (ul.get_ucontact(impu_rec, &chi->uri, ci->callid,
                                ci->path, ci->cseq, &ucontact) != 0) {	//get_contact returns with lock
                            LM_DBG("inserting new contact\n");
                            if (ul.insert_ucontact(impu_rec, &chi->uri, ci,
                                    &ucontact) != 0) {
                                LM_ERR("Error inserting contact <%.*s>\n", chi->uri.len, chi->uri.s);
//				ul.unlock_contact_slot(&chi->uri);
                                goto error;
                            }
                        } else {
                            LM_DBG("Contact already exists - updating\n");
                            if (ul.update_ucontact(impu_rec, ucontact, ci) != 0) {
                                LM_ERR("Error updating contact <%.*s>\n", chi->uri.len, chi->uri.s);
				ul.release_ucontact(ucontact);
//				ul.unlock_contact_slot(&chi->uri);
                                goto error;
                            }
			    ul.release_ucontact(ucontact);
                        }
			
			
//			ul.unlock_contact_slot(&chi->uri);
                    }
                }
            }
            break;
    }

    return 0;

error:
    return -1;
}

/*NB remember to lock udomain pritor to calling this*/
static inline int unregister_contact(udomain_t* _d, str* public_identity, contact_t* chi) {
    impurecord_t* impu_rec;
    struct ucontact* ucontact;
    str callid = {0, 0};
    str path = {0, 0};
    
    reg_subscriber *s;

    if (ul.get_impurecord(_d, public_identity, &impu_rec) != 0) {
        LM_ERR("Error, no public identity exists for <%.*s>\n", public_identity->len, public_identity->s);
        goto error;
    }

    if (ul.get_ucontact(impu_rec, &chi->uri, &callid, &path, 0/*cseq*/, &ucontact) != 0) {
        LM_ERR("Can't unregister contact that does not exist <%.*s>\n", chi->uri.len, chi->uri.s);
        goto error;
    }
    
    //Richard added this  - fix to remove subscribes that have presentity and watcher uri same as a contact aor that is being removed
    //When UEs explicitly dereg - they don't unsubscribe, so we remove subscriptions for them
    //only do this if ue_unsubscribe_on_dereg is set to 0
    if(!ue_unsubscribe_on_dereg){
	s = impu_rec->shead;
	LM_DBG("Checking if there is a subscription to this IMPU that has same watcher contact as this contact");
	while (s) {

	    LM_DBG("Subscription for this impurecord: watcher uri [%.*s] presentity uri [%.*s] watcher contact [%.*s] ", s->watcher_uri.len, s->watcher_uri.s, 
		    s->presentity_uri.len, s->presentity_uri.s, s->watcher_contact.len, s->watcher_contact.s);
	    LM_DBG("Contact to be removed [%.*s] ", ucontact->c.len, ucontact->c.s);
	    if(contact_port_ip_match(&s->watcher_contact, &ucontact->c)) {
	    //if ((s->watcher_contact.len == ucontact->c.len) && (strncasecmp(s->watcher_contact.s, ucontact->c.s, ucontact->c.len) == 0)) {
		LM_DBG("This contact has a subscription to its own status - so going to delete the subscription");
		ul.external_delete_subscriber(s, _d, 0 /*domain is locked*/);
	    }
	    s = s->next;
	}
    }
    
//    if (ul.delete_ucontact(impu_rec, ucontact) != 0) {
    ul.lock_contact_slot_i(ucontact->contact_hash);
    if (ul.unlink_contact_from_impu(impu_rec, ucontact, 1) != 0) {
        LM_ERR("Failed to delete ucontact <%.*s>\n", chi->uri.len, chi->uri.s);
    }
    ul.unlock_contact_slot_i(ucontact->contact_hash);
    ul.release_ucontact(ucontact);
    LM_DBG("Contact unlinked successfully <%.*s>\n", chi->uri.len, chi->uri.s);
    return 0;

error:
    return -1;
}

/* return
 * 1 - success(contacts left) - unregistered contacts and remaining contacts in contact buffer for reply message
 * 2 - success(no contacts left)
 * <=0 - on failure
 * */

int update_contacts_new(struct sip_msg* msg, udomain_t* _d,
        str* public_identity, int assignment_type, ims_subscription** s,
        str* ccf1, str* ccf2, str* ecf1, str* ecf2, contact_for_header_t** contact_header) {
    int reg_state, i, j;
    ims_public_identity* pi = 0;
    impurecord_t* impu_rec, *tmp_impu_rec;
    int expires_hdr = -1; //by default registration doesn't expire
    struct hdr_field* h;
    contact_t* chi; //contact header information
    qvalue_t qvalue;
    int sos = 0;
    ims_subscription* subscription = 0;
    int first_unbarred_impu = 1;		//this is used to flag the IMPU as anchor for implicit set
    int is_primary_impu = 0;
    int ret = 1;

    if (msg) {
        expires_hdr = cscf_get_expires_hdr(msg, 0); //get the expires from the main body of the sip message (global)
    }

    switch (assignment_type) {
        case AVP_IMS_SAR_REGISTRATION:
            LM_DBG("updating contacts in REGISTRATION state\n");
            reg_state = IMS_USER_REGISTERED;
            if (!s) {
                LM_ERR("no userdata supplied for AVP_IMS_SAR_REGISTRATION\n");
                goto error;
            }
            for (i = 0; i < (*s)->service_profiles_cnt; i++)
                for (j = 0; j < (*s)->service_profiles[i].public_identities_cnt; j++) {
                    pi = &((*s)->service_profiles[i].public_identities[j]);
                    ul.lock_udomain(_d, &pi->public_identity);
                    if (first_unbarred_impu && !pi->barring) {
                    	is_primary_impu = 1;
                    	first_unbarred_impu = 0;
                    } else {
                    	is_primary_impu = 0;
                    }
                    if (ul.update_impurecord(_d, &pi->public_identity, reg_state, -1 /*do not change send sar on delete */,
                            pi->barring, is_primary_impu, s, ccf1, ccf2, ecf1, ecf2, &impu_rec) != 0) {
                        LM_ERR("Unable to update impurecord for <%.*s>\n", pi->public_identity.len, pi->public_identity.s);
                        ul.unlock_udomain(_d, &pi->public_identity);
                        goto error;
                    }
                    //here we can do something with impu_rec if we want but we must unlock when done
                    //lets update the contacts
                    if (update_contacts_helper(msg, impu_rec, assignment_type, expires_hdr) != 0) {
                        LM_ERR("Failed trying to update contacts\n");
                        ul.unlock_udomain(_d, &pi->public_identity);
                        goto error;
                    }
                    ul.unlock_udomain(_d, &pi->public_identity);
                }
            //if we were successful up to this point, then we need to copy the contacts from main impu record (asserted IMPU) into the register response
            ul.lock_udomain(_d, public_identity);
            if (ul.get_impurecord(_d, public_identity, &impu_rec) != 0) {
                LM_ERR("Error, we should have a record after registraion\n");
                ul.unlock_udomain(_d, public_identity);
                goto error;
            }
            //now build the contact buffer to be include in the reply message and unlock
            build_contact(impu_rec, contact_header);
            ul.unlock_udomain(_d, public_identity);
            break;
        case AVP_IMS_SAR_RE_REGISTRATION:
            /* first update all the implicit IMPU based on the existing IMPUs subscription
             * then, once done with the implicits, update the explicit with the new subscription data
             */
            LM_DBG("updating contacts in RE-REGISTRATION state\n");
            reg_state = IMS_USER_REGISTERED;
            ul.lock_udomain(_d, public_identity);
            if (ul.get_impurecord(_d, public_identity, &impu_rec) != 0) {
                LM_ERR("No IMPU record fond for re-registration...aborting\n");
                ul.unlock_udomain(_d, public_identity);
                goto error;
            }

            if (update_contacts_helper(msg, impu_rec, assignment_type, expires_hdr) != 0) { //update the contacts for the explicit IMPU
                LM_ERR("Failed trying to update contacts for re-registration\n");
                ul.unlock_udomain(_d, public_identity);
                goto error;
            }
            //build the contact buffer for all registered contacts on explicit IMPU
            build_contact(impu_rec, contact_header);

            subscription = impu_rec->s;
            if (!subscription) {
                LM_ERR("No subscriber info associated with <%.*s>, not doing any implicit re-registrations\n", impu_rec->public_identity.len, impu_rec->public_identity.s);
                //update the new subscription infor for the explicit IMPU
                if (ul.update_impurecord(_d, public_identity, reg_state, -1 /*do not change send sar on delete */, 0 /*this is explicit so barring must be 0*/, 0, s, ccf1, ccf2,
                        ecf1, ecf2, &impu_rec) != 0) {
                    LM_ERR("Unable to update explicit impurecord for <%.*s>\n", public_identity->len, public_identity->s);
                }
                ul.unlock_udomain(_d, public_identity);
                build_contact(impu_rec, contact_header);
                break;
            }

            lock_get(subscription->lock);
            subscription->ref_count++;
            LM_DBG("ref count after add is now %d\n", subscription->ref_count);
            lock_release(subscription->lock);
            ul.unlock_udomain(_d, public_identity);

            //now update the implicit set
            for (i = 0; i < subscription->service_profiles_cnt; i++) {
                for (j = 0; j < subscription->service_profiles[i].public_identities_cnt; j++) {
                    pi = &((*s)->service_profiles[i].public_identities[j]);

                    if (memcmp(public_identity->s, pi->public_identity.s, public_identity->len) == 0) { //we don't need to update the explicit IMPU
                        LM_DBG("Ignoring explicit identity <%.*s>, updating later.....\n", public_identity->len, public_identity->s);
                        continue;
                    }
                    ul.lock_udomain(_d, &pi->public_identity);

                    //				LM_DBG("implicitly update IMPU <%.*s> for re-registration\n", pi->public_identity.len, pi->public_identity.s);
                    //				if (ul.get_impurecord(_d, &pi->public_identity,	&tmp_impu_rec) != 0) {
                    //					LM_ERR("Can't find IMPU for implicit re-registration update.....continuning\n");
                    //					ul.unlock_udomain(_d, &pi->public_identity);
                    //					continue;
                    //				}

                    //update the implicit IMPU with the new data
                    if (ul.update_impurecord(_d, &pi->public_identity,
                            reg_state, -1 /*do not change send sar on delete */, pi->barring, 0, s, ccf1, ccf2, ecf1, ecf2,
                            &impu_rec) != 0) {
                        LM_ERR("Unable to update implicit impurecord for <%.*s>.... continuing\n", pi->public_identity.len, pi->public_identity.s);
                        ul.unlock_udomain(_d, &pi->public_identity);
                        continue;
                    }

                    //update the contacts for the explicit IMPU
                    if (update_contacts_helper(msg, impu_rec, assignment_type, expires_hdr) != 0) {
                        LM_ERR("Failed trying to update contacts for re-registration of implicit IMPU <%.*s>.......continuing\n", pi->public_identity.len, pi->public_identity.s);
                        ul.unlock_udomain(_d, &pi->public_identity);
                        continue;
                    }
                    ul.unlock_udomain(_d, &pi->public_identity);
                }
            }
            lock_get(subscription->lock);
            subscription->ref_count--;
            LM_DBG("ref count after sub is now %d\n", subscription->ref_count);
            lock_release(subscription->lock);

            //finally we update the explicit IMPU record with the new data
            ul.lock_udomain(_d, public_identity);
            if (ul.update_impurecord(_d, public_identity, reg_state, -1 /*do not change send sar on delete */, 0 /*this is explicit so barring must be 0*/, 0, s, ccf1, ccf2, ecf1, ecf2, &impu_rec) != 0) {
                LM_ERR("Unable to update explicit impurecord for <%.*s>\n", public_identity->len, public_identity->s);
            }
            ul.unlock_udomain(_d, public_identity);
            break;
        case AVP_IMS_SAR_USER_DEREGISTRATION:
            /*TODO: if its not a star lets find all the contact records and remove them*/
            ul.lock_udomain(_d, public_identity);
            for (h = msg->contact; h; h = h->next) {
                if (h->type == HDR_CONTACT_T && h->parsed) {
                    for (chi = ((contact_body_t*) h->parsed)->contacts; chi; chi = chi->next) {
                        if (calc_contact_q(chi->q, &qvalue) != 0) {
                            LM_ERR("error on <%.*s>\n", chi->uri.len, chi->uri.s);
                            ul.unlock_udomain(_d, public_identity);
                            goto error;
                        }
                        sos = cscf_get_sos_uri_param(chi->uri);
                        if (sos < 0) {
                            LM_ERR("Error trying to determine if this is a sos contact <%.*s>\n", chi->uri.len, chi->uri.s);
                            ul.unlock_udomain(_d, public_identity);
                            goto error;
                        }
                        calc_contact_expires(chi, expires_hdr, sos);
                        if (unregister_contact(_d, public_identity, chi) != 0) {
                            LM_ERR("Unable to remove contact <%.*s\n", chi->uri.len, chi->uri.s);

                        }
                        //add this contact to the successful unregistered in the 200OK so the PCSCF can also see what is de-registered
                        build_expired_contact(chi, contact_header);
                    }
                }
            }
            /*now lets see if we still have any contacts left to decide on return value*/
            if (ul.get_impurecord(_d, public_identity, &impu_rec) != 0) {
                LM_ERR("Error retrieving impu record\n");
                ul.unlock_udomain(_d, public_identity);
                goto error;
            }

            if (impu_rec->num_contacts>=0 && impu_rec->newcontacts[0]) {
                LM_DBG("contacts still available\n");
                //TODO: add all other remaining contacts to reply message (contacts still registered for this IMPU)
                ret = 1;
            } else {
                LM_DBG("no more contacts available\n");
                ret = 2;
            }

            //before we release IMPU record - lets save the subscription for implicit dereg
            ims_subscription* subscription = impu_rec->s;

            if (!subscription) {
                LM_WARN("subscription is null..... continuing without de-registering implicit set\n");
                unlock(subscription->lock);
            } else {
                lock(subscription->lock);
                subscription->ref_count++; //this is so we can de-reg the implicit set just now without holding the lock on the current IMPU
                unlock(subscription->lock);

                ul.unlock_udomain(_d, public_identity);

                //lock(subscription->lock);
                for (i = 0; i < subscription->service_profiles_cnt; i++) {
                    for (j = 0; j < subscription->service_profiles[i].public_identities_cnt; j++) {
                        pi = &(subscription->service_profiles[i].public_identities[j]);
                        if (memcmp(public_identity->s, pi->public_identity.s, public_identity->len) == 0) { //we don't need to update the explicit IMPU
                            LM_DBG("Ignoring explicit identity <%.*s>, already de-reg/updated\n", public_identity->len, public_identity->s);
                            continue;
                        }
                        ul.lock_udomain(_d, &pi->public_identity);
                        if (ul.get_impurecord(_d, &pi->public_identity, &tmp_impu_rec) != 0) {
                            LM_ERR("Can't find IMPU for implicit de-registration update....continuing\n");
                            ul.unlock_udomain(_d, &pi->public_identity);
                            continue;
                        }
                        LM_DBG("Implicit deregistration of IMPU <%.*s>\n", pi->public_identity.len, pi->public_identity.s);
                        for (h = msg->contact; h; h = h->next) {
                            if (h->type == HDR_CONTACT_T && h->parsed) {
                                for (chi = ((contact_body_t*) h->parsed)->contacts; chi; chi = chi->next) {
                                    if (calc_contact_q(chi->q, &qvalue) != 0) {
                                        LM_ERR("error on <%.*s>\n", chi->uri.len, chi->uri.s);
                                        ul.unlock_udomain(_d, &pi->public_identity);
                                        LM_ERR("no q value of implicit de-reg....continuing\n");
                                        continue;
                                    }
                                    sos = cscf_get_sos_uri_param(chi->uri);
                                    if (sos < 0) {
                                        LM_ERR("Error trying to determine if this is a sos contact <%.*s>\n", chi->uri.len, chi->uri.s);
                                        ul.unlock_udomain(_d, public_identity);
                                        goto error;
                                    }
                                    calc_contact_expires(chi, expires_hdr, sos);
                                    if (unregister_contact(_d, &pi->public_identity, chi) != 0) {
                                        LM_ERR("Unable to remove contact <%.*s\n", chi->uri.len, chi->uri.s);

                                    }
                                }
                            }
                        }
                        /*now lets see if we still have any contacts left to decide on return value*/
                        if (ul.get_impurecord(_d, &pi->public_identity, &impu_rec) != 0) {
                            LM_ERR("Error retrieving impu record for implicit de-reg....continuing\n");
                            ul.unlock_udomain(_d, &pi->public_identity);
                            continue;
                        }

                        if (impu_rec->num_contacts && impu_rec->newcontacts[0])
                            LM_DBG("contacts still available after implicit dereg for IMPU: <%.*s>\n", pi->public_identity.len, pi->public_identity.s);
                        else {
                            LM_DBG("no contacts left after implicit dereg for IMPU: <%.*s>\n", pi->public_identity.len, pi->public_identity.s);
                            LM_DBG("Updating impu record to not send SAR on delete as this is explicit dereg");
			    reg_state = IMS_USER_REGISTERED;//keep reg_state as it is
			    if (ul.update_impurecord(_d, &pi->public_identity, reg_state, 0 /*do not send sar on delete */, -1 /*do not change barring*/, 0, 0, 0, 0, 0, 0, &tmp_impu_rec) != 0) {
				    LM_ERR("Unable to update explicit impurecord for <%.*s>\n", pi->public_identity.len, pi->public_identity.s);
			    }
                        }

                        ul.unlock_udomain(_d, &pi->public_identity);
                    }
                }
                lock(subscription->lock);
                subscription->ref_count--;
                unlock(subscription->lock);
            }

	    if (ret == 2) {
                LM_DBG("no contacts left after explicit dereg for IMPU: <%.*s>\n", public_identity->len, public_identity->s);
                LM_DBG("Updating impu record to not send SAR on delete as this is explicit dereg");
		reg_state = IMS_USER_REGISTERED;//keep reg_state as it is
		ul.lock_udomain(_d, public_identity);
		if (ul.update_impurecord(_d, public_identity, reg_state, 0 /*do not send sar on delete */, -1 /*do not change barring*/, 0, 0, 0, 0, 0, 0, &tmp_impu_rec) != 0) {
			LM_ERR("Unable to update explicit impurecord for <%.*s>\n", public_identity->len, public_identity->s);
		}
		ul.unlock_udomain(_d, public_identity);
            }
            break;

        case AVP_IMS_SAR_UNREGISTERED_USER:
            LM_DBG("updating contacts for UNREGISTERED_USER state\n");
            reg_state = IMS_USER_UNREGISTERED;
            for (i = 0; i < (*s)->service_profiles_cnt; i++)
                for (j = 0; j < (*s)->service_profiles[i].public_identities_cnt;
                        j++) {
                    pi = &((*s)->service_profiles[i].public_identities[j]);
                    ul.lock_udomain(_d, &pi->public_identity);
                    if (ul.update_impurecord(_d, &pi->public_identity, reg_state, -1 /*do not change send sar on delete */,
                            pi->barring, 0, s, ccf1, ccf2, ecf1, ecf2, &impu_rec)
                            != 0) {
                        LM_ERR("Unable to update impurecord for <%.*s>\n", pi->public_identity.len, pi->public_identity.s);
                        ul.unlock_udomain(_d, &pi->public_identity);
                        goto error;
                    }
                    ul.unlock_udomain(_d, &pi->public_identity);
                }
            //if we were successful up to this point, then we need to copy the contacts from main impu record (asserted IMPU) into the register response
            break;
        default:
            LM_ERR("unimplemented assignment_type when trying to update contacts\n");
    }
    return ret;

error:
    return -1;

}

int assign_server_unreg(struct sip_msg* _m, char* str1, str* direction, char* route) {
    str private_identity = {0, 0}, public_identity = {0, 0};
    int assignment_type = AVP_IMS_SAR_NO_ASSIGNMENT;
    int data_available = AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE;
    int require_user_data = 1;
    rerrno = R_FINE;
    tm_cell_t *t = 0;
    str route_name;

    saved_transaction_t* saved_t;
    cfg_action_t* cfg_action;

    udomain_t* _d = (udomain_t*) str1;
    
    if (fixup_get_svalue(_m, (gparam_t*) route, &route_name) != 0) {
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
    
    LM_DBG("Assigning unregistered user for direction [%.*s]\n", direction->len, direction->s);

    enum cscf_dialog_direction dir = cscf_get_dialog_direction(direction->s);
    switch (dir) {
        case CSCF_MOBILE_ORIGINATING:
            public_identity = cscf_get_asserted_identity(_m, 0);
            break;
        case CSCF_MOBILE_TERMINATING:
            public_identity = cscf_get_public_identity_from_requri(_m);
            break;
        default:
            LM_ERR("Bad dialog direction [%.*s]\n", direction->len, direction->s);
            rerrno = R_SAR_FAILED;
            goto error;
    }

    if (!public_identity.s || public_identity.len <= 0) {
        LM_ERR("No public identity\n");
        rerrno = R_SAR_FAILED;
        goto error;
    }

    assignment_type = AVP_IMS_SAR_UNREGISTERED_USER;
    data_available = AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE; //TODO: check this


    //before we send lets suspend the transaction
    t = tmb.t_gett();
    if (t == NULL || t == T_UNDEFINED) {
        if (tmb.t_newtran(_m) < 0) {
            LM_ERR("cannot create the transaction for SAR async\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }
        t = tmb.t_gett();
        if (t == NULL || t == T_UNDEFINED) {
            LM_ERR("cannot lookup the transaction\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }
    }

    saved_t = shm_malloc(sizeof (saved_transaction_t));
    if (!saved_t) {
        LM_ERR("no more memory trying to save transaction state\n");
        rerrno = R_SAR_FAILED;
        goto error;

    }
    memset(saved_t, 0, sizeof (saved_transaction_t));
    saved_t->act = cfg_action;

    saved_t->expires = 1; //not a dereg as this is server_assign_unreg
    saved_t->require_user_data = require_user_data;
    saved_t->sar_assignment_type = assignment_type;
    saved_t->domain = (udomain_t*) _d;

    saved_t->contact_header = 0;

    LM_DBG("Setting default AVP return code used for async callbacks to default as ERROR \n");
    create_return_code(CSCF_RETURN_ERROR);

    LM_DBG("Suspending SIP TM transaction\n");
    if (tmb.t_suspend(_m, &saved_t->tindex, &saved_t->tlabel) < 0) {
        LM_ERR("failed to suspend the TM processing\n");
        free_saved_transaction_data(saved_t);
        rerrno = R_SAR_FAILED;
        goto error;
    }

    if (scscf_assign_server(_m, public_identity, private_identity, assignment_type, data_available, saved_t) != 0) {
        LM_ERR("ERR:I_MAR: Error sending SAR or SAR time-out\n");
        tmb.t_cancel_suspend(saved_t->tindex, saved_t->tlabel);
        free_saved_transaction_data(saved_t);
        rerrno = R_SAR_FAILED;
        goto error;
    }

    if (public_identity.s && dir == CSCF_MOBILE_TERMINATING)
        shm_free(public_identity.s); // shm_malloc in cscf_get_public_identity_from_requri

    return CSCF_RETURN_BREAK;


error:
    update_stat(rejected_registrations, 1);
    if ((is_route_type(REQUEST_ROUTE)) && (reg_send_reply(_m, 0) < 0))
        return CSCF_RETURN_ERROR;
    return CSCF_RETURN_BREAK;


}

/*!\brief
 * Process REGISTER request and save it's contacts
 * 1. ensure request
 * 2. get impu, impi,realm,expiry,etc
 * 3. check expiry
 * 4. do SAR (reg,re-reg,dereg and no contacts left)
 * 5. update usrloc based on SAR results
 */
//int save(struct sip_msg* msg, udomain_t* _d) {

int save(struct sip_msg* msg, char* str1, char *route) {
    int expires;
    int require_user_data = 0;
    int data_available;
    contact_t* c;
    int st;
    str public_identity, private_identity, realm;
    int sar_assignment_type = AVP_IMS_SAR_NO_ASSIGNMENT;
    str route_name;
    
    udomain_t* _d = (udomain_t*) str1;

    rerrno = R_FINE;
    get_act_time();

    tm_cell_t *t = 0;

    saved_transaction_t* saved_t;
    cfg_action_t* cfg_action;

    contact_for_header_t* contact_header = 0;

    if (fixup_get_svalue(msg, (gparam_t*) route, &route_name) != 0) {
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

    //check which route block we are in - if not request then we fail out.
    if (!is_route_type(REQUEST_ROUTE)) {
        LM_ERR("Should only be called in REQUEST route\n");
        rerrno = R_SAR_FAILED;
        goto error;
    }

    /* check and see that all the required headers are available and can be parsed */
    if (parse_message_for_register(msg) < 0) {
        LM_ERR("Unable to parse register message correctly\n");
        rerrno = R_SAR_FAILED;
        goto error;
    }
    /** check we have valid contacts according to IMS spec. */
    if (check_contacts(msg, &st) > 0) {
        LM_ERR("contacts not valid for REGISTER\n");
        rerrno = R_SAR_FAILED;
        goto error;
    }

    /* get IMPU,IMPI,realm,expires */
    public_identity = cscf_get_public_identity(msg);
    if (public_identity.len <= 0 || !public_identity.s) {
        LM_ERR("failed to extract Address Of Record\n");
        rerrno = R_SAR_FAILED;
        goto error;
    }
    realm = cscf_get_realm_from_uri(public_identity);
    if (realm.len <= 0 || !realm.s) {
        LM_ERR("can't get realm\n");
        rerrno = R_SAR_FAILED;
        goto error;
    }

    private_identity = cscf_get_private_identity(msg, realm);
    if (private_identity.len <= 0 || !private_identity.s) {
        LM_ERR("cant get private identity\n");
    }

    expires = cscf_get_max_expires(msg, 0); //check all contacts for max expires
    if (expires != 0) { //if <0 then no expires was found in which case we treat as reg/re-reg with default expires.
        if (is_impu_registered(_d, &public_identity)) {
            LM_DBG("preparing for SAR assignment for RE-REGISTRATION <%.*s>\n", public_identity.len, public_identity.s);
            sar_assignment_type = AVP_IMS_SAR_RE_REGISTRATION;
        } else {
            LM_DBG("preparing for SAR assignment for new REGISTRATION <%.*s>\n", public_identity.len, public_identity.s);
            sar_assignment_type = AVP_IMS_SAR_REGISTRATION;
            require_user_data = 1;
        }
    } else {//de-reg
        if (store_data_on_dereg) {
            LM_DBG("preparing for SAR assignment for DE-REGISTRATION with storage <%.*s>\n", public_identity.len, public_identity.s);
            sar_assignment_type = AVP_IMS_SAR_USER_DEREGISTRATION_STORE_SERVER_NAME;
        } else {
            LM_DBG("preparing for SAR assignment for DE-REGISTRATION <%.*s>\n", public_identity.len, public_identity.s);
            sar_assignment_type = AVP_IMS_SAR_USER_DEREGISTRATION;
        }

        c = get_first_contact(msg);
        if (!c && !st) { //no contacts found - no need to do anything
            LM_ERR("no contacts found for de-registration and no star\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }

        //if we get here there are contacts to deregister, BUT we only send a SAR if there are no contacts (valid) left.
        if (!st) { //if it is a star then we delete all contacts and send a SAR
            //unregister the requested contacts, if none left at the end then send a SAR, otherwise return successfully
            LM_DBG("need to unregister contacts\n");
            //lets update the contacts - we need to know if all were deleted or not for the public identity
            int res = update_contacts_new(msg, _d, &public_identity, sar_assignment_type, 0, 0, 0, 0, 0, &contact_header);
            if (res <= 0) {
                LM_ERR("Error processing REGISTER for de-registration\n");
                free_contact_buf(contact_header);
                rerrno = R_SAR_FAILED;
                goto error;
            } else if (res == 2) {
                //send sar
                LM_DBG("no contacts left after de-registration, doing SAR\n");
            } else { //res=1
                //still contacts left so return success
                LM_DBG("contacts still available after deregister.... not doing SAR\n");
                //we must send the de reged contacts in this! so we only free the contact header after sending
                //free_contact_buf(contact_header);
                rerrno = R_FINE;
                goto no_sar;
            }
        }
    }
    
    if (!user_data_always) {
	if (require_user_data)
	    data_available = AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE;
	else
	    data_available = AVP_IMS_SAR_USER_DATA_ALREADY_AVAILABLE;
    } else {
	data_available = AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE;
    }
    
    //before we send lets suspend the transaction
    t = tmb.t_gett();
    if (t == NULL || t == T_UNDEFINED) {
        if (tmb.t_newtran(msg) < 0) {
            LM_ERR("cannot create the transaction for SAR async\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }
        t = tmb.t_gett();
        if (t == NULL || t == T_UNDEFINED) {
            LM_ERR("cannot lookup the transaction\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }
    }

    saved_t = shm_malloc(sizeof (saved_transaction_t));
    if (!saved_t) {
        LM_ERR("no more memory trying to save transaction state\n");
        free_contact_buf(contact_header);
        rerrno = R_SAR_FAILED;
        goto error;

    }
    memset(saved_t, 0, sizeof (saved_transaction_t));
    saved_t->act = cfg_action;

    //this is not server assign unreg - this is a save
    saved_t->expires = expires;
    saved_t->require_user_data = require_user_data;
    saved_t->sar_assignment_type = sar_assignment_type;
    
    saved_t->domain = _d;

    saved_t->public_identity.s = (char*) shm_malloc(public_identity.len + 1);
    if (!saved_t->public_identity.s) {
        LM_ERR("no more memory trying to save transaction state : callid\n");
        shm_free(saved_t);
        free_contact_buf(contact_header);
        rerrno = R_SAR_FAILED;
        goto error;
    }
    memset(saved_t->public_identity.s, 0, public_identity.len + 1);
    memcpy(saved_t->public_identity.s, public_identity.s, public_identity.len);
    saved_t->public_identity.len = public_identity.len;

    saved_t->contact_header = contact_header;

    create_return_code(CSCF_RETURN_ERROR);

    LM_DBG("Suspending SIP TM transaction\n");
    if (tmb.t_suspend(msg, &saved_t->tindex, &saved_t->tlabel) < 0) {
        LM_ERR("failed to suspend the TM processing\n");
        free_saved_transaction_data(saved_t);
        rerrno = R_SAR_FAILED;
        goto error;
    }

    if (scscf_assign_server(msg, public_identity, private_identity, sar_assignment_type, data_available, saved_t) != 0) {
        LM_ERR("ERR:I_MAR: Error sending SAR or SAR time-out\n");
        tmb.t_cancel_suspend(saved_t->tindex, saved_t->tlabel);
        free_saved_transaction_data(saved_t);
        rerrno = R_SAR_FAILED;
        goto error;
    }

    return CSCF_RETURN_BREAK;

no_sar:

    update_stat(accepted_registrations, 1);

        //we must send the de reged contacts in this! so we only free the contact header after sending
    /* Only send reply upon request, not upon reply */
    if ((is_route_type(REQUEST_ROUTE)) && (reg_send_reply(msg, contact_header) < 0)) {
        free_contact_buf(contact_header);
        return CSCF_RETURN_ERROR;
    }
    free_contact_buf(contact_header);
    return CSCF_RETURN_BREAK;

error:
    update_stat(rejected_registrations, 1);
    if ((is_route_type(REQUEST_ROUTE)) && (reg_send_reply(msg, contact_header) < 0))
        return CSCF_RETURN_ERROR;
    return CSCF_RETURN_BREAK;

}

int unregister(struct sip_msg* _m, char* _d, char* _uri) {
    str aor = {0, 0};
    str uri = {0, 0};

    if (fixup_get_svalue(_m, (gparam_p) _uri, &uri) != 0 || uri.len <= 0) {
        LM_ERR("invalid uri parameter\n");
        return -1;
    }

    if (extract_aor(&uri, &aor) < 0) {
        LM_ERR("failed to extract Address Of Record\n");
        return -1;
    }

    if (star((udomain_t*) _d, &aor) < 0) {
        LM_ERR("error unregistering user [%.*s]\n", aor.len, aor.s);
        return -1;
    }
    return 1;
}

