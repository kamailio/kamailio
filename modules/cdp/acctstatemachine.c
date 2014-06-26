/*
 * acctstatemachine.c
 *
 *  Created on: 03 Apr 2013
 *      Author: jaybeepee
 */

#include "acctstatemachine.h"
#include "diameter_ims.h"
#include "common.h"

/**
 * update Granted Service Unit timers based on CCR
 */
inline void update_gsu_request_timers(cdp_cc_acc_session_t* session, AAAMessage* msg) {
	AAA_AVP *avp;

	avp = AAAFindMatchingAVP(msg, 0, AVP_Event_Timestamp, 0, 0);
	if (avp && avp->data.len == 4) {
		session->last_reservation_request_time = ntohl(*((uint32_t*)avp->data.s))-EPOCH_UNIX_TO_EPOCH_NTP;
	}
}

/**
 * update Granted Service Unit timers based on CCA, for onw we assume on one MSCC per session and only TIME based supported
 */
inline void update_gsu_response_timers(cdp_cc_acc_session_t* session, AAAMessage* msg) {
	AAA_AVP *avp;
	AAA_AVP_LIST mscc_avp_list;
	AAA_AVP_LIST y;
	AAA_AVP *z;

	avp = AAAFindMatchingAVP(msg, 0, AVP_Multiple_Services_Credit_Control, 0, 0);
        if (!avp) {
            LM_WARN("Trying to update GSU timers but there is no MSCC AVP in the CCA response\n");
            return;
        }
	mscc_avp_list = AAAUngroupAVPS(avp->data);
	AAA_AVP *mscc_avp = mscc_avp_list.head;

	while (mscc_avp != NULL ) {
		LM_DBG("MSCC AVP code is [%i] and data length is [%i]", mscc_avp->code, mscc_avp->data.len);
		switch (mscc_avp->code) {
			case AVP_Granted_Service_Unit:
				y = AAAUngroupAVPS(mscc_avp->data);
				z = y.head;
				while (z) {
					switch (z->code) {
					case AVP_CC_Time:
						session->reserved_units = get_4bytes(z->data.s);
						break;
					default:
						LM_DBG("ignoring AVP in GSU group with code:[%d]\n", z->code);
					}
					z = z->next;
				}
				break;
			case AVP_Validity_Time:
				session->reserved_units_validity_time = get_4bytes(mscc_avp->data.s);
				break;
			case AVP_Final_Unit_Indication:
				y = AAAUngroupAVPS(mscc_avp->data);
				z = y.head;
				while (z) {
					switch (z->code) {
						case AVP_Final_Unit_Action:
							session->fua = get_4bytes(z->data.s);
							break;
						default:
							LM_DBG("ignoring AVP in FUI group with code:[%d]\n", z->code);
					}
					z = z->next;
				}
				break;
		}
		mscc_avp = mscc_avp->next;
	}

	AAAFreeAVPList(&mscc_avp_list);
	AAAFreeAVPList(&y);
}


/**
 * stateful client state machine
 * \Note - should be called with a lock on the session and will unlock it - do not use it after!
 * @param cc_acc - AAACCAccSession which uses this state machine
 * @param ev   - Event
 * @param msg  - AAAMessage
 * @returns 0 if msg should be given to the upper layer 1 if not
 */
