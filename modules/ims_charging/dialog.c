#include "mod.h"
#include "dialog.h"
#include "ro_session_hash.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../ims_usrloc_scscf/udomain.h"
#include "ro_db_handler.h"

struct cdp_binds cdpb;

extern usrloc_api_t ul;
extern int ro_db_mode;
extern char *domain;

void dlg_reply(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
	struct sip_msg *reply;
	struct ro_session* session = 0;
	struct ro_session_entry* ro_session_entry;
	time_t now = time(0);
	time_t time_since_last_event;

	LM_DBG("dlg_reply callback entered\n");
	
	if (!_params) {
		return;
	}
	
	reply = _params->rpl;
	if (!reply) {
		LM_WARN("dlg_reply has no SIP reply associated.\n");
		return;
	}

	if (reply != FAKED_REPLY && reply->REPLY_STATUS == 200) {
		LM_DBG("Call answered on dlg [%p] - search for Ro Session and initialise timers.\n", dlg);
		
		session = (struct ro_session*)*_params->param;
		if (!session) {
			LM_ERR("Ro Session object is NULL...... aborting\n");
			return;
		}

		ro_session_entry = &(ro_session_table->entries[session->h_entry]);

		ro_session_lock(ro_session_table, ro_session_entry);

		if (session->active) {
			LM_CRIT("Why the heck am i receiving a double confirmation of the dialog? Ignoring... ");
			ro_session_unlock(ro_session_table, ro_session_entry);
			return;
		}
	
		time_since_last_event = now - session->last_event_timestamp;
		session->start_time = session->last_event_timestamp = now;
		session->event_type = answered;
		session->active = 1;
		

		/* check to make sure that the validity of the credit is enough for the bundle */
		int ret = 0;
		if (session->reserved_secs < (session->valid_for - time_since_last_event)) {
			if (session->reserved_secs > ro_timer_buffer/*TIMEOUTBUFFER*/) {
				ret = insert_ro_timer(&session->ro_tl, session->reserved_secs - ro_timer_buffer); //subtract 5 seconds so as to get more credit before we run out
			} else {
				ret = insert_ro_timer(&session->ro_tl, session->reserved_secs);
			}
		} else {
			if (session->valid_for > ro_timer_buffer) {
				ret = insert_ro_timer(&session->ro_tl, session->valid_for - ro_timer_buffer); //subtract 5 seconds so as to get more credit before we run out
			} else {
				ret = insert_ro_timer(&session->ro_tl, session->valid_for);
			}
		}


		if (ret != 0) {
			LM_CRIT("unable to insert timer for Ro Session [%.*s]\n", session->ro_session_id.len, session->ro_session_id.s); 
		} else {
			ref_ro_session_unsafe(session, 1); // lock already acquired
		}

		if (ro_db_mode == DB_MODE_REALTIME) {
		    session->flags |= RO_SESSION_FLAG_CHANGED;
		    if (update_ro_dbinfo_unsafe(session) != 0) {
			LM_ERR("Failed to update ro_session in database... continuing\n");
		    };
		}
				
		ro_session_unlock(ro_session_table, ro_session_entry);

		AAASession* cdp_session = cdpb.AAAGetCCAccSession(session->ro_session_id);
		if (!cdp_session) {
			LM_ERR("could not find find CC App CDP session for session [%.*s]\n", session->ro_session_id.len, session->ro_session_id.s);
//			ro_session_unlock(ro_session_table, ro_session_entry);
			return;
		}
		
//		ro_session_unlock(ro_session_table, ro_session_entry);

		cdpb.AAAStartChargingCCAccSession(cdp_session);
		cdpb.AAASessionsUnlock(cdp_session->hash);		
		
//		unref_ro_session(session, 1);	DONT need this anymore because we don't do lookup so no addition to ref counter
	}
}

void dlg_terminated(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
	//int i;
	int unref = 0;
	struct ro_session *ro_session = 0;
	struct ro_session_entry *ro_session_entry;
	struct sip_msg *request;
	
	LM_DBG("dialog [%p] terminated, lets send stop record\n", dlg);

	if (!_params) {
		return;
	}
	
	ro_session = (struct ro_session*)*_params->param;
	if (!ro_session) {
		LM_ERR("Ro Session object is NULL...... aborting\n");
		return;
	}
	
	request = _params->req;
	if (!request) {
		LM_WARN("dlg_terminated has no SIP request associated.\n");
	}

	if (dlg && (dlg->callid.s && dlg->callid.len > 0)) {
		
	    
	    /* find the session for this call, possibly both for orig and term*/
		//for (i=0; i<2; i++) {
			//TODO: try and get the Ro session specifically for terminating dialog or originating one
			//currently the way we are doing is a hack.....
			//if ((ro_session = lookup_ro_session(dlg->h_entry, &dlg->callid, 0, 0))) {
				ro_session_entry =
						&(ro_session_table->entries[ro_session->h_entry]);

				//if the Ro session is not active we don't need to do anything. This prevents
				//double processing for various dialog_terminated callback events.
				//If however, the call was never answered, then we can continue as normal
				ro_session_lock(ro_session_table, ro_session_entry);
				if (!ro_session->active && (ro_session->start_time != 0)) {
					unref_ro_session(ro_session,1);
					LM_ERR("Ro Session is not active, but may have been answered [%d]\n", (int)ro_session->start_time);
					ro_session_unlock(ro_session_table, ro_session_entry);
					return;
				}
			
	

				if (ro_session->active) { // if the call was never activated, there's no timer to remove
					int ret = remove_ro_timer(&ro_session->ro_tl);
					if (ret < 0) {
						LM_CRIT("unable to unlink the timer on ro_session %p [%.*s]\n", 
							ro_session, ro_session->cdp_session_id.len, ro_session->cdp_session_id.s);
					} else if (ret > 0) {
						LM_WARN("inconsistent ro timer data on ro_session %p [%.*s]\n",
							ro_session, ro_session->cdp_session_id.len, ro_session->cdp_session_id.s);						
					} else {
						unref++;
					}
				}

				LM_DBG("Sending CCR STOP on Ro_Session [%p]\n", ro_session);
				send_ccr_stop(ro_session);
				ro_session->active = 0;
				
				if (ro_db_mode == DB_MODE_REALTIME) {
				    ro_session->flags |= RO_SESSION_FLAG_DELETED;
				    if (update_ro_dbinfo_unsafe(ro_session) != 0) {
					LM_ERR("Unable to update Ro session in DB...continuing\n");
				    }
				}
				
				//ro_session->start_time;
				unref_ro_session_unsafe(ro_session, 1+unref, ro_session_entry); //lock already acquired
				//unref_ro_session_unsafe(ro_session, 2+unref, ro_session_entry); //lock already acquired
				ro_session_unlock(ro_session_table, ro_session_entry);
			//}
		//}
	}
}

