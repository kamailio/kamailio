#include "ims_charging_mod.h"
#include "dialog.h"
#include "ro_session_hash.h"
#include "ro_db_handler.h"
#include "ims_charging_stats.h"
#include "../../core/parser/hf.h"

struct cdp_binds cdpb;

extern int ro_db_mode;
extern char *domain;
extern struct dlg_binds dlgb;
extern struct ims_charging_counters_h ims_charging_cnts_h;

void dlg_callback_received(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
    LM_DBG("Received dialog callback event [%d]\n", type);
    unsigned int termcode = 0;
    switch (type) {
        case DLGCB_CONFIRMED:
            dlg_answered(dlg, type, _params);
            break;
        case DLGCB_TERMINATED:
            dlg_terminated(dlg, type, termcode, "normal call clearing", _params);
            break;
        case DLGCB_FAILED:
            dlg_terminated(dlg, type, termcode, "call failed", _params);
            break;
        case DLGCB_EXPIRED:
            dlg_terminated(dlg, type, termcode, "dialog timeout", _params);
            break;
        default:
            LM_WARN("Received unknown dialog callback [%d]\n", type);
    }
}

void dlg_answered(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
    struct ro_session* session = 0;
    struct ro_session_entry* ro_session_entry;
    time_t now = get_current_time_micro();
    time_t time_since_last_event;

    LM_DBG("dlg_reply callback entered\n");

    if (!_params) {
        return;
    }

    session = (struct ro_session*) *_params->param;
    if (!session) {
        LM_ERR("Ro Session object is NULL...... aborting\n");
        return;
    }
    
    LM_DBG("Call answered on dlg [%p] - search for Ro Session [%p]\n", dlg, session);

    ro_session_entry = &(ro_session_table->entries[session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);

    if (session->active) {
        LM_CRIT("Why the heck am i receiving a double confirmation of the dialog? Ignoring... ");
        ro_session_unlock(ro_session_table, ro_session_entry);
        return;
    } else if (session->active < 0) {   //session has already been terminated - we can't reactivate...
        LM_WARN("Received an answer after terminating dialog.... ignoring\n");
        ro_session_unlock(ro_session_table, ro_session_entry);
        return;
    }

    time_since_last_event = (now - session->last_event_timestamp)/1000000;
    session->start_time = session->last_event_timestamp = now;
    session->event_type = answered;
    session->active = 1;

    /* check to make sure that the validity of the credit is enough for the bundle */
    int ret = 0;
    LM_DBG("we were granted %d seconds (valid for %d seconds) and it's been %d seconds since we requested\n", (int)session->reserved_secs, (int)session->valid_for, (int)time_since_last_event);
    if (session->reserved_secs < (session->valid_for - time_since_last_event)) {
        if (session->reserved_secs > ro_timer_buffer/*TIMEOUTBUFFER*/) {
            ret = insert_ro_timer(&session->ro_tl, session->reserved_secs - (session->is_final_allocation?0:ro_timer_buffer)); //subtract 5 seconds so as to get more credit before we run out
        } else {
            ret = insert_ro_timer(&session->ro_tl, session->reserved_secs);
        }
    } else {
        if (session->valid_for > ro_timer_buffer) {
            ret = insert_ro_timer(&session->ro_tl, session->valid_for - (session->is_final_allocation?0:ro_timer_buffer)); //subtract 5 seconds so as to get more credit before we run out
        } else {
            ret = insert_ro_timer(&session->ro_tl, session->valid_for);
        }
    }


    if (ret != 0) {
        LM_CRIT("unable to insert timer for Ro Session [%.*s]\n", session->ro_session_id.len, session->ro_session_id.s);
    } else {
        ref_ro_session(session, 1, 0); // lock already acquired
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
        return;
    }

    cdpb.AAAStartChargingCCAccSession(cdp_session);
    cdpb.AAASessionsUnlock(cdp_session->hash);


}

void dlg_terminated(struct dlg_cell *dlg, int type, unsigned int termcode, char* reason, struct dlg_cb_params *_params) {
	//int i;
	int unref = 0;
	struct ro_session *ro_session = 0;
	struct ro_session_entry *ro_session_entry;
	struct sip_msg *request;
        str s_reason;
        
        s_reason.s = reason;
        s_reason.len = strlen(reason);
	
	LM_DBG("dialog [%p] terminated on type [%d], lets send stop record\n", dlg, type);

	if (!_params) {
		return;
	}
	
	LM_DBG("Direction is %d\n", _params->direction);
	if (_params->req) {
		if (_params->req->first_line.u.request.method_value == METHOD_BYE) {
			if (_params->direction == DLG_DIR_DOWNSTREAM) {
				LM_DBG("Dialog ended by Caller\n");
			} else {
				LM_DBG("Dialog ended by Callee\n");
			}
		} else {
			LM_DBG("Request is %.*s\n", _params->req->first_line.u.request.method.len, _params->req->first_line.u.request.method.s);
		}

		struct hdr_field* h = get_hdr_by_name(_params->req, "Reason", 6);
		if(h!=NULL){
                        LM_DBG("reason header is [%.*s]\n", h->body.len, h->body.s);
			s_reason = h->body;
		}
	} else if (_params->rpl) {
		LM_DBG("Reply is [%d - %.*s]", _params->rpl->first_line.u.reply.statuscode, _params->rpl->first_line.u.reply.reason.len, _params->rpl->first_line.u.reply.reason.s);
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
                                
                                LM_DBG("processing dlg_terminated in Ro and session [%.*s] has active = %d", ro_session->ro_session_id.len, ro_session->ro_session_id.s, ro_session->active);
				if ((!ro_session->active && (ro_session->start_time != 0)) || (ro_session->ccr_sent == 1)) {
					unref_ro_session(ro_session,1,0);
					LM_DBG("CCR already sent or Ro Session is not active, but may have been answered [%d]\n", (int)ro_session->start_time);
					ro_session_unlock(ro_session_table, ro_session_entry);
					return;
				}

				if (ro_session->active) { // if the call was never activated, there's no timer to remove
					int ret = remove_ro_timer(&ro_session->ro_tl);
					if (ret < 0) {
						LM_CRIT("unable to unlink the timer on ro_session %p [%.*s]\n", 
							ro_session, ro_session->ro_session_id.len, ro_session->ro_session_id.s);
					} else if (ret > 0) {
						LM_WARN("inconsistent ro timer data on ro_session %p [%.*s]\n",
							ro_session, ro_session->ro_session_id.len, ro_session->ro_session_id.s);						
					} else {
						unref++;
					}
				}

				LM_DBG("Sending CCR STOP on Ro_Session [%p]\n", ro_session);
				send_ccr_stop_with_param(ro_session, termcode, &s_reason);
				ro_session->active = -1;    //deleted.... terminated ....
                                ro_session->ccr_sent = 1;
//                                counter_add(ims_charging_cnts_h.active_ro_sessions, -1);
				
				if (ro_db_mode == DB_MODE_REALTIME) {
				    ro_session->flags |= RO_SESSION_FLAG_DELETED;
				    if (update_ro_dbinfo_unsafe(ro_session) != 0) {
					LM_ERR("Unable to update Ro session in DB...continuing\n");
				    }
				}
				
				//ro_session->start_time;
				unref_ro_session(ro_session, 1+unref, 0); //lock already acquired
				//unref_ro_session_unsafe(ro_session, 2+unref, ro_session_entry); //lock already acquired
				ro_session_unlock(ro_session_table, ro_session_entry);
			//}
		//}
	}
}