inline int cc_acc_client_stateful_sm_process(cdp_session_t* s, int event, AAAMessage* msg)
{
	cdp_cc_acc_session_t* x;
	int ret = 0;
	int rc;		//return code for responses
	int record_type;

	x = &(s->u.cc_acc);
	LM_DBG("cc_acc_client_stateful_sm_process: processing CC App in state [%d] and event [%d]\n", x->state, event);

	//first run session callbacks
	if (s->cb) (s->cb)(event, s);
	LM_DBG("finished callback of event %i\n", event);

	switch (x->state) {
		case ACC_CC_ST_IDLE:
			switch (event) {
				case ACC_CC_EV_SEND_REQ:		//were sending a message - CCR
					//assert this is an initial request. we can't move from IDLE with anything else
					record_type = get_accounting_record_type(msg);
					switch (record_type) {
						case 2 /*START RECORD*/:
							LM_DBG("sending CCR START record on session\n");
							s->application_id = msg->applicationId;
							s->u.cc_acc.state = ACC_CC_ST_PENDING_I;
							//update our reservation and its timers... if they exist in CCR
							update_gsu_request_timers(x, msg);
							break;
						default:
							LM_ERR("Sending CCR with no/incorrect accounting record type AVP. In state IDLE\n");
							break;
					}
					break;

				default:
					LM_ERR("Recevied unknown event [%d] in state [%d]\n", event, x->state);
					break;
			}
			break;
		case ACC_CC_ST_OPEN:
			switch (event) {
				case ACC_CC_EV_SEND_REQ:		//were sending a message - CCR
					//make sure it is either an update or a termination.
					record_type = get_accounting_record_type(msg);
					switch (record_type) {
						case 3 /*UPDATE RECORD*/:
							LM_DBG("sending CCR UPDATE record on session\n");
							s->u.cc_acc.state = ACC_CC_ST_PENDING_U;
							//update our reservation and its timers...
							update_gsu_request_timers(x, msg);
							break;
						case 4: /*TERMINATE RECORD*/
							LM_DBG("sending CCR TERMINATE record on session\n");
							s->u.cc_acc.state = ACC_CC_ST_PENDING_T;
							//update our reservation and its timers...
							update_gsu_request_timers(x, msg);
							break;
						default:
							LM_ERR("asked to send CCR with no/incorrect accounting record type AVP. In state IDLE\n");
							break;
					}
					break;
				case ACC_CC_EV_RSVN_WARNING:
					//nothing we can do here, we have sent callback, client needs to send CCR Update
					LM_DBG("Reservation close to expiring\n");
					break;
				default:
					LM_ERR("Received unknown event [%d] in state [%d]\n", event, x->state);
					break;
			}
			break;
		case ACC_CC_ST_PENDING_I:
			if (event == ACC_CC_EV_RECV_ANS && msg && !is_req(msg)) {
				rc = get_result_code(msg);
				if (rc >= 2000 && rc < 3000) {
					event = ACC_CC_EV_RECV_ANS_SUCCESS;
				} else {
					event = ACC_CC_EV_RECV_ANS_UNSUCCESS;
				}
			}
			switch (event) {
				case ACC_CC_EV_RECV_ANS_SUCCESS:
					x->state = ACC_CC_ST_OPEN;
					LM_DBG("received success response for CCR START\n");
					update_gsu_response_timers(x, msg);
					break;
				case ACC_CC_EV_RECV_ANS_UNSUCCESS:
					//TODO: grant/terminate service callbacks to callback clients
					LM_ERR("failed answer on CCR START\n");
					x->state = ACC_CC_ST_DISCON;
					break;
				default:
					LM_ERR("Received unknown event [%d] in state [%d]\n", event, x->state);
					break;
			}
			break;
		case ACC_CC_ST_PENDING_T:
			if (event == ACC_CC_EV_RECV_ANS && msg && !is_req(msg)) {
				rc = get_result_code(msg);
				if (rc >= 2000 && rc < 3000) {
					event = ACC_CC_EV_RECV_ANS_SUCCESS;
				} else {
					event = ACC_CC_EV_RECV_ANS_UNSUCCESS;
				}
			}
			switch (event) {
				case ACC_CC_EV_RECV_ANS_SUCCESS:
					x->state = ACC_CC_ST_DISCON;
//					update_gsu_response_timers(x, msg);
				case ACC_CC_EV_RECV_ANS_UNSUCCESS:
					x->state = ACC_CC_ST_DISCON;
				default:
					LM_DBG("Received event [%d] in state [%d] - cleaning up session regardless\n", event, x->state);
					//have to leave session alone because our client app still has to be given this msg
					x->discon_time = time(0);
//					if (msg) AAAFreeMessage(&msg);
//					cdp_session_cleanup(s, NULL);
//					s = 0;
			}
			break;
		case ACC_CC_ST_PENDING_U:
			if (event == ACC_CC_EV_RECV_ANS && msg && !is_req(msg)) {
				rc = get_result_code(msg);
				if (rc >= 2000 && rc < 3000) {
					event = ACC_CC_EV_RECV_ANS_SUCCESS;
				} else {
					event = ACC_CC_EV_RECV_ANS_UNSUCCESS;
				}
			}
			switch (event) {
				case ACC_CC_EV_RECV_ANS_SUCCESS:
					x->state = ACC_CC_ST_OPEN;
					LM_DBG("success CCA for UPDATE\n");
					update_gsu_response_timers(x, msg);
					break;
				case ACC_CC_EV_RECV_ANS_UNSUCCESS:
					//TODO: check whether we grant or terminate service to callback clients
					x->state = ACC_CC_ST_DISCON;
					LM_ERR("update failed... going back to IDLE/DISCON\n");
					break;
				default:
					LM_ERR("Received unknown event [%d] in state [%d]\n", event, x->state);
				break;
			}
			break;
		case ACC_CC_ST_DISCON:
			switch (event) {
				case ACC_CC_EV_SESSION_STALE:
					LM_DBG("stale session about to be cleared\n");
					cdp_session_cleanup(s, msg);
					s = 0;
					break;
				default:
					LM_ERR("Received unknown event [%d] in state [%d]\n", event, x->state);
					break;
			}
			break;
	}

	if (s) {
		AAASessionsUnlock(s->hash);
	}

	return ret;
}