void remove_dlg_data_from_contact(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
        
    struct impu_data *impu_data;
    impurecord_t* implicit_impurecord = 0;
    struct ucontact* ucontact;
    str callid = {0, 0};
    str path = {0, 0};
    udomain_t* domain_t;
    
    LM_DBG("dialog [%p] terminated, lets remove dlg data from contact\n", dlg);
    
    if (ul.register_udomain(domain, &domain_t) < 0) {
	    LM_ERR("Unable to register usrloc domain....aborting\n");
	    return;
    }
    
    if(_params && _params->param){
	impu_data = (struct impu_data*)*_params->param;
	if (!impu_data) {
		LM_ERR("IMPU data object is NULL...... aborting\n");
		return;
	}
	
	LM_DBG("IMPU data is present, contact: <%.*s> identity <%.*s>", impu_data->contact.len, impu_data->contact.s, impu_data->identity.len, impu_data->identity.s);
	LM_DBG("IMPU data domain <%.*s>", domain_t->name->len, domain_t->name->s);
	
	ul.lock_udomain(domain_t, &impu_data->identity);
	if (ul.get_impurecord(domain_t, &impu_data->identity, &implicit_impurecord) != 0) {
	    LM_DBG("usrloc does not have imprecord for implicity IMPU, ignore\n");
	}else {
	    if (ul.get_ucontact(implicit_impurecord, &impu_data->contact, &callid, &path, 0/*cseq*/,  &ucontact) != 0) { //contact does not exist
		LM_DBG("This contact: <%.*s> is not in usrloc, ignore - NOTE: You need S-CSCF usrloc set to match_mode CONTACT_PORT_IP_ONLY\n", impu_data->contact.len, impu_data->contact.s);
	    } else {//contact exists so add dialog data to it
		ul.remove_dialog_data_from_contact(ucontact, dlg->h_entry, dlg->h_id);
		ul.release_ucontact(ucontact);
	    }
	}
	ul.unlock_udomain(domain_t, &impu_data->identity);
	free_impu_data(impu_data);
    }
}

void add_dlg_data_to_contact(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
    
    struct impu_data *impu_data;
    impurecord_t* implicit_impurecord = 0;
    struct ucontact* ucontact;
    str callid = {0, 0};
    str path = {0, 0};
    udomain_t* domain_t;
    
    LM_DBG("dialog [%p] confirmed, lets add dlg data to contact\n", dlg);
    
    if (ul.register_udomain(domain, &domain_t) < 0) {
	    LM_ERR("Unable to register usrloc domain....aborting\n");
	    return;
    }
    
    if(_params && _params->param){
	impu_data = (struct impu_data*)*_params->param;
	if (!impu_data) {
		LM_ERR("IMPU data object is NULL...... aborting\n");
		return;
	}
	
	LM_DBG("IMPU data is present, contact: <%.*s> identity <%.*s>", impu_data->contact.len, impu_data->contact.s, impu_data->identity.len, impu_data->identity.s);
	LM_DBG("IMPU data domain <%.*s>", domain_t->name->len, domain_t->name->s);
	
	ul.lock_udomain(domain_t, &impu_data->identity);
	if (ul.get_impurecord(domain_t, &impu_data->identity, &implicit_impurecord) != 0) {
	    LM_DBG("usrloc does not have imprecord for implicity IMPU, ignore\n");
	}else {
	    if (ul.get_ucontact(implicit_impurecord, &impu_data->contact, &callid, &path, 0/*cseq*/,  &ucontact) != 0) { //contact does not exist
		LM_DBG("This contact: <%.*s> is not in usrloc, ignore - NOTE: You need S-CSCF usrloc set to match_mode CONTACT_PORT_IP_ONLY\n", impu_data->contact.len, impu_data->contact.s);
	    } else {//contact exists so add dialog data to it
		ul.add_dialog_data_to_contact(ucontact, dlg->h_entry, dlg->h_id);
		ul.release_ucontact(ucontact);
	    }
	}
	ul.unlock_udomain(domain_t, &impu_data->identity);
    }
} 
