#include "ims_charging_mod.h"

#include <math.h>
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/sr_module.h"
#include "../../core/socket_info.h"
#include "../../core/timer.h"
#include "../../core/locking.h"
#include "../../modules/tm/tm_load.h"

#include "../../modules/ims_dialog/dlg_hash.h"
#include "../../modules/ims_dialog/dlg_load.h"


#include "../cdp/cdp_load.h"
#include "../../core/mod_fix.h"

#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"

#include "../../lib/ims/ims_getters.h"
#include "../../core/parser/sdp/sdp.h"

#include "diameter_ro.h"
#include "ims_ro.h"
#include "Ro_data.h"
#include "dialog.h"

#include "ccr.h"
#include "config.h"
#include "ro_session_hash.h"
#include "ro_avp.h"
#include "ro_db_handler.h"
#include "ims_charging_stats.h"

static pv_spec_t *custom_user_avp;		/*!< AVP for custom_user setting */

extern struct tm_binds tmb;
extern struct cdp_binds cdpb;
extern client_ro_cfg cfg;
extern ims_dlg_api_t dlgb;
extern cdp_avp_bind_t *cdp_avp;
extern str ro_forced_peer;
extern int ro_db_mode;
extern struct ims_charging_counters_h ims_charging_cnts_h;
extern int vendor_specific_id;
extern int vendor_specific_chargeinfo;

struct session_setup_data {
    struct ro_session *ro_session;
    cfg_action_t* action;
    unsigned int tindex;
    unsigned int tlabel;
};

struct dlg_binds* dlgb_p;
extern struct tm_binds tmb;

int interim_request_credits;

extern int voice_service_identifier;
extern int voice_rating_group;
extern int video_service_identifier;
extern int video_rating_group;

static int create_cca_return_code(int result);
static int create_cca_result_code(int result);
static int create_cca_fui_avps(int action, str* redirecturi);
static void resume_on_initial_ccr(int is_timeout, void *param, AAAMessage *cca, long elapsed_msecs);
static void resume_on_interim_ccr(int is_timeout, void *param, AAAMessage *cca, long elapsed_msecs);
static void resume_on_termination_ccr(int is_timeout, void *param, AAAMessage *cca, long elapsed_msecs);
static int get_mac_avp_value(struct sip_msg *msg, str *value);

void init_custom_user(pv_spec_t *custom_user_avp_p)
{
    custom_user_avp = custom_user_avp_p;
}

/*!
 * \brief Return the custom_user for a record route
 * \param req SIP message
 * \param custom_user to be returned
 * \return <0 for failure
 */
static int get_custom_user(struct sip_msg *req, str *custom_user) {
	pv_value_t pv_val;

	if (custom_user_avp) {
		if ((pv_get_spec_value(req, custom_user_avp, &pv_val) == 0)
				&& (pv_val.flags & PV_VAL_STR) && (pv_val.rs.len > 0)) {
			custom_user->s = pv_val.rs.s;
			custom_user->len = pv_val.rs.len;
			return 0;
		}
		LM_DBG("invalid AVP value, using default user from P-Asserted-Identity/From-Header\n");
	}

	return -1;
}

void credit_control_session_callback(int event, void* session) {
    switch (event) {
        case AUTH_EV_SESSION_DROP:
            LM_DBG("Received notification of CC App session drop - we must free the generic data\n");
            break;
        default:
            LM_DBG("Received unhandled event [%d] in credit control session callback from CDP\n", event);
    }
}

/**
 * Create and add an AVP to a list of AVPs.
 * @param list - the AVP list to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
int Ro_add_avp_list(AAA_AVP_LIST *list, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR("%s: Failed creating avp\n", func);
        return 0;
    }
    if (list->tail) {
        avp->prev = list->tail;
        avp->next = 0;
        list->tail->next = avp;
        list->tail = avp;
    } else {
        list->head = avp;
        list->tail = avp;
        avp->next = 0;
        avp->prev = 0;
    }

    return 1;
}

int Ro_add_cc_request(AAAMessage *msg, unsigned int cc_request_type, unsigned int cc_request_number) {
    char x[4];
    set_4bytes(x, cc_request_type);
    int success = Ro_add_avp(msg, x, 4, AVP_CC_Request_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    char y[4];
    set_4bytes(y, cc_request_number);

    return success && Ro_add_avp(msg, y, 4, AVP_CC_Request_Number, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

}

int Ro_add_event_timestamp(AAAMessage *msg, time_t now) {
    char x[4];
    str s = {x, 4};
    uint32_t ntime = htonl(now + EPOCH_UNIX_TO_EPOCH_NTP);
    memcpy(x, &ntime, sizeof (uint32_t));

    return Ro_add_avp(msg, s.s, s.len, AVP_Event_Timestamp, AAA_AVP_FLAG_NONE, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

}

int Ro_add_user_equipment_info(AAAMessage *msg, unsigned int type, str value) {
    AAA_AVP_LIST list;
    str group;
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, type);
    Ro_add_avp_list(&list, x, 4, AVP_User_Equipment_Info_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    Ro_add_avp_list(&list, value.s, value.len, AVP_User_Equipment_Info_Value, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    group = cdpb.AAAGroupAVPS(list);

    cdpb.AAAFreeAVPList(&list);

    return Ro_add_avp(msg, group.s, group.len, AVP_User_Equipment_Info, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

int Ro_add_termination_cause(AAAMessage *msg, unsigned int term_code) {
    char x[4];
    str s = {x, 4};
    uint32_t code = htonl(term_code);
    memcpy(x, &code, sizeof (uint32_t));

    return Ro_add_avp(msg, s.s, s.len, AVP_Termination_Cause, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
}

int Ro_add_vendor_specific_termination_cause(AAAMessage *msg, unsigned int term_code) {
    char x[4];
    str s = {x, 4};
    uint32_t code = htonl(term_code);
    memcpy(x, &code, sizeof (uint32_t));

    return Ro_add_avp(msg, s.s, s.len, VS_TERMCODE, AAA_AVP_FLAG_VENDOR_SPECIFIC, 10, AVP_DUPLICATE_DATA, __FUNCTION__);
}

int Ro_add_vendor_specific_termination_reason(AAAMessage *msg, str* reason) {
    return Ro_add_avp(msg, reason->s, reason->len, VS_TERMREASON, AAA_AVP_FLAG_VENDOR_SPECIFIC, 10, AVP_DUPLICATE_DATA, __FUNCTION__);
}



/* called only when building stop record AVPS */
int Ro_add_multiple_service_credit_Control_stop(AAAMessage *msg, int used_unit, int active_rating_group, int active_service_identifier) {
    char x[4];
    AAA_AVP_LIST used_list, mscc_list;
    str used_group;

    // Add Multiple-Services AVP Indicator
    set_4bytes(x, 1);
    Ro_add_avp(msg, x, 4, AVP_Multiple_Services_Indicator, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    //unsigned int service_id = 1000; //Removed these are now configurable config file params

    //unsigned int rating_group = 500; //Removed these are now configurable config file params

    used_list.head = 0;
    used_list.tail = 0;
    mscc_list.head = 0;
    mscc_list.tail = 0;

    /* if we must Used-Service-Unit */
    if (used_unit >= 0) {
        set_4bytes(x, used_unit);
        Ro_add_avp_list(&used_list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
        used_group = cdpb.AAAGroupAVPS(used_list);
        cdpb.AAAFreeAVPList(&used_list);
        Ro_add_avp_list(&mscc_list, used_group.s, used_group.len, AVP_Used_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
    }

    set_4bytes(x, active_service_identifier);
    Ro_add_avp_list(&mscc_list, x, 4, AVP_Service_Identifier, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    // Rating Group = -1 => omit Rating group
    if (active_rating_group >= 0) {
        set_4bytes(x, active_rating_group);
        Ro_add_avp_list(&mscc_list, x, 4, AVP_Rating_Group, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }

    used_group = cdpb.AAAGroupAVPS(mscc_list);
    cdpb.AAAFreeAVPList(&mscc_list);

    return Ro_add_avp(msg, used_group.s, used_group.len, AVP_Multiple_Services_Credit_Control, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

int Ro_add_multiple_service_credit_Control(AAAMessage *msg, unsigned int requested_unit, int used_unit, int active_rating_group, int active_service_identifier) {
    // Add Multiple-Services AVP Indicator
    char x[4];
    set_4bytes(x, 1);
    Ro_add_avp(msg, x, 4, AVP_Multiple_Services_Indicator, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    AAA_AVP_LIST list, used_list, mscc_list;
    str group, used_group;

    list.head = 0;
    list.tail = 0;
    used_list.head = 0;
    used_list.tail = 0;
    mscc_list.head = 0;
    mscc_list.tail = 0;

    set_4bytes(x, requested_unit);
    Ro_add_avp_list(&list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    group = cdpb.AAAGroupAVPS(list);
    cdpb.AAAFreeAVPList(&list);

    Ro_add_avp_list(&mscc_list, group.s, group.len, AVP_Requested_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);

    set_4bytes(x, active_service_identifier);
    Ro_add_avp_list(&mscc_list, x, 4, AVP_Service_Identifier, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    // Rating Group = -1 => omit Rating group
    if (active_rating_group >= 0) {
      set_4bytes(x, active_rating_group);
      Ro_add_avp_list(&mscc_list, x, 4, AVP_Rating_Group, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }

    /* if we must Used-Service-Unit */
    if (used_unit >= 0) {
        set_4bytes(x, used_unit);
        Ro_add_avp_list(&used_list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
        used_group = cdpb.AAAGroupAVPS(used_list);
        cdpb.AAAFreeAVPList(&used_list);
        Ro_add_avp_list(&mscc_list, used_group.s, used_group.len, AVP_Used_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
    }

    group = cdpb.AAAGroupAVPS(mscc_list);
    cdpb.AAAFreeAVPList(&mscc_list);

    return Ro_add_avp(msg, group.s, group.len, AVP_Multiple_Services_Credit_Control, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

int Ro_add_subscription_id(AAAMessage *msg, unsigned int type, str *subscription_id)//, struct sip_msg* sip_msg)
{
    AAA_AVP_LIST list;
    str group;
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, type);
    Ro_add_avp_list(&list, x, 4, AVP_Subscription_Id_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    Ro_add_avp_list(&list, subscription_id->s, subscription_id->len, AVP_Subscription_Id_Data, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    group = cdpb.AAAGroupAVPS(list);

    cdpb.AAAFreeAVPList(&list);

    return Ro_add_avp(msg, group.s, group.len, AVP_Subscription_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

/**
 * Creates and adds a Vendor-Specifig-Application-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param vendor_id - the value of the vendor_id,
 * @param auth_id - the authorization application id
 * @param acct_id - the accounting application id
 * @returns 1 on success or 0 on error
 */
int Ro_add_vendor_specific_appid(AAAMessage *msg, unsigned int vendor_id, unsigned int auth_id, unsigned int acct_id) {
    AAA_AVP_LIST list;
    str group;
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, vendor_id);
    Ro_add_avp_list(&list, x, 4, AVP_Vendor_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    if (auth_id) {
        set_4bytes(x, auth_id);
        Ro_add_avp_list(&list, x, 4, AVP_Auth_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }
    if (acct_id) {
        set_4bytes(x, acct_id);
        Ro_add_avp_list(&list, x, 4, AVP_Acct_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }

    group = cdpb.AAAGroupAVPS(list);

    cdpb.AAAFreeAVPList(&list);

    return Ro_add_avp(msg, group.s, group.len, AVP_Vendor_Specific_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

int get_sip_header_info(struct sip_msg * req,
        struct sip_msg * reply,
        int32_t * acc_record_type,
        str * sip_method,
        str * event, uint32_t * expires,
        str * callid, str * asserted_id_uri, str * to_uri) {

    sip_method->s = req->first_line.u.request.method.s;
    sip_method->len = req->first_line.u.request.method.len;

    if (strncmp(sip_method->s, "INVITE", 6) == 0)
        *acc_record_type = AAA_ACCT_START;
    else if (strncmp(sip_method->s, "BYE", 3) == 0)
        *acc_record_type = AAA_ACCT_STOP;
    else
        *acc_record_type = AAA_ACCT_EVENT;

    *event = cscf_get_event(req);
    *expires = cscf_get_expires_hdr(req, 0);
    *callid = cscf_get_call_id(req, NULL);

    if (get_custom_user(req, asserted_id_uri) == -1) {
	    if ((*asserted_id_uri = cscf_get_asserted_identity(req, 0)).len == 0) {
		LM_DBG("No P-Asserted-Identity hdr found. Using From hdr");

		if (!cscf_get_from_uri(req, asserted_id_uri)) {
		    LM_ERR("Error assigning P-Asserted-Identity using From hdr");
		    goto error;
		}
	    }
    }

    *to_uri = req->first_line.u.request.uri;

    LM_DBG("retrieved sip info : sip_method %.*s acc_record_type %i, event %.*s expires %u "
            "call_id %.*s from_uri %.*s to_uri %.*s\n",
            sip_method->len, sip_method->s, *acc_record_type, event->len, event->s, *expires,
            callid->len, callid->s, asserted_id_uri->len, asserted_id_uri->s, to_uri->len, to_uri->s);

    return 1;
error:
    return 0;
}

int get_ims_charging_info(struct sip_msg *req, struct sip_msg * reply, str * icid, str * orig_ioi, str * term_ioi) {

    LM_DBG("get ims charging info\n");
    if (req)
        cscf_get_p_charging_vector(req, icid, orig_ioi, term_ioi);
    if (reply)
        cscf_get_p_charging_vector(reply, icid, orig_ioi, term_ioi);

    return 1;
}

int get_timestamps(struct sip_msg * req, struct sip_msg * reply, time_t * req_timestamp, time_t * reply_timestamp) {

    if (reply)
        *reply_timestamp = time(NULL);
    if (req)
        *req_timestamp = time(NULL);
    return 1;
}

/*
 * creates the ro session for a session establishment
 *
 */

Ro_CCR_t * dlg_create_ro_session(struct sip_msg * req, struct sip_msg * reply, AAASession ** authp, int dir, str asserted_identity,
        str called_asserted_identity, str subscription_id, int subscription_id_type, str* incoming_trunk_id, str *outgoing_trunk_id, str* pani) {

    Ro_CCR_t * ro_ccr_data = 0;
    AAASession * auth = NULL;
    str user_name/* ={0,0}*/, sip_method = {0, 0}, event = {0, 0};
    uint32_t expires = 0;
    str callid = {0, 0}, to_uri = {0, 0}, from_uri = {0, 0},
    icid = {0, 0}, orig_ioi = {0, 0}, term_ioi = {0, 0};

    event_type_t * event_type = 0;
    ims_information_t * ims_info = 0;
    time_stamps_t * time_stamps = 0;
    time_t req_timestamp = 0, reply_timestamp = 0;
    int32_t acc_record_type;
    subscription_id_t subscr;

    *authp = 0;

    if (!get_sip_header_info(req, reply, &acc_record_type, &sip_method, &event, &expires, &callid, &from_uri, &to_uri))
        goto error;
    user_name.s = subscription_id.s;
    user_name.len = subscription_id.len;

    /*	if(!get_ims_charging_info(req, reply, &icid, &orig_ioi, &term_ioi))
                    goto error;
     */
    LM_DBG("retrieved ims charging info icid:[%.*s] orig_ioi:[%.*s] term_ioi:[%.*s]\n",
            icid.len, icid.s, orig_ioi.len, orig_ioi.s, term_ioi.len, term_ioi.s);

    if (!get_timestamps(req, reply, &req_timestamp, &reply_timestamp))
        goto error;

    if (!(event_type = new_event_type(&sip_method, &event, &expires)))
        goto error;

    if (!(time_stamps = new_time_stamps(&req_timestamp, NULL, &reply_timestamp, NULL)))
        goto error;

    if (!(ims_info = new_ims_information(event_type, time_stamps, &callid, &callid, &asserted_identity, &called_asserted_identity, &icid,
            &orig_ioi, &term_ioi, dir, incoming_trunk_id, outgoing_trunk_id, pani)))
        goto error;
    event_type = 0;
    time_stamps = 0;


    subscr.id.s = subscription_id.s;
    subscr.id.len = subscription_id.len;
    subscr.type = subscription_id_type;

    ro_ccr_data = new_Ro_CCR(acc_record_type, &user_name, ims_info, &subscr);
    if (!ro_ccr_data) {
        LM_ERR("dlg_create_ro_session: no memory left for generic\n");
        goto out_of_memory;
    }
    ims_info = 0;

    if (strncmp(req->first_line.u.request.method.s, "INVITE", 6) == 0) {
        //create CDP CC Accounting session
        auth = cdpb.AAACreateCCAccSession(credit_control_session_callback, 1/*is_session*/, NULL); //must unlock session hash when done
        LM_DBG("Created Ro Session with id Session ID [%.*s]\n", auth->id.len, auth->id.s);
        //save_session = auth->id;

    }
    /*if (strncmp(req->first_line.u.request.method.s, "BYE", 3) == 0) {
        auth = cdp_avp->cdp->AAAGetAuthSession(save_session);
    }*/


    if (!auth) {
        LM_ERR("unable to create the Ro Session\n");
        goto error;
    }

    *authp = auth;
    return ro_ccr_data;

out_of_memory:
    error :
            time_stamps_free(time_stamps);
    event_type_free(event_type);
    ims_information_free(ims_info);
    Ro_free_CCR(ro_ccr_data);

    return NULL;
}

int sip_create_ro_ccr_data(struct sip_msg * msg, int dir, Ro_CCR_t ** ro_ccr_data, AAASession ** auth, str asserted_identity, str called_asserted_identity,
        str subscription_id, int subscription_id_type, str* incoming_trunk_id, str* outgoing_trunk_id, str* pani) {

    if (msg->first_line.type == SIP_REQUEST) {
        /*end of session*/
        if (strncmp(msg->first_line.u.request.method.s, "INVITE", 6) == 0) {
            if (!(*ro_ccr_data = dlg_create_ro_session(msg, NULL, auth, dir, asserted_identity, called_asserted_identity, subscription_id,
                    subscription_id_type, incoming_trunk_id, outgoing_trunk_id, pani)))
                goto error;
        }
    } else {
        goto error; //We only support Request (INVITE) messages on this interface
    }

    return 1;
error:
    return 0;
}

/* must be called with lock on ro_session */
void send_ccr_interim(struct ro_session* ro_session, unsigned int used, unsigned int reserve) {
    AAASession * auth = 0;

    AAAMessage * ccr = 0;
    Ro_CCR_t *ro_ccr_data = 0;
    ims_information_t *ims_info = 0;
    int32_t acc_record_type;
    subscription_id_t subscr;
    time_stamps_t *time_stamps;
    struct interim_ccr *i_req = shm_malloc(sizeof (struct interim_ccr));
    int ret = 0;
    event_type_t *event_type;

    memset(i_req, 0, sizeof (sizeof (struct interim_ccr)));
    i_req->ro_session = ro_session;

    str sip_method = str_init("dummy");
    str sip_event = str_init("dummy");

    str user_name = {0, 0};

    time_t req_timestamp;

    event_type = new_event_type(&sip_method, &sip_event, 0);

    LM_DBG("Sending interim CCR request for (usage:new) [%i:%i] seconds for user [%.*s] using session id [%.*s] active rating group [%d] active service identifier [%d] incoming_trunk_id [%.*s] outgoing_trunk_id [%.*s]\n",
            used,
            reserve,
            ro_session->asserted_identity.len, ro_session->asserted_identity.s,
            ro_session->ro_session_id.len, ro_session->ro_session_id.s,
            ro_session->rating_group, ro_session->service_identifier,
            ro_session->incoming_trunk_id.len, ro_session->incoming_trunk_id.s,
            ro_session->outgoing_trunk_id.len, ro_session->outgoing_trunk_id.s);

    req_timestamp = time(0);

    if (!(time_stamps = new_time_stamps(&req_timestamp, NULL, NULL, NULL)))
        goto error;

    if (!(ims_info = new_ims_information(event_type, time_stamps, &ro_session->callid, &ro_session->callid, &ro_session->asserted_identity,
            &ro_session->called_asserted_identity, 0, 0, 0, ro_session->direction, &ro_session->incoming_trunk_id, &ro_session->outgoing_trunk_id, &ro_session->pani)))
        goto error;

    LM_DBG("Created IMS information\n");

    event_type = 0;

    if (ro_session->direction == RO_ORIG_DIRECTION) {
        subscr.id = ro_session->asserted_identity;


    } else if (ro_session->direction == RO_TERM_DIRECTION) {
        subscr.id = ro_session->called_asserted_identity;
    } else {
        LM_CRIT("don't know what to do in unknown mode - should we even get here\n");
        goto error;
    }

    //getting subscription id type
    if (strncasecmp(subscr.id.s, "tel:", 4) == 0) {
        subscr.type = Subscription_Type_MSISDN;
        // Strip "tel:":
        subscr.id.s += 4;
        subscr.id.len -= 4;
    } else {
        subscr.type = Subscription_Type_IMPU; //default is END_USER_SIP_URI
    }

    user_name.s = subscr.id.s;
    user_name.len = subscr.id.len;

    acc_record_type = AAA_ACCT_INTERIM;

    ro_ccr_data = new_Ro_CCR(acc_record_type, &user_name, ims_info, &subscr);
    if (!ro_ccr_data) {
        LM_ERR("no memory left for generic\n");
        goto error;
    }
    ims_info = NULL;

    auth = cdpb.AAAGetCCAccSession(ro_session->ro_session_id);
    if (!auth) {
        LM_DBG("Diameter Auth Session has timed out.... creating a new one.\n");
        /* lets try and recreate this session */
        //TODO: make a CC App session auth = cdpb.AAASession(ro_session->auth_appid, ro_session->auth_session_type, ro_session->ro_session_id); //TODO: would like this session to last longer (see session timeout in cdp
        //BUG("Oh shit, session timed out and I don't know how to create a new one.");

        auth = cdpb.AAAMakeSession(ro_session->auth_appid, ro_session->auth_session_type, ro_session->ro_session_id); //TODO: would like this session to last longer (see session timeout in cdp
        if (!auth)
            goto error;
    }

    //don't send INTERIM record if session is not in OPEN state (it could already be waiting for a previous response, etc)
    if (auth->u.cc_acc.state != ACC_CC_ST_OPEN) {
        LM_WARN("ignoring interim update on CC session not in correct state, currently in state [%d]\n", auth->u.cc_acc.state);
        goto error;
    }

    if (!(ccr = Ro_new_ccr(auth, ro_ccr_data)))
        goto error;

    if (!Ro_add_vendor_specific_appid(ccr, IMS_vendor_id_3GPP, IMS_Ro, 0/*acct id*/)) {
        LM_ERR("Problem adding Vendor specific ID\n");
    }
    ro_session->hop_by_hop += 1;
    if (!Ro_add_cc_request(ccr, RO_CC_INTERIM, ro_session->hop_by_hop)) {
        LM_ERR("Problem adding CC-Request data\n");
    }
    if (!Ro_add_event_timestamp(ccr, time(NULL))) {
        LM_ERR("Problem adding Event-Timestamp data\n");
    }

    if (!Ro_add_user_equipment_info(ccr, AVP_EPC_User_Equipment_Info_Type_MAC, ro_session->mac)) {
        LM_ERR("Problem adding User-Equipment data\n");
    }

    if (!Ro_add_subscription_id(ccr, subscr.type, &(subscr.id))) {
        LM_ERR("Problem adding Subscription ID data\n");
    }

    if (!Ro_add_multiple_service_credit_Control(ccr, interim_request_credits/*INTERIM_CREDIT_REQ_AMOUNT*/, used, ro_session->rating_group, ro_session->service_identifier)) {
        LM_ERR("Problem adding Multiple Service Credit Control data\n");
    }

    LM_DBG("Sending CCR Diameter message.\n");
    ro_session->last_event_timestamp_backup = ro_session->last_event_timestamp;
    ro_session->last_event_timestamp = get_current_time_micro(); /*this is to make sure that if we get a term request now that we don't double bill for this time we are about to bill for in the interim */

    cdpb.AAASessionsUnlock(auth->hash);

    if (ro_forced_peer.len > 0) {
        ret = cdpb.AAASendMessageToPeer(ccr, &ro_forced_peer, resume_on_interim_ccr, (void *) i_req);
    } else {
        ret = cdpb.AAASendMessage(ccr, resume_on_interim_ccr, (void *) i_req);
    }

    if (ret != 1) {
        ccr = 0;	// If an error is returned from cdp AAASendMessage, the message has been freed there
        goto error;
    }
    //    cdpb.AAASessionsUnlock(auth->hash);

    Ro_free_CCR(ro_ccr_data);

    counter_inc(ims_charging_cnts_h.interim_ccrs);
    return;
error:
    LM_ERR("error trying to reserve interim credit\n");

    if (ro_ccr_data)
        Ro_free_CCR(ro_ccr_data);

    if (ccr)
        cdpb.AAAFreeMessage(&ccr);

    if (auth) {
        cdpb.AAASessionsUnlock(auth->hash);
        cdpb.AAADropCCAccSession(auth);
    }

    shm_free(i_req);
    //
    // since callback function will be never called because of the error, we need to release the lock on the session
    // to it can be reused later.
    //
	unref_ro_session(ro_session, 1, 1);

    return;
}

static void resume_on_interim_ccr(int is_timeout, void *param, AAAMessage *cca, long elapsed_msecs) {
    struct interim_ccr *i_req = (struct interim_ccr *) param;
    Ro_CCA_t * ro_cca_data = NULL;
    int error_or_timeout = 0;

    if (is_timeout) {
        counter_inc(ims_charging_cnts_h.ccr_timeouts);
        LM_ERR("Transaction timeout - did not get CCA\n");
        error_or_timeout = 1;
        goto error;
    }

    counter_add(ims_charging_cnts_h.ccr_response_time, elapsed_msecs);
    counter_inc(ims_charging_cnts_h.ccr_replies_received);

    if (!i_req) {
        LM_ERR("This is so wrong: ro session is NULL\n");
        error_or_timeout = 1;
        goto error;
    }

    if (cca == NULL) {
        LM_ERR("Error reserving credit for CCA.\n");
        error_or_timeout = 1;
        goto error;
    }

    ro_cca_data = Ro_parse_CCA_avps(cca);

    if (ro_cca_data == NULL) {
        LM_ERR("Could not parse CCA message response.\n");
        error_or_timeout = 1;
        goto error;
    }

    if (ro_cca_data->resultcode != 2001) {
        LM_ERR("Got bad CCA result code [%d] - reservation failed", ro_cca_data->resultcode);
        error_or_timeout = 1;
        goto error;
    } else {
        LM_DBG("Valid CCA response with time chunk of [%i] and validity [%i].\n", ro_cca_data->mscc->granted_service_unit->cc_time, ro_cca_data->mscc->validity_time);
    }

    i_req->new_credit = ro_cca_data->mscc->granted_service_unit->cc_time;
    i_req->credit_valid_for = ro_cca_data->mscc->validity_time;
    i_req->is_final_allocation = 0;

    if (ro_cca_data->mscc->final_unit_action && (ro_cca_data->mscc->final_unit_action->action == 0))
        i_req->is_final_allocation = 1;

    Ro_free_CCA(ro_cca_data);
    cdpb.AAAFreeMessage(&cca);

    counter_inc(ims_charging_cnts_h.successful_interim_ccrs);
    goto success;

error:
    counter_inc(ims_charging_cnts_h.failed_interim_ccr);
    if (ro_cca_data)
        Ro_free_CCA(ro_cca_data);

    if (!is_timeout && cca) {
        cdpb.AAAFreeMessage(&cca);
    }

    if (i_req) {
        i_req->credit_valid_for = 0;
        i_req->new_credit = 0;
    }

success:
    resume_ro_session_ontimeout(i_req, error_or_timeout);
}

long get_current_time_micro() {
    struct timeval tv;

    gettimeofday(&tv, 0);
    return tv.tv_sec*1000000 + tv.tv_usec;
}

void send_ccr_stop_with_param(struct ro_session *ro_session, unsigned int code, str* reason) {
    AAASession * auth = 0;
    Ro_CCR_t * ro_ccr_data = 0;
    AAAMessage * ccr = 0;
    ims_information_t *ims_info = 0;
    int32_t acc_record_type;
    subscription_id_t subscr;
    time_stamps_t *time_stamps;
    long used = 0;
    str user_name = {0, 0};
    int ret = 0;
    time_t stop_time;
    time_t actual_time_micros;
    int actual_time_seconds;

    stop_time = get_current_time_micro();

    if (ro_session->start_time == 0)
        actual_time_micros = 0;
    else
        actual_time_micros = stop_time - ro_session->start_time;

    actual_time_seconds = (actual_time_micros + (1000000 - 1)) / (float) 1000000;

    if (ro_session->event_type != pending) {
        used = rint((stop_time - ro_session->last_event_timestamp) / (float) 1000000);
        LM_DBG("Final used number of seconds for session is %ld\n", used);
    }

    LM_DBG("Call started at %ld and ended at %ld and lasted %d seconds and so far we have billed for %ld seconds\n", ro_session->start_time, stop_time,
            actual_time_seconds, ro_session->billed + used);
    if (ro_session->billed + used < actual_time_seconds) {
        LM_DBG("Making adjustment by adding %ld seconds\n", actual_time_seconds - (ro_session->billed + used));
        used += actual_time_seconds - (ro_session->billed + used);
    }

    counter_add(ims_charging_cnts_h.billed_secs, (int)used);

    event_type_t *event_type;

    str sip_method = str_init("dummy");
    str sip_event = str_init("dummy");

    time_t req_timestamp;

    event_type = new_event_type(&sip_method, &sip_event, 0);

    LM_DBG("Sending stop CCR request for (usage) [%i] seconds for user [%.*s] using session id [%.*s] active rating group [%d] active service identifier [%d] incoming_trunk_id [%.*s] outgoing_trunk_id [%.*s] pani [%.*s]\n",
            (int)used,
            ro_session->asserted_identity.len, ro_session->asserted_identity.s,
            ro_session->ro_session_id.len, ro_session->ro_session_id.s,
            ro_session->rating_group, ro_session->service_identifier,
            ro_session->incoming_trunk_id.len, ro_session->incoming_trunk_id.s,
            ro_session->outgoing_trunk_id.len, ro_session->outgoing_trunk_id.s,
            ro_session->pani.len, ro_session->pani.s);

    req_timestamp = get_current_time_micro();

    if (!(time_stamps = new_time_stamps(&req_timestamp, NULL, NULL, NULL)))
        goto error0;

    if (!(ims_info = new_ims_information(event_type, time_stamps, &ro_session->callid, &ro_session->callid, &ro_session->asserted_identity,
            &ro_session->called_asserted_identity, 0, 0, 0, ro_session->direction, &ro_session->incoming_trunk_id, &ro_session->outgoing_trunk_id, &ro_session->pani)))
        goto error0;

    event_type = 0;

    if (ro_session->direction == RO_ORIG_DIRECTION) {
        subscr.id = ro_session->asserted_identity;


    } else if (ro_session->direction == RO_TERM_DIRECTION) {
        subscr.id = ro_session->called_asserted_identity;
    } else {
        LM_CRIT("don't know what to do in unknown mode - should we even get here\n");
        goto error0;
    }

    //getting subscription id type
    if (strncasecmp(subscr.id.s, "tel:", 4) == 0) {
        subscr.type = Subscription_Type_MSISDN;
    } else {
        subscr.type = Subscription_Type_IMPU; //default is END_USER_SIP_URI
    }

    user_name.s = subscr.id.s;
    user_name.len = subscr.id.len;


    acc_record_type = AAA_ACCT_STOP;

    ro_ccr_data = new_Ro_CCR(acc_record_type, &user_name, ims_info, &subscr);
    if (!ro_ccr_data) {
        LM_ERR("send_ccr_stop: no memory left for generic\n");
        goto error0;
    }
    ims_info = 0;

    LM_DBG("Created Ro data\n");

    auth = cdpb.AAAGetCCAccSession(ro_session->ro_session_id);

    if (!auth) {
        LM_DBG("Diameter Auth Session has timed out.... creating a new one.\n");
        /* lets try and recreate this session */
        auth = cdpb.AAAMakeSession(ro_session->auth_appid, ro_session->auth_session_type, ro_session->ro_session_id); //TODO: would like this session to last longer (see session timeout in cdp
        if (!auth)
            goto error1;
    }


    if (!(ccr = Ro_new_ccr(auth, ro_ccr_data)))
        goto error1;

    LM_DBG("Created new CCR\n");

    if (!Ro_add_vendor_specific_appid(ccr, IMS_vendor_id_3GPP, IMS_Ro, 0)) {
        LM_ERR("Problem adding Vendor specific ID\n");
    }

    ro_session->hop_by_hop += 1;
    if (!Ro_add_cc_request(ccr, RO_CC_STOP, ro_session->hop_by_hop)) {
        LM_ERR("Problem adding CC-Request data\n");
    }

    if (!Ro_add_event_timestamp(ccr, time(NULL))) {
        LM_ERR("Problem adding Event-Timestamp data\n");
    }

    if (!Ro_add_user_equipment_info(ccr, AVP_EPC_User_Equipment_Info_Type_MAC, ro_session->mac)) {
        LM_ERR("Problem adding User-Equipment data\n");
    }

    if (!Ro_add_subscription_id(ccr, subscr.type, &subscr.id)) {
        LM_ERR("Problem adding Subscription ID data\n");
    }

    if (!Ro_add_multiple_service_credit_Control_stop(ccr, used, ro_session->rating_group, ro_session->service_identifier)) {
        LM_ERR("Problem adding Multiple Service Credit Control data\n");
    }

    if (!Ro_add_termination_cause(ccr, TERM_CAUSE_LOGOUT)) {
        LM_ERR("problem add Termination cause AVP to STOP record.\n");
    }

    if (vendor_specific_chargeinfo) {
        if (!Ro_add_vendor_specific_termination_cause(ccr, code)) {
            LM_ERR("problem add Termination cause AVP to STOP record.\n");
        }

        if (!Ro_add_vendor_specific_termination_reason(ccr, reason)) {
            LM_ERR("problem add Termination cause AVP to STOP record.\n");
        }
    }

    cdpb.AAASessionsUnlock(auth->hash);

    if (ro_forced_peer.len > 0) {
        ret = cdpb.AAASendMessageToPeer(ccr, &ro_forced_peer, resume_on_termination_ccr, NULL);
    } else {
        ret = cdpb.AAASendMessage(ccr, resume_on_termination_ccr, NULL);
    }

    if (ret != 1) {
        goto error1;
    }

    Ro_free_CCR(ro_ccr_data);

    counter_inc(ims_charging_cnts_h.final_ccrs);
//    counter_add(ims_charging_cnts_h.active_ro_sessions, -1);
    return;

error1:
    LM_ERR("error on Ro STOP record\n");
    Ro_free_CCR(ro_ccr_data);

    if (auth) {
        cdpb.AAASessionsUnlock(auth->hash);
        cdpb.AAADropCCAccSession(auth);
    }

error0:
    return;

}

static void resume_on_termination_ccr(int is_timeout, void *param, AAAMessage *cca, long elapsed_msecs) {
    Ro_CCA_t *ro_cca_data = NULL;

    if (is_timeout) {
        counter_inc(ims_charging_cnts_h.ccr_timeouts);
        LM_ERR("Transaction timeout - did not get CCA\n");
        goto error;
    }

    counter_inc(ims_charging_cnts_h.ccr_replies_received);
    counter_add(ims_charging_cnts_h.ccr_response_time, elapsed_msecs);

    if (!cca) {
        LM_ERR("Error in termination CCR.\n");
        counter_inc(ims_charging_cnts_h.failed_final_ccrs);
        return;
    }

    ro_cca_data = Ro_parse_CCA_avps(cca);

    if (ro_cca_data == NULL) {
        LM_DBG("Could not parse CCA message response.\n");
        counter_inc(ims_charging_cnts_h.failed_final_ccrs);
        return;
    }

    if (ro_cca_data->resultcode != 2001) {
        LM_ERR("Got bad CCA result code for STOP record - [%d]\n", ro_cca_data->resultcode);
        goto error;
    } else {
        LM_DBG("Valid CCA response for STOP record\n");
    }

    counter_inc(ims_charging_cnts_h.successful_final_ccrs);
    Ro_free_CCA(ro_cca_data);
    if (!is_timeout && cca) {
        cdpb.AAAFreeMessage(&cca);
    }
    return;

error:
    counter_inc(ims_charging_cnts_h.failed_final_ccrs);
    Ro_free_CCA(ro_cca_data);
    if (!is_timeout && cca) {
        cdpb.AAAFreeMessage(&cca);
    }
}

/**
 * Send a CCR to the OCS based on the SIP message (INVITE ONLY)
 * @param msg - SIP message
 * @param direction - orig|term
 * @param reservation_units - units to try to reserve
 * @param reservation_units - config route to call when receiving a CCA
 * @param tindex - transaction index
 * @param tindex - transaction label
 *
 * @returns #CSCF_RETURN_BREAK if OK, #CSCF_RETURN_ERROR on error
 */
int Ro_Send_CCR(struct sip_msg *msg, struct dlg_cell *dlg, int dir, int reservation_units, str* incoming_trunk_id, str* outgoing_trunk_id,
        str* pani, cfg_action_t* action, unsigned int tindex, unsigned int tlabel) {
    str session_id = {0, 0},
    called_asserted_identity = {0, 0},
    subscription_id = {0, 0},
    asserted_identity = {0, 0};
    int subscription_id_type = AVP_EPC_Subscription_Id_Type_End_User_SIP_URI;
    AAASession* cc_acc_session = NULL;
    Ro_CCR_t * ro_ccr_data = 0;
    AAAMessage * ccr = 0;
    struct ro_session *new_session = 0;
    struct session_setup_data *ssd;
    int ret = 0;
    struct hdr_field *h = 0;
    char *p;

    int cc_event_number = 0; //According to IOT tests this should start at 0
    int cc_event_type = RO_CC_START;
    int free_called_asserted_identity = 0;

    sdp_session_cell_t* msg_sdp_session;
    sdp_stream_cell_t* msg_sdp_stream;

    int active_service_identifier;
    int active_rating_group;

    int sdp_stream_num = 0;

    LM_DBG("Sending initial CCR request (%c) for reservation_units [%d] incoming_trunk_id [%.*s] outgoing_trunk_id [%.*s]\n",
	dir==RO_ORIG_DIRECTION?'O':'T',
			reservation_units,
            incoming_trunk_id->len, incoming_trunk_id->s,
            outgoing_trunk_id->len, outgoing_trunk_id->s);

    ssd = shm_malloc(sizeof (struct session_setup_data)); // lookup structure used to load session info from cdp callback on CCA
    if (!ssd) {
        LM_ERR("no more shm mem\n");
        goto error;
    }

    //getting asserted identity
    if (get_custom_user(msg, &asserted_identity) == -1) {
      if ((asserted_identity = cscf_get_asserted_identity(msg, 0)).len == 0) {
          LM_DBG("No P-Asserted-Identity hdr found. Using From hdr for asserted_identity");
          asserted_identity = dlg->from_uri;
          if (asserted_identity.len > 0 && asserted_identity.s) {
              p=(char*)memchr(asserted_identity.s, ';',asserted_identity.len);
              if (p)
                  asserted_identity.len = (p-asserted_identity.s);
          }
      }
    }

    //getting called asserted identity
    if ((called_asserted_identity = cscf_get_public_identity_from_called_party_id(msg, &h)).len == 0) {
        LM_DBG("No P-Called-Identity hdr found. Using request URI for called_asserted_identity");
        called_asserted_identity = cscf_get_public_identity_from_requri(msg);
        free_called_asserted_identity = 1;
    }

    if (dir == RO_ORIG_DIRECTION) {
        subscription_id.s = asserted_identity.s;
        subscription_id.len = asserted_identity.len;

    } else if (dir == RO_TERM_DIRECTION) {
        subscription_id.s = called_asserted_identity.s;
        subscription_id.len = called_asserted_identity.len;
    } else {
        LM_CRIT("don't know what to do in unknown mode - should we even get here\n");
        goto error;
    }

    //getting subscription id type
    if (strncasecmp(subscription_id.s, "tel:", 4) == 0) {
        subscription_id_type = Subscription_Type_MSISDN;
    } else {
        subscription_id_type = Subscription_Type_IMPU; //default is END_USER_SIP_URI
    }

    str mac = {0, 0};
    if (get_mac_avp_value(msg, &mac) != 0)
        LM_DBG(RO_MAC_AVP_NAME" was not set. Using default.");

    //by default we use voice service id and rate group
    //then we check SDP - if we find video then we use video service id and rate group
    LM_DBG("Setting default SID to %d and RG to %d for voice",
            voice_service_identifier, voice_rating_group);
    active_service_identifier = voice_service_identifier;
    active_rating_group = voice_rating_group;

    //check SDP - if there is video then set default to video, if not set it to audio
    if (parse_sdp(msg) < 0) {
        LM_ERR("Unable to parse req SDP\n");
        goto error;
    }

    msg_sdp_session = get_sdp_session(msg, 0);
    if (!msg_sdp_session) {
        LM_ERR("Missing SDP session information from rpl\n");
    } else {
        for (;;) {
            msg_sdp_stream = get_sdp_stream(msg, 0, sdp_stream_num);
            if (!msg_sdp_stream) {
                //LM_ERR("Missing SDP stream information\n");
                break;
            }

            int intportA = atoi(msg_sdp_stream->port.s);
            if (intportA != 0 && strncasecmp(msg_sdp_stream->media.s, "video", 5) == 0) {
                LM_DBG("This SDP has a video component and src ports not equal to 0 - so we set default SID to %d and RG to %d for video",
                        video_service_identifier, video_rating_group);
                active_service_identifier = video_service_identifier;
                active_rating_group = video_rating_group;
                break;
            }

            sdp_stream_num++;
        }
    }

    free_sdp((sdp_info_t**) (void*) &msg->body);

    //create a session object without auth and diameter session id - we will add this later.
    new_session = build_new_ro_session(dir, 0, 0, &session_id, &dlg->callid,
            &asserted_identity, &called_asserted_identity, &mac, dlg->h_entry, dlg->h_id,
            reservation_units, 0, active_rating_group, active_service_identifier, incoming_trunk_id, outgoing_trunk_id, pani);

    if (!new_session) {
        LM_ERR("Couldn't create new Ro Session - this is BAD!\n");
        goto error;
    }

    ssd->action = action;
    ssd->tindex = tindex;
    ssd->tlabel = tlabel;
    ssd->ro_session = new_session;

    if (!sip_create_ro_ccr_data(msg, dir, &ro_ccr_data, &cc_acc_session, asserted_identity, called_asserted_identity, subscription_id, subscription_id_type, incoming_trunk_id, outgoing_trunk_id, pani))
        goto error;

    if (!ro_ccr_data)
        goto error;

    if (!cc_acc_session)
        goto error;

    if (!(ccr = Ro_new_ccr(cc_acc_session, ro_ccr_data)))
        goto error;

    if (!Ro_add_vendor_specific_appid(ccr, IMS_vendor_id_3GPP, IMS_Ro, 0)) {
        LM_ERR("Problem adding Vendor specific ID\n");
        goto error;
    }

    if (!Ro_add_cc_request(ccr, cc_event_type, cc_event_number)) {
        LM_ERR("Problem adding CC-Request data\n");
        goto error;
    }

    if (!Ro_add_event_timestamp(ccr, time(NULL))) {
        LM_ERR("Problem adding Event-Timestamp data\n");
        goto error;
    }

    if (!Ro_add_user_equipment_info(ccr, AVP_EPC_User_Equipment_Info_Type_MAC, mac)) {
        LM_ERR("Problem adding User-Equipment data\n");
        goto error;
    }

    if (!Ro_add_subscription_id(ccr, subscription_id_type, &subscription_id)) {
        LM_ERR("Problem adding Subscription ID data\n");
        goto error;
    }
    if (!Ro_add_multiple_service_credit_Control(ccr, reservation_units, -1, active_rating_group, active_service_identifier)) {
        LM_ERR("Problem adding Multiple Service Credit Control data\n");
        goto error;
    }

    /* before we send, update our session object with CC App session ID and data */
    new_session->auth_appid = cc_acc_session->application_id;
    new_session->auth_session_type = cc_acc_session->type;
    new_session->ro_session_id.s = (char*) shm_malloc(cc_acc_session->id.len);
    if (!new_session->ro_session_id.s) {
        LM_ERR("no more shm mem\n");
        goto error;
    }

    new_session->ro_session_id.len = cc_acc_session->id.len;
    memcpy(new_session->ro_session_id.s, cc_acc_session->id.s, cc_acc_session->id.len);

    LM_DBG("new CC Ro Session ID: [%.*s] stored in shared memory address [%p]\n", cc_acc_session->id.len, cc_acc_session->id.s, new_session);

    LM_DBG("Sending CCR Diameter message.\n");
//    new_session->ccr_sent = 1;      //assume we will send successfully
    cdpb.AAASessionsUnlock(cc_acc_session->hash);

    if (ro_forced_peer.len > 0) {
        LM_DBG("Sending message with Peer\n");
        ret = cdpb.AAASendMessageToPeer(ccr, &ro_forced_peer, resume_on_initial_ccr, (void *) ssd);
    } else {
        LM_DBG("Sending message without Peer and realm is [%.*s]\n", ccr->dest_realm->data.len, ccr->dest_realm->data.s);
        ret = cdpb.AAASendMessage(ccr, resume_on_initial_ccr, (void *) ssd);
    }

    if (ret != 1) {
        LM_ERR("Failed to send Diameter CCR\n");
//        new_session->ccr_sent = 0;
        goto error;
    }

    Ro_free_CCR(ro_ccr_data);

    LM_DBG("Registering for callbacks on Dialog [%p] and charging session [%p]\n", dlg, new_session);

    //TODO: if the following fail, we should clean up the Ro session.......
    if (dlgb.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED | DLGCB_CONFIRMED, dlg_callback_received, (void*) new_session, NULL) != 0) {
        LM_CRIT("cannot register callback for dialog confirmation\n");
        goto error;
    }

    counter_inc(ims_charging_cnts_h.initial_ccrs);
    counter_inc(ims_charging_cnts_h.active_ro_sessions);

    if (free_called_asserted_identity) shm_free(called_asserted_identity.s); // shm_malloc in cscf_get_public_identity_from_requri
    return RO_RETURN_BREAK;

error:
    LM_DBG("Trying to reserve credit on initial INVITE failed.\n");

    if (free_called_asserted_identity) shm_free(called_asserted_identity.s); // shm_malloc in cscf_get_public_identity_from_requri
    Ro_free_CCR(ro_ccr_data);
    if (cc_acc_session) {
        cdpb.AAASessionsUnlock(cc_acc_session->hash);
        cdpb.AAADropSession(cc_acc_session);
    }

    if (ssd)
        shm_free(ssd);

	    return RO_RETURN_ERROR;
}

static void resume_on_initial_ccr(int is_timeout, void *param, AAAMessage *cca, long elapsed_msecs) {
    Ro_CCA_t *ro_cca_data = NULL;
    struct cell *t = NULL;
    struct session_setup_data *ssd = (struct session_setup_data *) param;
    int error_code = RO_RETURN_ERROR;
	str *redirecturi = 0;
	int fui_action = 0;

    if (is_timeout) {
        counter_inc(ims_charging_cnts_h.ccr_timeouts);
        LM_ERR("Transaction timeout - did not get CCA\n");
        error_code = RO_RETURN_ERROR;
        goto error0;
    }

    counter_inc(ims_charging_cnts_h.ccr_replies_received);
    counter_add(ims_charging_cnts_h.ccr_response_time, elapsed_msecs);

    if (!cca) {
        LM_ERR("Error reserving credit for CCA.\n");
        error_code = RO_RETURN_ERROR;
        goto error0;
    }

    if (!ssd) {
        LM_ERR("Session lookup data is NULL.\n");
        error_code = RO_RETURN_ERROR;
        goto error0;
    }

    // we make sure the transaction exists
    if (tmb.t_lookup_ident(&t, ssd->tindex, ssd->tlabel) < 0) {
        LM_ERR("t_continue: transaction not found\n");
        error_code = RO_RETURN_ERROR;
        goto error0;
    }

    // we bring the list of AVPs of the transaction to the current context
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

    ro_cca_data = Ro_parse_CCA_avps(cca);

    if (!ro_cca_data) {
        LM_ERR("Could not parse CCA message response.\n");
        error_code = RO_RETURN_ERROR;
		create_cca_result_code(0);
        goto error0;
    }

    LM_DBG("Ro result code is [%d]\n", (int)ro_cca_data->resultcode);
    create_cca_result_code((int)ro_cca_data->resultcode);
    if (ro_cca_data->resultcode != 2001) {
        if (ro_cca_data->resultcode != 4012)
            LM_ERR("Got bad CCA result code [%d] - reservation failed\n", (int)ro_cca_data->resultcode);
        error_code = RO_RETURN_FALSE;
        goto error1;
    }

	if (ro_cca_data->mscc->final_unit_action) {
		fui_action = ro_cca_data->mscc->final_unit_action->action;

		if (fui_action == AVP_Final_Unit_Action_Redirect) {
			if (ro_cca_data->mscc->final_unit_action->redirect_server) {
				LM_DBG("FUI with action: [%d]", ro_cca_data->mscc->final_unit_action->action);

				if (ro_cca_data->mscc->final_unit_action->action == AVP_Final_Unit_Action_Redirect) {
					LM_DBG("Have REDIRECT action with address type of [%d]\n", ro_cca_data->mscc->final_unit_action->redirect_server->address_type);
					if (ro_cca_data->mscc->final_unit_action->redirect_server->address_type == AVP_Redirect_Address_Type_SIP_URI) {
						LM_DBG("SIP URI for redirect is [%.*s] with len of %d\n",
							ro_cca_data->mscc->final_unit_action->redirect_server->server_address->len, ro_cca_data->mscc->final_unit_action->redirect_server->server_address->s,
							ro_cca_data->mscc->final_unit_action->redirect_server->server_address->len);
						redirecturi = ro_cca_data->mscc->final_unit_action->redirect_server->server_address;
					} else {
						LM_DBG("we don't cater for any redirect action which is not a SIP URI... ignoring [%d]\n", ro_cca_data->mscc->final_unit_action->redirect_server->address_type);
					}
				} else {
					LM_DBG("ignoring final unit action which is not REDIRECT - [%d]\n", fui_action);
				}
			}
		}
	}

	/* create the AVPs cca_redirect_uri and cca_fui_action  for export to cfg file */
	create_cca_fui_avps(fui_action, redirecturi);

	/* check result code at mscc level */
	if (ro_cca_data->mscc->resultcode && ro_cca_data->mscc->resultcode != 2001) {
		LM_DBG("CCA failure at MSCC level with resultcode [%d]\n", ro_cca_data->mscc->resultcode);
		error_code = RO_RETURN_FALSE;
        goto error1;
	}

    LM_DBG("Valid CCA response with time chunk of [%i] and validity [%i]\n",
            ro_cca_data->mscc->granted_service_unit->cc_time,
            ro_cca_data->mscc->validity_time);

    if (ro_cca_data->mscc->granted_service_unit->cc_time <= 0) {
        LM_DBG("got zero GSU.... reservation failed");
        error_code = RO_RETURN_FALSE;
        goto error1;
    }

    ssd->ro_session->last_event_timestamp = get_current_time_micro();
    ssd->ro_session->event_type = pending;
    ssd->ro_session->reserved_secs = ro_cca_data->mscc->granted_service_unit->cc_time;
    ssd->ro_session->valid_for = ro_cca_data->mscc->validity_time;
    ssd->ro_session->is_final_allocation = 0;

    if (ro_cca_data->mscc->final_unit_action && (ro_cca_data->mscc->final_unit_action->action == 0))
        ssd->ro_session->is_final_allocation = 1;

    Ro_free_CCA(ro_cca_data);

    LM_DBG("Freeing CCA message\n");
    cdpb.AAAFreeMessage(&cca);

    link_ro_session(ssd->ro_session, 0);

    if (ro_db_mode == DB_MODE_REALTIME) {
        ssd->ro_session->flags |= RO_SESSION_FLAG_NEW;
        if (update_ro_dbinfo(ssd->ro_session) != 0) {
            LM_ERR("Failed to update ro_session in database... continuing\n");
        };
    }

    unref_ro_session(ssd->ro_session, 1, 1); /* release our reference */

    create_cca_return_code(RO_RETURN_TRUE);

    if (t)
        tmb.unref_cell(t);

    tmb.t_continue(ssd->tindex, ssd->tlabel, ssd->action);
    shm_free(ssd);

    counter_inc(ims_charging_cnts_h.successful_initial_ccrs);

    return;

error1:
    Ro_free_CCA(ro_cca_data);

error0:
    LM_DBG("Trying to reserve credit on initial INVITE failed on cdp callback\n");
//    counter_add(ims_charging_cnts_h.active_ro_sessions, -1); /*we bumped active on the original initial ccr sent */
    counter_inc(ims_charging_cnts_h.failed_initial_ccrs);      /* drop by one as theoretically this is failed initial ccr */
    create_cca_return_code(error_code);

    if (!is_timeout && cca) {
        cdpb.AAAFreeMessage(&cca);
    }

    if (t)
        tmb.unref_cell(t);

    tmb.t_continue(ssd->tindex, ssd->tlabel, ssd->action);
    shm_free(ssd);
}

void remove_aaa_session(str *session_id) {
    AAASession *session;

    if ((session = cdpb.AAAGetCCAccSession(*session_id))) {
        LM_DBG("Found AAA CC App Auth session to delete.\n");
        cdpb.AAASessionsUnlock(session->hash);
        cdpb.AAADropCCAccSession(session);
    }
}

int get_direction_as_int(str* direction) {
    char* p = direction->s;

    if (direction->len > 0 && p) {
        if (p[0] == 'O' || p[0] == 'o') {
            return RO_ORIG_DIRECTION;
        } else if (p[0] == 'T' || p[0] == 't') {
            return RO_TERM_DIRECTION;
        }
    }
    return RO_UNKNOWN_DIRECTION;
}

static int create_cca_return_code(int result) {
    int rc;
    int_str avp_val, avp_name;
    avp_name.s.s = RO_AVP_CCA_RETURN_CODE;
    avp_name.s.len = RO_AVP_CCA_RETURN_CODE_LENGTH;

    avp_val.n = result;
    avp_val.s.s = RO_RETURN_TRUE_STR; //assume true
    avp_val.s.len = 1;

    switch (result) {
        case RO_RETURN_FALSE:
            avp_val.s.s = RO_RETURN_FALSE_STR;
            break;
        case RO_RETURN_ERROR:
            avp_val.s.s = RO_RETURN_ERROR_STR;
            break;
        default:
            if (result >= 0)
                break;

            LM_ERR("Unknown result code: %d", result);
            avp_val.s.s = "??";
    }

    if (result < 0)
        avp_val.s.len = 2;

    rc = add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val);

    if (rc < 0)
        LM_ERR("Couldn't create ["RO_AVP_CCA_RETURN_CODE"] AVP\n");
    else
        LM_DBG("Created AVP ["RO_AVP_CCA_RETURN_CODE"] successfully: value=[%d]\n", result);

    return 1;
}

static int create_cca_result_code(int result) {
    int rc;
    int_str avp_val, avp_name;
    avp_name.s.s = RO_AVP_CCA_RESULT_CODE;
    avp_name.s.len = RO_AVP_CCA_RESULT_CODE_LENGTH;
    char buf[10];

    avp_val.n = result;
    avp_val.s.len = snprintf(buf, 10, "%i", result);
    avp_val.s.s = buf;

    rc = add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val);

    if (rc < 0)
        LM_ERR("Couldn't create ["RO_AVP_CCA_RESULT_CODE"] AVP\n");
    else
        LM_DBG("Created AVP ["RO_AVP_CCA_RESULT_CODE"] successfully: value=[%d]\n", result);

    return 1;
}

static int create_cca_fui_avps(int action, str* redirecturi) {
	int_str action_avp_val, action_avp_name, redirecturi_avp_val, redirecturi_avp_name;
    action_avp_name.s.s = RO_AVP_CCA_FUI_ACTION;
    action_avp_name.s.len = RO_AVP_CCA_FUI_ACTION_LENGTH;
	redirecturi_avp_name.s.s = RO_AVP_CCA_FUI_REDIRECT_URI;
    redirecturi_avp_name.s.len = RO_AVP_CCA_FUI_REDIRECT_URI_LENGTH;
    char buf[10];
	int rc;

    action_avp_val.n = action;
    action_avp_val.s.len = snprintf(buf, 10, "%i", action);
    action_avp_val.s.s = buf;

    rc = add_avp(AVP_NAME_STR|AVP_VAL_STR, action_avp_name, action_avp_val);

    if (rc < 0)
        LM_ERR("Couldn't create ["RO_AVP_CCA_FUI_ACTION"] AVP\n");
    else
        LM_DBG("Created AVP ["RO_AVP_CCA_FUI_ACTION"] successfully: value=[%d]\n", action);

	if (redirecturi && redirecturi->len >0 && redirecturi->s) {
		redirecturi_avp_val.s.len = redirecturi->len;
		redirecturi_avp_val.s.s = redirecturi->s;

		rc = add_avp(AVP_NAME_STR|AVP_VAL_STR, redirecturi_avp_name, redirecturi_avp_val);

		if (rc < 0)
			LM_ERR("Couldn't create ["RO_AVP_CCA_FUI_REDIRECT_URI"] AVP\n");
		else
			LM_DBG("Created AVP ["RO_AVP_CCA_FUI_REDIRECT_URI"] successfully: value=[%.*s]\n", redirecturi->len, redirecturi->s);
	}

    return 1;
}

static int get_mac_avp_value(struct sip_msg *msg, str *value) {
    str mac_avp_name_str = str_init(RO_MAC_AVP_NAME);
    pv_spec_t avp_spec;
    pv_value_t val;

    pv_parse_spec2(&mac_avp_name_str, &avp_spec, 1);
    if (pv_get_spec_value(msg, &avp_spec, &val) != 0 || val.rs.len == 0) {

        value->s = "00:00:00:00:00:00";
        value->len = sizeof ("00:00:00:00:00:00") - 1;
        return -1;
    }

    *value = val.rs;
    return 0;
}
