/*!
 * \file
 * \brief Functions related to dialog handling
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <string.h>
#include <time.h>

#include "../../core/trim.h"
#include "../../core/pvar.h"
#include "../../core/timer.h"
#include "../../core/counters.h"
#include "../../core/action.h"
#include "../../core/script_cb.h"
#include "../../core/fmsg.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_rr.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/rr/api.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_cb.h"
#include "dlg_handlers.h"
#include "dlg_req_within.h"
#include "dlg_profile.h"
#include "dlg_var.h"
#include "dlg_db_handler.h"
#include "dlg_ng_stats.h"

static str rr_param; /*!< record-route parameter for matching */
static int dlg_flag; /*!< flag for dialog tracking */
static pv_spec_t *timeout_avp; /*!< AVP for timeout setting */
static int default_timeout; /*!< default dialog timeout */
static int seq_match_mode; /*!< dlg_match mode */
static int shutdown_done = 0; /*!< 1 when destroy_dlg_handlers was called */
extern int detect_spirals;
int spiral_detected = -1;

extern struct rr_binds d_rrb; /*!< binding to record-routing module */
extern struct tm_binds d_tmb;
extern struct dialog_ng_counters_h dialog_ng_cnts_h;
time_t act_time;

extern pv_elem_t *ruri_param_model; /*!< pv-string to get r-uri */

static unsigned int CURR_DLG_LIFETIME = 0; /*!< current dialog lifetime */
static unsigned int CURR_DLG_STATUS = 0; /*!< current dialog state */
static unsigned int CURR_DLG_ID = 0xffffffff; /*!< current dialog id */


/*! size of the dialog record-route parameter */
#define RR_DLG_PARAM_SIZE  (2*2*sizeof(int)+3+MAX_DLG_RR_PARAM_NAME)
/*! separator inside the record-route paramter */
#define DLG_SEPARATOR      '.'

/*!
 * \brief Initialize the dialog handlers
 * \param rr_param_p added record-route parameter
 * \param dlg_flag_p dialog flag
 * \param timeout_avp_p AVP for timeout setting
 * \param default_timeout_p default timeout
 * \param seq_match_mode_p matching mode
 */
void init_dlg_handlers(char *rr_param_p, int dlg_flag_p, pv_spec_t *timeout_avp_p, int default_timeout_p, int seq_match_mode_p) {
    rr_param.s = rr_param_p;
    rr_param.len = strlen(rr_param.s);

    dlg_flag = 1 << dlg_flag_p;

    timeout_avp = timeout_avp_p;
    default_timeout = default_timeout_p;
    seq_match_mode = seq_match_mode_p;
}

/*!
 * \brief Shutdown operation of the module
 */
void destroy_dlg_handlers(void) {
    shutdown_done = 1;
}

/*!
 * \brief Add record-route parameter for dialog tracking
 * \param req SIP request
 * \param entry dialog hash entry
 * \param id dialog hash id
 * \return 0 on success, -1 on failure
 */
static inline int add_dlg_rr_param(struct sip_msg *req, unsigned int entry, unsigned int id) {
    static char buf[RR_DLG_PARAM_SIZE];
    str s;
    int n;
    char *p;

    s.s = p = buf;

    *(p++) = ';';
    memcpy(p, rr_param.s, rr_param.len);
    p += rr_param.len;
    *(p++) = '=';

    n = RR_DLG_PARAM_SIZE - (p - buf);
    if (int2reverse_hex(&p, &n, entry) == -1)
        return -1;

    *(p++) = DLG_SEPARATOR;

    n = RR_DLG_PARAM_SIZE - (p - buf);
    if (int2reverse_hex(&p, &n, id) == -1)
        return -1;

    s.len = p - buf;

    if (d_rrb.add_rr_param(req, &s) < 0) {
        LM_ERR("failed to add rr param\n");
        return -1;
    }

    return 0;
}

/*!
 * \brief Unreference a dialog from tm callback (another wrapper)
 * \param t transaction, unused
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
static void unref_dlg_from_cb(struct cell* t, int type, struct tmcb_params *param) {
    dlg_cell_t *dlg = NULL;
    dlg_iuid_t *iuid = NULL;

    LM_DBG("Unref dlg from callback called\n");
    iuid = (dlg_iuid_t*) (*param->param);
    if (iuid == NULL)
        return;

    dlg = dlg_get_by_iuid(iuid);
    if (dlg == NULL)
        return;
    /* unref by 2: 1 set when adding in tm cb, 1 sent by dlg_get_by_iuid() */
    unref_dlg(dlg, 2);
}

/*!
 * \brief Parse SIP message and populate leg informations
 *
 * Parse SIP message and populate leg informations.
 * \param dlg the dialog to add cseq, contact & record_route
 * \param msg sip message
 * \param t transaction
 * \param leg type of the call leg
 * \param tag SIP To tag
 * \return 0 on success, -1 on failure
 * \note for a request: get record route in normal order, for a reply get
 * in reverse order, skipping the ones from the request and the proxies' own
 */
int populate_leg_info(struct dlg_cell *dlg, struct sip_msg *msg,
        struct cell* t, unsigned int leg, str *tag) {
    unsigned int skip_recs;
    str cseq;
    str contact;
    str rr_set;
    struct socket_info* callee_bind_address = NULL;

    if (leg == DLG_CALLER_LEG)
        dlg->caller_bind_addr = msg->rcv.bind_address;
    else
        callee_bind_address = msg->rcv.bind_address;



    /* extract the cseq number as string from the request or response*/
    //TO DO - can pair the cseqs here to make sure that the response and request are in sync

    if ((!msg->cseq && (parse_headers(msg, HDR_CSEQ_F, 0) < 0 || !msg->cseq))
            || !msg->cseq->parsed) {
        LM_ERR("bad sip message or missing CSeq hdr :-/\n");
        goto error0;
    }
    cseq = (get_cseq(msg))->number;


    /* extract the contact address */
    if (!msg->contact && (parse_headers(msg, HDR_CONTACT_F, 0) < 0 || !msg->contact)) {
        if (msg->first_line.type == SIP_REQUEST)
            LM_ERR("bad sip message or missing Contact hdr for message [%.*s]\n", msg->first_line.u.request.method.len, msg->first_line.u.request.method.s);
        else
            LM_ERR("bad sip message or missing Contact hdr for message [%.*s]\n", msg->first_line.u.reply.status.len, msg->first_line.u.reply.status.s);
        goto error0;
    }
    if (parse_contact(msg->contact) < 0 ||
            ((contact_body_t *) msg->contact->parsed)->contacts == NULL ||
            ((contact_body_t *) msg->contact->parsed)->contacts->next != NULL) {
        LM_ERR("bad Contact HDR\n");
        goto error0;
    }
    contact = ((contact_body_t *) msg->contact->parsed)->contacts->uri;

    /* extract the RR parts */
    if (!msg->record_route && (parse_headers(msg, HDR_EOH_F, 0) < 0)) {
        LM_ERR("failed to parse record route header\n");
        goto error0;
    }

    if (leg == DLG_CALLER_LEG) {
        skip_recs = 0;
    } else {
        skip_recs = 0;
        /* was the 200 OK received or local generated */
        skip_recs = dlg->from_rr_nb +
                ((t->relayed_reply_branch >= 0) ?
                ((t->uac[t->relayed_reply_branch].flags & TM_UAC_FLAG_R2) ? 2 :
                ((t->uac[t->relayed_reply_branch].flags & TM_UAC_FLAG_RR) ? 1 : 0))
                : 0);
    }

    if (msg->record_route) {
        if (print_rr_body(msg->record_route, &rr_set, leg,
                &skip_recs) != 0) {
            LM_ERR("failed to print route records \n");
            goto error0;
        }
    } else {
        rr_set.s = 0;
        rr_set.len = 0;
    }

    if (leg == DLG_CALLER_LEG)
        dlg->from_rr_nb = skip_recs;

    LM_DBG("route_set %.*s, contact %.*s, cseq %.*s and bind_addr %.*s [%p]\n",
            rr_set.len, rr_set.s, contact.len, contact.s,
            cseq.len, cseq.s,
            msg->rcv.bind_address->sock_str.len,
            msg->rcv.bind_address->sock_str.s, msg->rcv.bind_address);

    if (dlg_set_leg_info(dlg, tag, &rr_set, &contact, &cseq, callee_bind_address, leg) != 0) {
        LM_ERR("dlg_set_leg_info failed\n");
        if (rr_set.s) pkg_free(rr_set.s);
        goto error0;
    }

    if (rr_set.s) pkg_free(rr_set.s);

    return 0;
error0:
    return -1;
}

/*!
 * \brief Clone dialog internal unique id to shared memory
 * \param dlg dialog
 */
dlg_iuid_t *dlg_get_iuid_shm_clone(dlg_cell_t *dlg) {
    dlg_iuid_t *iuid = NULL;

    if (dlg == NULL)
        return NULL;

    iuid = (dlg_iuid_t*) shm_malloc(sizeof (dlg_iuid_t));
    if (iuid == NULL) {
        LM_ERR("failed to clone dialog iuid\n");
        return NULL;
    }

    memset(iuid, 0, sizeof (dlg_iuid_t));
    iuid->h_entry = dlg->h_entry;
    iuid->h_id = dlg->h_id;

    return iuid;
}

/*!
 * \brief Free dialog internal unique id stored in shared memory
 * \param iuid dialog iuid
 */
void dlg_iuid_sfree(void *iuid) {
    if (iuid) {
        LM_DBG("freeing dlg iuid [%u:%u] (%p)\n",
                ((dlg_iuid_t*) iuid)->h_entry,
                ((dlg_iuid_t*) iuid)->h_id, iuid);
        shm_free(iuid);
    }
}

///*!
// * \brief Function that executes BYE reply callbacks
// * \param t transaction, unused
// * \param type type of the callback, should be TMCB_RESPONSE_FWDED
// * \param param saved dialog structure inside the callback
// */
//static void dlg_terminated_confirmed(struct cell* t,
//        int type,
//        struct tmcb_params* params) {
//    if (!params || !params->req || !params->param) {
//        LM_ERR("invalid parameters!\n");
//       return;
//    }
//
//   struct dlg_cell* dlg = (struct dlg_cell*) *params->param;
//
//    if (!dlg) {
//        LM_ERR("failed to get dialog from params!\n");
//        return;
//    }
//    /* dialog termination confirmed (BYE reply) */
//    run_dlg_callbacks(DLGCB_TERMINATED_CONFIRMED,
//            dlg,
//            params->req,
//            params->rpl,
//            DLG_DIR_UPSTREAM,
//            0);
//}

static void dlg_cancel_confirmed(struct cell* t,
        int type,
        struct tmcb_params* params) {
    if (!params || !params->req || !params->param) {
        LM_ERR("invalid parameters!\n");
        return;
    }

    struct dlg_cell* dlg = (struct dlg_cell*) *params->param;

    if (!dlg) {
        LM_ERR("failed to get dialog from params!\n");
        return;
    }
    /* dialog termination confirmed (BYE reply) */
    run_dlg_callbacks(DLGCB_TERMINATED_CONFIRMED,
            dlg,
            params->req,
            params->rpl,
            DLG_DIR_UPSTREAM,
            0);
}

/*!
 * \brief Execute callback for the BYE request and register callback for the BYE reply
 * \param req request message
 * \param dlg corresponding dialog
 * \param dir message direction
 */
static void dlg_terminated(struct sip_msg* req,
        struct dlg_cell* dlg,
        unsigned int dir) {
    if (!req) {
        LM_ERR("request is empty!");
        return;
    }

    if (!dlg) {
        LM_ERR("dialog is empty!");
        return;
    }

    /* dialog terminated (BYE) */
    run_dlg_callbacks(DLGCB_TERMINATED, dlg, req, NULL, dir, 0);

//    /* register callback for the coresponding reply */
//    LM_DBG("Registering tmcb1\n");
//    if (d_tmb.register_tmcb(req,
//            0,
//            TMCB_RESPONSE_OUT,
//            dlg_terminated_confirmed,
//            (void*) dlg,
//            0) <= 0) {
//        LM_ERR("cannot register response callback for BYE request\n");
//        return;
//    }
}

/*!
 * \brief Execute callback for the BYE request and register callback for the BYE reply
 * \param req request message
 * \param dlg corresponding dialog
 * \param dir message direction
 */
static void dlg_cancelled(struct sip_msg* req,
        struct dlg_cell* dlg,
        unsigned int dir) {
    if (!req) {
        LM_ERR("request is empty!");
        return;
    }

    if (!dlg) {
        LM_ERR("dialog is empty!");
        return;
    }

    /* dialog terminated (BYE) */
    run_dlg_callbacks(DLGCB_TERMINATED, dlg, req, NULL, dir, 0);

    /* register callback for the coresponding reply */
    LM_DBG("Registering tmcb for CANCEL confirmed\n");
    if (d_tmb.register_tmcb(req,
            0,
            TMCB_RESPONSE_OUT,
            dlg_cancel_confirmed,
            (void*) dlg,
            0) <= 0) {
        LM_ERR("cannot register response callback for CANCEL request\n");
        return;
    }
}

//static void unlink_dlgouts_from_cb(struct cell* t, int type, struct tmcb_params *param) {
//    struct dlg_cell *dlg = (struct dlg_cell *) (*param->param);
//
//    if (!dlg)
//        return;

//    if (t && t->fwded_totags && t->fwded_totags->tag.len > 0) {
//        LM_DBG("unlink_dlgouts_from_cb: transaction [%.*s] can now be removed IFF it has been marked for deletion\n", t->fwded_totags->tag.len, t->fwded_totags->tag.s);
//        dlg_remove_dlg_out_tag(dlg, &t->fwded_totags->tag);
//    }
//}

/*static void dlg_oncancel(struct cell* t, int type, struct tmcb_params *param) {
    struct dlg_cell* dlg;
    int unref = 0, old_state, new_state;

    LM_DBG("RECEIVED CANCEL ON DIALOG......\n");
    dlg = (struct dlg_cell *) (*param->param);
    if (shutdown_done || dlg == 0)
        return;

    struct sip_msg *req = param->req;
    old_state = dlg->state;
    LM_DBG("DLG dialog id is entry:id [%i:%i]\n", dlg->h_entry, dlg->h_id);
    next_state_dlg(dlg, DLG_EVENT_REQCANCEL, &old_state, &new_state, &unref, 0);

    if (old_state != DLG_STATE_DELETED && new_state == DLG_STATE_DELETED)
        dlg_cancelled(req, dlg, DLG_DIR_NONE);

    unref_dlg(dlg, unref);
}*/

/*!
 * \brief Helper function that run dialog callbacks on forwarded requests
 * \see dlg_seq_up_onreply
 * \see dlg_seq_down_onreply
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param param saved dialog structure inside the callback
 * \param direction direction of the request
 */
static void dlg_seq_onreply_helper(struct cell* t, int type,
        struct tmcb_params *param, const int direction) {
    dlg_cell_t *dlg = NULL;
    dlg_iuid_t *iuid = NULL;

    if (shutdown_done)
        return;
    iuid = (dlg_iuid_t*) (*param->param);
    dlg = dlg_get_by_iuid(iuid);
    if (dlg == 0)
        return;

    if (type == TMCB_RESPONSE_FWDED) {
        run_dlg_callbacks(DLGCB_RESPONSE_WITHIN,
                dlg,
                param->req,
                param->rpl,
                direction,
                0);
    }
    dlg_release(dlg);

    return;
}

/*!
 * \brief Run dialog callbacks on forwarded requests in upstream direction
 * \see dlg_seq_onreply_helper
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param param saved dialog structure inside the callback
 */
static void dlg_seq_up_onreply(struct cell* t, int type, struct tmcb_params *param) {
    return dlg_seq_onreply_helper(t, type, param, DLG_DIR_UPSTREAM);
}

/*!
 * \brief Run dialog callbacks on forwarded requests in downstream direction
 * \see dlg_seq_onreply_helper
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param param saved dialog structure inside the callback
 */
static void dlg_seq_down_onreply(struct cell* t, int type, struct tmcb_params *param) {
    return dlg_seq_onreply_helper(t, type, param, DLG_DIR_DOWNSTREAM);
}

/*!
 * \brief Return the timeout for a dialog
 * \param req SIP message
 * \return value from timeout AVP if present or default timeout
 */
inline static int get_dlg_timeout(struct sip_msg *req) {
    pv_value_t pv_val;

    if (timeout_avp) {
        if (pv_get_spec_value(req, timeout_avp, &pv_val) == 0 &&
                pv_val.flags & PV_VAL_INT && pv_val.ri > 0) {
            return pv_val.ri;
        }
        LM_DBG("invalid AVP value, using default timeout\n");
    }
    return default_timeout;
}

/*!
 * \brief Helper function to get the necessary content from SIP message
 * \param req SIP request
 * \param callid found callid
 * \param ftag found from tag
 * \param ttag found to tag
 * \param with_ttag flag set if to tag must be found for success
 * \return 0 on success, -1 on failure
 */
static inline int pre_match_parse(struct sip_msg *req, str *callid,
        str *ftag, str *ttag, int with_ttag) {
    if (parse_headers(req, HDR_CALLID_F | HDR_TO_F, 0) < 0 || !req->callid ||
            !req->to) {
        LM_ERR("bad request or missing CALLID/TO hdr :-/\n");
        return -1;
    }

    if (get_to(req)->tag_value.len == 0) {
        if (with_ttag == 1) {
            /* out of dialog request with preloaded Route headers; ignore. */
            return -1;
        } else {
            ttag->s = NULL;
            ttag->len = 0;
        }
    } else {
        *ttag = get_to(req)->tag_value;
    }

    if (parse_from_header(req) < 0 || get_from(req)->tag_value.len == 0) {
        LM_ERR("failed to get From header\n");
        return -1;
    }

    /* callid */
    *callid = req->callid->body;
    trim(callid);
    /* from tag */
    *ftag = get_from(req)->tag_value;
    return 0;
}

static inline char* tm_type_to_string(int type) {
    switch (type) {
        case TMCB_REQUEST_IN:
            return "TMCB_REQUEST_IN";
        case TMCB_RESPONSE_IN:
            return "TMCB_RESPONSE_IN";
        case TMCB_E2EACK_IN:
            return "TMCB_E2EACK_IN";
        case TMCB_REQUEST_PENDING:
            return "TMCB_REQUEST_PENDING";
        case TMCB_REQUEST_FWDED:
            return "TMCB_REQUEST_FWDED";
        case TMCB_RESPONSE_FWDED:
            return "TMCB_RESPONSE_FWDED";
        case TMCB_ON_FAILURE_RO:
            return "TMCB_ON_FAILURE_RO";
        case TMCB_ON_FAILURE:
            return "TMCB_ON_FAILURE";
        case TMCB_REQUEST_OUT:
            return "TMCB_REQUEST_OUT";
        case TMCB_RESPONSE_OUT:
            return "TMCB_RESPONSE_OUT";
        case TMCB_LOCAL_COMPLETED:
            return "TMCB_LOCAL_COMPLETED";
        case TMCB_LOCAL_RESPONSE_OUT:
            return "TMCB_LOCAL_RESPONSE_OUT";
        case TMCB_ACK_NEG_IN:
            return "TMCB_ACK_NEG_IN";
        case TMCB_REQ_RETR_IN:
            return "TMCB_REQ_RETR_IN";
        case TMCB_LOCAL_RESPONSE_IN:
            return "TMCB_LOCAL_RESPONSE_IN";
        case TMCB_LOCAL_REQUEST_IN:
            return "TMCB_LOCAL_REQUEST_IN";
        case TMCB_DLG:
            return "TMCB_DLG";
        case TMCB_DESTROY:
            return "TMCB_DESTROY";
        case TMCB_E2ECANCEL_IN:
            return "TMCB_E2ECANCEL_IN";
        case TMCB_E2EACK_RETR_IN:
            return "TMCB_E2EACK_RETR_IN";
        case TMCB_RESPONSE_READY:
            return "TMCB_RESPONSE_READY";
        case TMCB_DONT_ACK:
            return "TMCB_DONT_ACK";
        case TMCB_REQUEST_SENT:
            return "TMCB_REQUEST_SENT";
        case TMCB_RESPONSE_SENT:
            return "TMCB_RESPONSE_SENT";
        case TMCB_ON_BRANCH_FAILURE:
            return "TMCB_ON_BRANCH_FAILURE";
        case TMCB_ON_BRANCH_FAILURE_RO:
            return "TMCB_ON_BRANCH_FAILURE_RO";
        case TMCB_MAX:
            return "TMCB_MAX";
    }

    return "UNKNOWN";
}

/*
 * \brief Unreference a dialog (small wrapper to take care of shutdown)
 * \see unref_dlg
 * \param dialog unreferenced dialog
 */

/*static void unreference_dialog(void *dialog) {
    // if the dialog table is gone, it means the system is shutting down.
    if (!dialog || !d_table)
        return;
    unref_dlg((struct dlg_cell*) dialog, 1);
}*/

/*!
 * \brief Parse the record-route parameter, to get dialog information back
 * \param p start of parameter string
 * \param end end of parameter string
 * \param h_entry found dialog hash entry
 * \param h_id found dialog hash id
 * \return 0 on success, -1 on failure
 */
static inline int parse_dlg_rr_param(char *p, char *end, int *h_entry, int *h_id) {
    char *s;

    for (s = p; p < end && *p != DLG_SEPARATOR; p++);
    if (*p != DLG_SEPARATOR) {
        LM_ERR("malformed rr param '%.*s'\n", (int) (long) (end - s), s);
        return -1;
    }

    if (reverse_hex2int(s, p - s, (unsigned int*) h_entry) < 0) {
        LM_ERR("invalid hash entry '%.*s'\n", (int) (long) (p - s), s);
        return -1;
    }

    if (reverse_hex2int(p + 1, end - (p + 1), (unsigned int*) h_id) < 0) {
        LM_ERR("invalid hash id '%.*s'\n", (int) (long) (end - (p + 1)), p + 1);
        return -1;
    }

    return 0;
}

/*!
 * \brief Register a transaction on a dialog
 * \param t transaction
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
/*static int store_dlg_in_tm(struct sip_msg* msg,
        struct cell* t,
        struct dlg_cell *dlg) {
    if (!msg || msg == FAKED_REPLY || !t || !dlg) {
        LM_ERR("invalid parameter msg(%p), t(%p), dlg(%p)\n", msg, t, dlg);
        return -1;
    }

    if (get_dialog_from_tm(t)) {
        LM_NOTICE("dialog %p is already set for this transaction!\n", dlg);
        return 1;
    }

    // facilitate referencing of dialog through TMCB_MAX
    if (d_tmb.register_tmcb(msg,
            t,
            TMCB_MAX,
            dlg_tmcb_dummy,
            (void*) dlg, unreference_dialog) < 0) {
        LM_ERR("failed cache in T the shortcut to dlg %p\n", dlg);
        return -3;
    }

    // registering succeeded, we must increase the reference counter
    ref_dlg(dlg, 1);

    return 0;
}*/

/*
 * \brief Callback to register a transaction on a dialog
 * \param t transaction, unused
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */

/*static void store_dlg_in_tm_cb(struct cell* t,
        int type,
        struct tmcb_params *param) {
    struct dlg_cell *dlg = (struct dlg_cell *) (*param->param);

    struct sip_msg* msg = param->rpl;
    if (msg == NULL || msg == FAKED_REPLY) {
        msg = param->req;
    }

    store_dlg_in_tm(msg, t, dlg);
}*/

/*!
 * \brief Update the saved CSEQ information in dialog from SIP message
 * \param dlg updated dialog
 * \param req SIP request
 * \param dir direction of request, must DLG_DIR_UPSTREAM or DLG_DIR_DOWNSTREAM
 * \param to_tag dialog to tag
 * \return 0 on success, -1 on failure
 */
static inline int update_cseqs(struct dlg_cell *dlg, struct sip_msg *req,
        unsigned int dir, str *to_tag) {
    if ((!req->cseq && parse_headers(req, HDR_CSEQ_F, 0) < 0) || !req->cseq ||
            !req->cseq->parsed) {
        LM_ERR("bad sip message or missing CSeq hdr :-/\n");
        return -1;
    }

    if (dir == DLG_DIR_UPSTREAM) {
        return dlg_update_cseq(dlg, DLG_CALLEE_LEG, &((get_cseq(req))->number), to_tag);
    } else if (dir == DLG_DIR_DOWNSTREAM) {
        return dlg_update_cseq(dlg, DLG_CALLER_LEG, &((get_cseq(req))->number), to_tag);
    } else {
        LM_CRIT("dir is not set!\n");
        return -1;
    }
}

/*!
 * \brief Function that is registered as TM callback and called on requests
 * \see dlg_new_dialog
 * \param t transaction, used to created the dialog
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
void dlg_onreq(struct cell* t, int type, struct tmcb_params *param) {
    sip_msg_t *req = param->req;
    dlg_cell_t *dlg = NULL;

    if (req->first_line.u.request.method_value == METHOD_BYE) {
        _dlg_ctx.t = 1;
        return;
    }

    if (req->first_line.u.request.method_value != METHOD_INVITE/* && req->first_line.u.request.method_value != METHOD_CANCEL*/) {
        return;
    }

    dlg = dlg_get_ctx_dialog();

    if (dlg != NULL) {
        if (spiral_detected == 1) {
            LM_DBG("Running DLGB_SPIRALED callback\n");
            run_dlg_callbacks(DLGCB_SPIRALED, dlg,
                    req, NULL, DLG_DIR_DOWNSTREAM, 0);
            LM_DBG("This is a spiraled REQUEST\n");
        } else if (spiral_detected == 0)
            run_create_callbacks(dlg, req);
    }
    if (dlg == NULL) {
        if ((req->flags & dlg_flag) != dlg_flag)
            return;
        LM_DBG("dialog creation on config flag\n");
        dlg_new_dialog(req, t, 1);
        dlg = dlg_get_ctx_dialog();
    }
    if (dlg != NULL) {
        if (dlg_set_tm_callbacks(t, req, dlg, spiral_detected) !=0) {
            LM_ERR("Failed to register TM Callbacks for dialog... this is bad!\n");
            //the DLG_TM flag will not be set so at least we can clear the dialog later...
        } else {
            LM_DBG("dialog [%p] added to tm callbacks\n", dlg);
        }
        _dlg_ctx.t = 1;
        dlg_release(dlg);
    } else {
        LM_ERR("Failed to create dialog.... this is bad\n");
    }
}

/*!
 * \brief Function that is registered as RR callback for dialog tracking
 *
 * Function that is registered as RR callback for dialog tracking. It
 * sets the appropriate events after the SIP method and run the state
 * machine to update the dialog state. It updates then the saved
 * dialogs and also the statistics.
 * \param req SIP request
 * \param route_params record-route parameter
 * \param param unused
 */
void dlg_onroute(struct sip_msg* req, str *route_params, void *param) {
    dlg_cell_t *dlg;
    dlg_iuid_t *iuid;
    str val, callid, ftag, ttag;
    int h_entry, h_id, new_state, old_state, unref, event, timeout;
    unsigned int dir;
    int ret = 0;
    int reset = 1;

    dlg = dlg_get_ctx_dialog();
    if (dlg != NULL) {
        dlg_release(dlg);
        return;
    }

    /* skip initial requests - they may end up here because of the
     * preloaded route */
    if ((!req->to && parse_headers(req, HDR_TO_F, 0) < 0) || !req->to) {
        LM_ERR("bad request or missing TO hdr :-/\n");
        return;
    }
    if (get_to(req)->tag_value.len == 0) {
        LM_DBG("No to tag header found --- ignoring... MESSAGE [%.*s\n", req->first_line.u.request.method.len, req->first_line.u.request.method.s);
        return;
    }

    dlg = 0;
    dir = DLG_DIR_NONE;

    if (seq_match_mode != SEQ_MATCH_NO_ID) {
        if (d_rrb.get_route_param(req, &rr_param, &val) != 0) {
            LM_DBG("Route param '%.*s' not found\n", rr_param.len, rr_param.s);
            if (seq_match_mode == SEQ_MATCH_STRICT_ID)
                return;
        } else {
            LM_DBG("route param is '%.*s' (len=%d)\n", val.len, val.s, val.len);

            if (parse_dlg_rr_param(val.s, val.s + val.len, &h_entry, &h_id) < 0)
                return;

            dlg = lookup_dlg(h_entry, h_id);
            if (dlg == 0) {
                LM_WARN("unable to find dialog for %.*s "
                        "with route param '%.*s' [%u:%u]\n",
                        req->first_line.u.request.method.len,
                        req->first_line.u.request.method.s,
                        val.len, val.s, h_entry, h_id);
                if (seq_match_mode == SEQ_MATCH_STRICT_ID)
                    return;
            } else {
                if (pre_match_parse(req, &callid, &ftag, &ttag, 1) < 0) {
                    // lookup_dlg has incremented the ref count by 1
                    unref_dlg(dlg, 1);
                    return;
                }
                if (match_dialog(dlg, &callid, &ftag, &ttag, &dir) == 0) {
                    LM_WARN("tight matching failed for %.*s with callid='%.*s'/%d, "
                            "ftag='%.*s'/%d, ttag='%.*s'/%d and direction=%d\n",
                            req->first_line.u.request.method.len,
                            req->first_line.u.request.method.s,
                            callid.len, callid.s, callid.len,
                            ftag.len, ftag.s, ftag.len,
                            ttag.len, ttag.s, ttag.len, dir);
                    LM_WARN("dialog identification elements are callid='%.*s'/%d, "
                            "caller tag='%.*s'/%d\n",
                            dlg->callid.len, dlg->callid.s, dlg->callid.len,
                            dlg->from_tag.len, dlg->from_tag.s,
                            dlg->from_tag.len);
                    // lookup_dlg has incremented the ref count by 1
                    dlg_release(dlg);

                    // Reset variables in order to do a lookup based on SIP-Elements.
                    dlg = 0;
                    dir = DLG_DIR_NONE;

                    if (seq_match_mode == SEQ_MATCH_STRICT_ID)
                        return;
                }
            }
        }
    }

    if (dlg == 0) {
        if (pre_match_parse(req, &callid, &ftag, &ttag, 1) < 0)
            return;
        /* TODO - try to use the RR dir detection to speed up here the
         * search -bogdan */
        dlg = get_dlg(&callid, &ftag, &ttag, &dir);
        if (!dlg) {
            LM_DBG("Callid '%.*s' not found\n",
                    req->callid->body.len, req->callid->body.s);
            return;
        }
    }

    /* set current dialog - re-use ref increment from dlg_get() above */
    set_current_dialog(req, dlg);
    _dlg_ctx.iuid.h_entry = dlg->h_entry;
    _dlg_ctx.iuid.h_id = dlg->h_id;

    if (req->first_line.u.request.method_value != METHOD_ACK) {
        iuid = dlg_get_iuid_shm_clone(dlg);
        if (iuid != NULL) {
            /* register callback for the replies of this request */
            if (d_tmb.register_tmcb(req, 0, TMCB_RESPONSE_IN | TMCB_ON_FAILURE,
                    dlg_onreply, (void*) iuid, dlg_iuid_sfree) < 0) {
                LM_ERR("failed to register TMCB (3)\n");
                shm_free(iuid);
            }
            iuid = NULL;
        }
    }
    //    if (d_tmb.register_tmcb(req, NULL, TMCB_REQUEST_FWDED,
    //            store_dlg_in_tm_cb, (void*) dlg, NULL) < 0) {
    //        LM_ERR("failed to store dialog in transaction during dialog creation for later reference\n");
    //    }

    /* run state machine */
    switch (req->first_line.u.request.method_value) {
        case METHOD_PRACK:
            event = DLG_EVENT_REQPRACK;
            break;
        case METHOD_ACK:
            event = DLG_EVENT_REQACK;
            break;
        case METHOD_BYE:
            event = DLG_EVENT_REQBYE;
            break;
//        case METHOD_CANCEL:
//            event = DLG_EVENT_REQCANCEL;
//            break;
        default:
            event = DLG_EVENT_REQ;
    }

    next_state_dlg(dlg, event, &old_state, &new_state, &unref, 0);
    LM_DBG("unref after next state is %i\n", unref);
    CURR_DLG_ID = req->id;
    CURR_DLG_LIFETIME = (unsigned int) (time(0)) - dlg->start_ts;
    CURR_DLG_STATUS = new_state;

    //    dlg_run_event_route(dlg, req, old_state, new_state);

    /* delay deletion of dialog until transaction has died off in order
     * to absorb in-air messages */
    if ((new_state == DLG_STATE_DELETED) && (old_state != DLG_STATE_DELETED)) {
        iuid = dlg_get_iuid_shm_clone(dlg);
        if (iuid != NULL) {
            if (d_tmb.register_tmcb(req, NULL, TMCB_DESTROY,
                    unref_dlg_from_cb, (void*) iuid, dlg_iuid_sfree) < 0) {
                LM_ERR("failed to register deletion delay function\n");
                shm_free(iuid);
            } else {
                ref_dlg(dlg, 1);
            }
        }
//        unref_dlg(dlg, unref);
    }

    //    if (new_state == DLG_STATE_CONFIRMED && old_state != DLG_STATE_CONFIRMED)
    //        dlg_ka_add(dlg);

    if (event == DLG_EVENT_REQCANCEL && new_state == DLG_STATE_DELETED &&
            old_state != DLG_STATE_DELETED) {
        LM_DBG("CANCEL successfully processed and old state was [%d]\n", old_state);
        
        ret = remove_dialog_timer(&dlg->tl);
        if (ret < 0) {
            LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
                    "with clid '%.*s' and tags '%.*s'\n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);

        } else if (ret > 0) {
            LM_WARN("inconsistent dlg timer data on dlg %p [%u:%u] "
                    "with clid '%.*s' and tags '%.*s' \n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);

        } else {
            unref++;
        }
        
        dlg_terminated(req, dlg, dir);
        unref_dlg(dlg, unref);
        _dlg_ctx.cpid = my_pid();
        _dlg_ctx.expect_t = 1;
        dlg_set_ctx_iuid(dlg);
        
        if (old_state == DLG_STATE_EARLY) {
            counter_add(dialog_ng_cnts_h.early, -1);
        } else if (old_state == DLG_STATE_CONFIRMED ||old_state == DLG_STATE_CONFIRMED_NA) {
            counter_add(dialog_ng_cnts_h.active, -1);
        }
        
        goto done;
    }
    
    /* run actions for the transition */
    if (event == DLG_EVENT_REQBYE && new_state == DLG_STATE_DELETED &&
            old_state != DLG_STATE_DELETED) {
        LM_DBG("BYE successfully processed\n");
        /* remove from timer */
        ret = remove_dialog_timer(&dlg->tl);
        if (ret < 0) {
            LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
                    "with clid '%.*s' and tags '%.*s'\n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);

        } else if (ret > 0) {
            LM_WARN("inconsistent dlg timer data on dlg %p [%u:%u] "
                    "with clid '%.*s' and tags '%.*s' \n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);

        } else {
            unref++;
        }
        /* dialog terminated (BYE) */
        dlg_terminated(req, dlg, dir);
        unref_dlg(dlg, unref);
        _dlg_ctx.cpid = my_pid();
        _dlg_ctx.expect_t = 1;
        dlg_set_ctx_iuid(dlg);

        if (old_state == DLG_STATE_CONFIRMED ||old_state == DLG_STATE_CONFIRMED_NA) {
            counter_add(dialog_ng_cnts_h.active, -1);
        }
        
        goto done;
    }

    if (event == DLG_EVENT_REQCANCEL && new_state == DLG_STATE_DELETED &&
            old_state != DLG_STATE_DELETED) {
        LM_DBG("CANCEL successfully processed\n");
        /* remove from timer */
        ret = remove_dialog_timer(&dlg->tl);
        if (ret < 0) {
            LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
                    "with clid '%.*s' and tags '%.*s'\n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);

        } else if (ret > 0) {
            LM_WARN("inconsistent dlg timer data on dlg %p [%u:%u] "
                    "with clid '%.*s' and tags '%.*s' \n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);

        } else {
            unref++;
        }
        /* dialog terminated (CANCEL) */
        dlg_cancelled(req, dlg, dir);
//        unref_dlg(dlg, unref);
        _dlg_ctx.cpid = my_pid();
        _dlg_ctx.expect_t = 1;
        dlg_set_ctx_iuid(dlg);
        counter_add(dialog_ng_cnts_h.early, -1);
        goto done;
    }

    if ((event == DLG_EVENT_REQ || event == DLG_EVENT_REQACK)
            && (new_state == DLG_STATE_CONFIRMED || new_state == DLG_STATE_EARLY)) {

        if (event != DLG_EVENT_REQACK) {
            if (update_cseqs(dlg, req, dir, &ttag) != 0) {
                LM_ERR("cseqs update failed\n");
            } else {
                dlg->dflags |= DLG_FLAG_CHANGED;
            }
        }
        if (dlg_db_mode == DB_MODE_REALTIME && (dlg->dflags & DLG_FLAG_CHANGED)) {
            update_dialog_dbinfo(dlg);
        }

        if (old_state == DLG_STATE_CONFIRMED_NA) {
            LM_DBG("confirming ACK successfully processed\n");

            /* confirming ACK request */
            run_dlg_callbacks(DLGCB_CONFIRMED, dlg, req, NULL, dir, 0);
        } else {
            LM_DBG("sequential request successfully processed\n");

            /* within dialog request */
            run_dlg_callbacks(DLGCB_REQ_WITHIN, dlg, req, NULL, dir, 0);

            if ((event != DLG_EVENT_REQACK) &&
                    (dlg->cbs.types) & DLGCB_RESPONSE_WITHIN) {
                iuid = dlg_get_iuid_shm_clone(dlg);
                if (iuid != NULL) {
                    /* register callback for the replies of this request */
                    if (d_tmb.register_tmcb(req, 0, TMCB_RESPONSE_FWDED,
                            (dir == DLG_DIR_UPSTREAM) ? dlg_seq_down_onreply :
                            dlg_seq_up_onreply,
                            (void*) iuid, dlg_iuid_sfree) < 0) {
                        LM_ERR("failed to register TMCB (2)\n");
                        shm_free(iuid);
                    }
                }
            }
        }
        
        timeout = get_dlg_timeout(req);
        if (timeout != default_timeout) {
            dlg->lifetime = timeout;
        }
        //        reset = !((dlg->iflags & DLG_IFLAG_TIMER_NORESET) || dlg_timeout_noreset);

        if ((new_state != DLG_STATE_EARLY) && (old_state != DLG_STATE_CONFIRMED || reset)) {
            if (update_dlg_timer(&dlg->tl, dlg->lifetime) == -1) {
                LM_ERR("failed to update dialog lifetime\n");
            } else {
                dlg->dflags |= DLG_FLAG_CHANGED;
            }
        }
    }

    if (new_state == DLG_STATE_CONFIRMED && old_state == DLG_STATE_CONFIRMED_NA) {
        dlg->dflags |= DLG_FLAG_CHANGED;
        if (dlg_db_mode == DB_MODE_REALTIME)
            update_dialog_dbinfo(dlg);
    }

done:
    dlg_release(dlg);
    return;
}

/*!
 * \brief Function that is registered as TM callback and called on replies
 *
 * Function that is registered as TM callback and called on replies. It
 * parses the reply and set the appropriate event. This is then used to
 * update the dialog state, run eventual dialog callbacks and save or
 * update the necessary informations about the dialog.
 * \see next_state_dlg
 * \param t transaction, unused
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
void dlg_onreply(struct cell* t, int type, struct tmcb_params *param) {
    dlg_cell_t *dlg = NULL;
    dlg_iuid_t *iuid = NULL;
    struct dlg_cell_out *dlg_out = 0;
    char *event_s = 0;

    int new_state, old_state, unref, event;
    str to_tag={0,0}, to_uri={0,0}, branch={0,0};
    struct sip_msg *req = param->req;
    struct sip_msg *rpl = param->rpl;
    struct dlg_entry_out* dlg_entry_out = 0;

    char* cb_type_s = tm_type_to_string(type);

    if (t && t->fwded_totags)
		LM_DBG("ONREPLY CALL_BACK from TM received and type is [%i] = [%s]"
				" and TO is [%.*s]\n", type, cb_type_s, t->fwded_totags->tag.len,
				t->fwded_totags->tag.s);
    else
        LM_DBG("ONREPLY CALL_BACK from TM received and type is [%i] = [%s]\n",
        		type, cb_type_s);

    if (shutdown_done)
        return;

    iuid = (dlg_iuid_t*) (*param->param);
    dlg = dlg_get_by_iuid(iuid);
    if (dlg == 0)
        return;

    if (t) {
        dlg->transaction = t;
    }

    unref = 0;

    LM_DBG("DLG dialogid is entry:id [%i:%i]\n", dlg->h_entry, dlg->h_id);

    if (type & TMCB_E2ECANCEL_IN) {
        LM_DBG("Received CANCEL...\n");
        goto done;
    }
    
    if (type & TMCB_REQUEST_OUT) {
        LM_DBG("Received Request out for request [%.*s]\n", param->send_buf.len, param->send_buf.s);
        if (param->send_buf.s && (param->send_buf.len > 6) && !memcmp(param->send_buf.s, "CANCEL", 6)) {
            LM_DBG("Request out sending CANCEL...\n");
            type = TMCB_E2ECANCEL_IN;
            goto done;
        } else {
            LM_DBG("Ignoring request out as it's not a CANCEL\n");
            goto done;
        }
    }

    if (type & (TMCB_RESPONSE_IN | TMCB_ON_FAILURE)) {
        /* Set the dialog context so it is available in onreply_route and failure_route*/
        set_current_dialog(req, dlg);
        dlg_set_ctx_iuid(dlg);
        goto done;
    }

    if (type == TMCB_RESPONSE_FWDED) {
        // The state does not change, but the msg is mutable in this callback
        LM_DBG("TMCB_RESPONSE_FWDED from TM received");
        run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
        goto done;
    }

    if (type == TMCB_RESPONSE_READY) {
        if (rpl == FAKED_REPLY) {
            LM_DBG("Faked reply\n");
            //TODO - maybe we should run the state machine especially for things like cancel (ie early dialog.....)
            to_tag.s = 0;
            to_tag.len = 0;
//            goto done;
        } else {
        // get to tag
        LM_DBG("Extracting to-tag from reply");
        if (!rpl->to && ((parse_headers(rpl, HDR_TO_F, 0) < 0) || !rpl->to)) {
            LM_DBG("bad reply or missing TO hdr :-/\n");
            to_tag.s = 0;
            to_tag.len = 0;
        } else {
            //populate to uri for this branch.
            to_uri = get_to(rpl)->uri;

            to_tag = get_to(rpl)->tag_value;
            if (to_tag.s == 0 || to_tag.len == 0) {
                LM_DBG("missing TAG param in TO hdr :-/\n");
                to_tag.s = 0;
                to_tag.len = 0;
                //Here we assume that the transaction module timer will remove any early dialogs
                //return;       if we leave this then we have over-reffed dialogs
                }
            }
        }

        LM_DBG("Got to-tag from response: %.*s \n", to_tag.len, to_tag.s);
    }

    if (type == TMCB_DESTROY) {
        event = DLG_EVENT_TDEL;
        event_s = "DLG_EVENT_TDEL";
//    } else if (type == TMCB_E2ECANCEL_IN) {
//        event = DLG_EVENT_REQCANCEL;
//        event_s = "DLG_EVENT_REQCANCEL";
    } else if (param->code < 200) {
        event = DLG_EVENT_RPL1xx;
        event_s = "DLG_EVENT_RPL1xx";
    } else if (param->code < 300) {
        event = DLG_EVENT_RPL2xx;
        event_s = "DLG_EVENT_RPL2xx";
    } else {
        event = DLG_EVENT_RPL3xx;
        event_s = "DLG_EVENT_RPL3xx";
    }

    LM_DBG("Calling next_state_dlg and event is %i = %s\n", event, event_s);
    next_state_dlg(dlg, event, &old_state, &new_state, &unref, &to_tag);

    if (type == TMCB_RESPONSE_READY) {
        LM_DBG("Checking if there is an existing dialog_out entry with same to-tag");
        if (rpl == FAKED_REPLY) {
            LM_DBG("Faked reply\n");
            
            if (new_state == DLG_STATE_DELETED
                && (old_state == DLG_STATE_UNCONFIRMED
                || old_state == DLG_STATE_EARLY)) {
                LM_DBG("dialog %p failed (negative reply)\n", dlg);
                /* dialog setup not completed (3456XX) */
                run_dlg_callbacks(DLGCB_FAILED, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
                /* do unref */
                if (unref)
                    unref_dlg(dlg, unref);

                if (old_state == DLG_STATE_EARLY)
                    counter_add(dialog_ng_cnts_h.early, -1);
            }
            goto done;
        }

        // get to tag
        LM_DBG("Extracting to-tag from reply");
        if (rpl == FAKED_REPLY || (!rpl->to && ((parse_headers(rpl, HDR_TO_F, 0) < 0) || !rpl->to))) {
            LM_ERR("faked reply or bad reply or missing TO hdr :-/\n");
            to_tag.s = 0;
            to_tag.len = 0;
        } else {
            //populate to uri for this branch.
            to_uri = get_to(rpl)->uri;

            to_tag = get_to(rpl)->tag_value;
            if (to_tag.s == 0 || to_tag.len == 0) {
                LM_DBG("missing TAG param in TO hdr :-/ - probably a failure response\n");
                to_tag.s = 0;
                to_tag.len = 0;
                //Here we assume that the transaction module timer will remove any early dialogs
                if (new_state == DLG_STATE_DELETED
                        && (old_state == DLG_STATE_UNCONFIRMED
                        || old_state == DLG_STATE_EARLY)) {
                    LM_DBG("dialog %p failed (negative reply)\n", dlg);
                    /* dialog setup not completed (3456XX) */
                    run_dlg_callbacks(DLGCB_FAILED, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
                    /* do unref */
                    if (unref)
                        unref_dlg(dlg, unref);

                    if (old_state == DLG_STATE_EARLY)
                        counter_add(dialog_ng_cnts_h.early, -1);
                }
                goto done;
            }
        }
        LM_DBG("Got to-tag from response: %.*s \n", to_tag.len, to_tag.s);

        dlg_entry_out = &dlg->dlg_entry_out;

        lock_get(dlg->dlg_out_entries_lock);
        dlg_out = dlg_entry_out->first;

        LM_DBG("Scanning dlg_entry_out list for dlg_out");
        while (dlg_out) {
            //Check if there is an already dialog_out entry with same To-tag
            if (dlg_out->to_tag.len == to_tag.len &&
                    memcmp(dlg_out->to_tag.s, to_tag.s, dlg_out->to_tag.len) == 0) {
                //Found a dialog_out entry with same to_tag!
                LM_DBG("Found dlg_out for to-tag: %.*s\n", dlg_out->to_tag.len, dlg_out->to_tag.s);
                break;
            }
            dlg_out = dlg_out->next;
        }
        lock_release(dlg->dlg_out_entries_lock);

        if (!dlg_out) {
            if (rpl->first_line.u.reply.statuscode < 299) { /*we don't care about failure responses to dialog - not necessary to create dialog out...*/
                if (rpl->via1->branch && (rpl->via1->branch->value.s) && (rpl->via1->branch->value.len > 0)) {
                    branch = rpl->via1->branch->value;
                }

                LM_DBG("No dlg_out entry found - creating a new dialog_out entry on dialog [%p]\n", dlg);
                dlg_out = build_new_dlg_out(dlg, &to_uri, &to_tag, &branch);

                link_dlg_out(dlg, dlg_out, 0);

                /* save callee's cseq, caller cseq, callee contact and callee record route*/
                if (populate_leg_info(dlg, rpl, t, DLG_CALLEE_LEG, &to_tag) != 0) {
                    LM_ERR("could not add further info to the dlg out\n");
                }

                if (!dlg_out) {
                    LM_ERR("failed to create new dialog out structure\n");
                    goto done;
                    //TODO do something on this error!

                }
            }
        } else {
            //This dlg_out already exists, update cseq and contact if present

            LM_DBG("dlg_out entry found - updating cseq's for dialog out [%p] for to-tag [%.*s] \n", dlg_out, dlg_out->to_tag.len, dlg_out->to_tag.s);

            if ((!rpl->cseq && parse_headers(rpl, HDR_CSEQ_F, 0) < 0) || !rpl->cseq ||
                    !rpl->cseq->parsed) {
                LM_ERR("bad sip message or missing CSeq hdr :-/\n");
            }
            dlg_update_cseq(dlg, DLG_CALLEE_LEG, &((get_cseq(rpl))->number), &(dlg_out->to_tag));


            /* extract the contact address to update if present*/
            if (!rpl->contact && (parse_headers(rpl, HDR_CONTACT_F, 0) < 0 || !rpl->contact)) {
                LM_DBG("Can not update callee contact: bad sip message or missing Contact hdr\n");
            } else if (parse_contact(rpl->contact) < 0 ||
                    ((contact_body_t *) rpl->contact->parsed)->contacts == NULL ||
                    ((contact_body_t *) rpl->contact->parsed)->contacts->next != NULL) {
                LM_ERR("Can not update callee contact: bad Contact HDR\n");
            } else {
                str contact;
                contact = ((contact_body_t *) rpl->contact->parsed)->contacts->uri;
                dlg_update_contact(dlg, DLG_CALLEE_LEG, &contact, &(dlg_out->to_tag));
            }
        }
    }
    if (new_state == DLG_STATE_EARLY) {
        if (dlg_db_mode == DB_MODE_REALTIME)
            update_dialog_dbinfo(dlg);
        run_dlg_callbacks(DLGCB_EARLY, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
        if (old_state != DLG_STATE_EARLY)
            counter_inc(dialog_ng_cnts_h.early);
        goto done;
    }

    LM_DBG("new state is %i and old state is %i\n", new_state, old_state);

    if (new_state == DLG_STATE_CONFIRMED_NA &&
            old_state != DLG_STATE_CONFIRMED_NA && old_state != DLG_STATE_CONFIRMED) {
        //    if ((new_state == DLG_STATE_CONFIRMED) && (event == DLG_EVENT_RPL2xx)) {
        LM_DBG("dialog %p confirmed (No Ack) \n", dlg);

        /* set start time */
        dlg->start_ts = (unsigned int) (time(0));

        /* save the settings to the database,
         * if realtime saving mode configured- save dialog now
         * else: the next time the timer will fire the update*/
        if ((dlg->dflags & DLG_FLAG_INSERTED) == 0) {
            dlg->dflags |= DLG_FLAG_NEW;
        } else {
            dlg->dflags |= DLG_FLAG_CHANGED;
        }
        if (dlg_db_mode == DB_MODE_REALTIME)
            update_dialog_dbinfo(dlg);

        if (0 != insert_dlg_timer(&dlg->tl, dlg->lifetime)) {
            LM_CRIT("Unable to insert dlg %p [%u:%u] on event %d [%d->%d] "
                    "with clid '%.*s' and tags '%.*s' \n",
                    dlg, dlg->h_entry, dlg->h_id, event, old_state, new_state,
                    dlg->callid.len, dlg->callid.s,
                    dlg->from_tag.len, dlg->from_tag.s);
        } else {
            ref_dlg(dlg, 1);
        }

        /* dialog confirmed (ACK pending) */
        run_dlg_callbacks(DLGCB_CONFIRMED_NA, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);

        if (old_state == DLG_STATE_EARLY)
            counter_add(dialog_ng_cnts_h.early, -1);

        if (unref) unref_dlg(dlg, unref);

        counter_inc(dialog_ng_cnts_h.active);

        goto done;
    }

    if (new_state == DLG_STATE_CONCURRENTLY_CONFIRMED && (old_state == DLG_STATE_CONFIRMED || old_state == DLG_STATE_CONCURRENTLY_CONFIRMED)) {
        //This is a concurrently confirmed call
        LM_DBG("This is a concurrently confirmed call.");
        //Create a new Dialog ID token “X”
        //Not sure how to do this so just going to use existing Did and add an X character to it
        str new_did;
        create_concurrent_did(dlg, &new_did);

        //assign new did to the created or updated dialog_out entry.
        update_dlg_out_did(dlg_out, &new_did);

        //Then, duplicate the dialog_in entry and set its Dialog ID value to new_did
        //for now rather just create new dlg structure with the correct params - this should be fixed if future use requires

        struct dlg_cell *new_dlg = 0;
        new_dlg = build_new_dlg(&(dlg->callid) /*callid*/,
                &(dlg->from_uri) /*from uri*/,
                &(dlg->from_tag)/*from_tag*/,
                &(dlg->req_uri) /*r-uri*/);

        //assign new did to dlg_in
        update_dlg_did(new_dlg, &new_did);

        if (new_dlg == 0) {
            LM_ERR("failed to create new dialog\n");
            goto done;
        }

        //link the new_dlg with dlg_out object
        link_dlg_out(new_dlg, dlg_out, 0);

    }

    if (new_state == DLG_STATE_DELETED
            && (old_state == DLG_STATE_UNCONFIRMED
            || old_state == DLG_STATE_EARLY)) {
        LM_DBG("dialog %p failed (negative reply)\n", dlg);
        /* dialog setup not completed (3456XX) */
        run_dlg_callbacks(DLGCB_FAILED, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
        /* do unref */
        if (unref)
            unref_dlg(dlg, unref);

        if (old_state == DLG_STATE_EARLY)
            counter_add(dialog_ng_cnts_h.early, -1);
        goto done;
    }

    if (unref) unref_dlg(dlg, unref);

done:
    /* unref due to dlg_get_by_iuid() */
    dlg_release(dlg);
    return;
}

/*!
 * \brief Unreference a new dialog, helper function for dlg_onreq
 * \see dlg_onreq
 * \param dialog unreferenced dialog
 */
/*static void unref_new_dialog(void *dialog) {
    struct tmcb_params p;

    memset(&p, 0, sizeof (struct tmcb_params));
    p.param = (void*) &dialog;
    dlg_onreply(0, TMCB_DESTROY, &p);
}*/

/*!
 * \brief Dummy callback just to keep the compiler happy
 * \param t unused
 * \param type unused
 * \param param unused
 */
void dlg_tmcb_dummy(struct cell* t, int type, struct tmcb_params *param) {
    return;
}

/*!
 * \brief Create a new dialog from a sip message
 *
 * Create a new dialog from a SIP message, register a callback
 * to keep track of the dialog with help of the tm module.
 * This function is either called from the request callback, or
 * from the dlg_manage function in the configuration script.
 * \see dlg_onreq
 * \see w_dlg_manage
 * \param req SIP message
 * \param t transaction
 * \param run_initial_cbs if set zero, initial callbacks are not executed
 * \return 0 on success, -1 on failure
 */
int dlg_new_dialog(struct sip_msg *req, struct cell *t, const int run_initial_cbs) {
    struct dlg_cell *dlg;
    str s;
    str callid;
    str ftag;
    str ttag;
    str req_uri;
    unsigned int dir;
    int mlock;

    if (req->first_line.u.request.method_value != METHOD_INVITE)
        return -1;

    LM_DBG("starting dlg_new_dialog and method is [%.*s]\n", req->first_line.u.request.method.len, req->first_line.u.request.method.s);

    if (pre_match_parse(req, &callid, &ftag, &ttag, 0) < 0) {
        LM_WARN("pre-matching failed\n");
        return -1;
    }

    if (ttag.s != 0 && ttag.len != 0)
        return -1;

    if (pv_printf_s(req, ruri_param_model, &req_uri) < 0) {
        LM_ERR("error - cannot print the r-uri format\n");
        return -1;
    }
    trim(&req_uri);

    mlock = 1;
    dlg = search_dlg(&callid, &ftag, &ttag, &dir);
    if (detect_spirals) {
        if (spiral_detected == 1) {
            LM_DBG("spiral detected - returning 0\n");
            return 0;
        }
        dir = DLG_DIR_NONE;
        if (dlg) {
            mlock = 0;
            if (dlg->state != DLG_STATE_DELETED) {
                LM_DBG("Callid '%.*s' found, must be a spiraled request\n",
                        callid.len, callid.s);
                spiral_detected = 1;

                if (run_initial_cbs)
                    run_dlg_callbacks(DLGCB_SPIRALED, dlg, req, NULL, DLG_DIR_DOWNSTREAM, 0);

                //Add did to rr header for all spiralled requested INVITEs
                if (req->first_line.u.request.method_value == METHOD_INVITE) {
                    if (add_dlg_rr_param(req, dlg->h_entry, dlg->h_id) < 0) {
                        LM_ERR("failed to add RR param\n");
                    }
                }
//                dlg_release(dlg); //this is a fix so that we register for terminating callbacks too (caters for spiralled invites on same P/S-CSCF)
                _dlg_ctx.cpid = my_pid();
                _dlg_ctx.iuid.h_entry = dlg->h_entry;
                _dlg_ctx.iuid.h_id = dlg->h_id;
                set_current_dialog(req, dlg);
                dlg_release(dlg);
                return 0;
            } else {
                LM_DBG("spiral deteced and current state is [%d]\n", dlg->start_ts);
            }
            // get_dlg has incremented the ref count by 1
            dlg_release(dlg);
        }
    } else {
        // get_dlg has incremented the ref count by 1
        if (dlg)
            dlg_release(dlg);
    }
    spiral_detected = 0;


    LM_DBG("Building new Dialog for call-id %.*s\n", callid.len, callid.s);
    LM_DBG("SIP Method: %.*s  \n", req->first_line.u.request.method.len, req->first_line.u.request.method.s);
    dlg = build_new_dlg(&callid /*callid*/,
            &(get_from(req)->uri) /*from uri*/,
            &ftag/*from_tag*/,
            &req_uri /*r-uri*/);

    if (dlg == 0) {
        if (likely(mlock == 1)) dlg_hash_release(&callid);
        LM_ERR("failed to create new dialog\n");
        return -1;
    }

    /* save caller's tag, cseq, contact and record route*/
    if (populate_leg_info(dlg, req, t, DLG_CALLER_LEG,
            &(get_from(req)->tag_value)) != 0) {
        LM_ERR("could not add further info to the dialog\n");
        lock_destroy(dlg->dlg_out_entries_lock);
        lock_dealloc(dlg->dlg_out_entries_lock);
        shm_free(dlg);
        return -1;
    }

    /* Populate initial varlist: */
    dlg->vars = get_local_varlist_pointer(req, 1);

    /* if search_dlg() returned NULL, slot was kept locked */
    link_dlg(dlg, 0, mlock);
    if (likely(mlock == 1)) dlg_hash_release(&callid);

    s.s = _dlg_ctx.to_route_name;
    s.len = strlen(s.s);
    dlg_set_toroute(dlg, &s);
    dlg->sflags |= _dlg_ctx.flags;
    //    dlg->iflags |= _dlg_ctx.iflags;

    //    if (dlg_send_bye != 0 || _dlg_ctx.to_bye != 0)
    //        dlg->iflags |= DLG_IFLAG_TIMEOUTBYE;

    if (run_initial_cbs) run_create_callbacks(dlg, req);

    dlg->lifetime = get_dlg_timeout(req);

    /* first INVITE seen (dialog created, unconfirmed) */
    if (seq_match_mode != SEQ_MATCH_NO_ID &&
            add_dlg_rr_param(req, dlg->h_entry, dlg->h_id) < 0) {
        LM_ERR("failed to add RR param\n");
        goto error;
    }

    counter_inc(dialog_ng_cnts_h.processed);

    _dlg_ctx.cpid = my_pid();
    _dlg_ctx.iuid.h_entry = dlg->h_entry;
    _dlg_ctx.iuid.h_id = dlg->h_id;
    set_current_dialog(req, dlg);

    LM_DBG("New dialog created\n");

    return 0;

error:
    if (!spiral_detected)
        unref_dlg(dlg, 1); // undo ref regarding linking
    return -1;

}

/*!
 * \brief add dlg structure to tm callbacks
 * \param t current transaction
 * \param req current sip request
 * \param dlg current dialog
 * \param smode if the sip request was spiraled
 * \return 0 on success, -1 on failure
 */
int dlg_set_tm_callbacks(tm_cell_t *t, sip_msg_t *req, dlg_cell_t *dlg,
        int smode) {
    dlg_iuid_t *iuid = NULL;
    if (t == NULL)
        return -1;

    if (smode == 0) {
//        if (smode == 1) {
//            LM_DBG("Setting up TM callbacks for spiralled request on T (%p)\n", t);
//        }
        iuid = dlg_get_iuid_shm_clone(dlg);
        if (iuid == NULL) {
            LM_ERR("failed to create dialog unique id clone\n");
            goto error;
        }
        if (d_tmb.register_tmcb(req, t,
                TMCB_RESPONSE_IN | TMCB_RESPONSE_READY | TMCB_RESPONSE_FWDED | TMCB_ON_FAILURE | TMCB_E2ECANCEL_IN | TMCB_REQUEST_OUT,
                dlg_onreply, (void*) iuid, dlg_iuid_sfree) < 0) {
            LM_ERR("failed to register TMCB\n");
            goto error;
        }
    } else {
        LM_DBG("Not registering for TM callbacks because this is a spiraled request and we should have already registered callbacks on this transaction....\n");
    }

    dlg->dflags |= DLG_FLAG_TM;

    return 0;
error:
    dlg_iuid_sfree(iuid);
    return -1;
}

static str reason_hdr_s = {"Reason: dialog_timeout\r\n", 24};

/*!
 * \brief Timer function that removes expired dialogs, run timeout route
 * \param tl dialog timer list
 */
void dlg_ontimeout(struct dlg_tl *tl) {
    dlg_cell_t *dlg;
    int new_state, old_state, unref;
    sip_msg_t *fmsg;
    

    /* get the dialog tl payload */
    dlg = ((struct dlg_cell*) ((char *) (tl) -
            (unsigned long) (&((struct dlg_cell*) 0)->tl)));

    /* mark dialog as expired */
    dlg->dflags |= DLG_FLAG_EXPIRED;

    if (dlg->state == DLG_STATE_CONFIRMED_NA
            || dlg->state == DLG_STATE_CONFIRMED) {
        if (dlg->toroute > 0 && dlg->toroute < main_rt.entries
                && main_rt.rlist[dlg->toroute] != NULL) {
            fmsg = faked_msg_next();
            if (exec_pre_script_cb(fmsg, REQUEST_CB_TYPE) > 0) {
                ref_dlg(dlg, 1);
                dlg_set_ctx_iuid(dlg);
                LM_DBG("executing route %d on timeout\n", dlg->toroute);
                set_route_type(REQUEST_ROUTE);
                run_top_route(main_rt.rlist[dlg->toroute], fmsg, 0);
                dlg_reset_ctx_iuid();
                exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
                unref_dlg(dlg, 1);
            }
        }

        //        if (dlg->iflags & DLG_IFLAG_TIMEOUTBYE) { //TODO return therse flags
        if (dlg->state == DLG_STATE_CONFIRMED) {
            if (dlg_bye_all(dlg, &reason_hdr_s) < 0 )  /* function only supports DLG_STATE_CONFIRMED*/
               LM_DBG("Failed to do dlg_bye_all.!!");
//            unref_dlg(dlg, 1);
        }
//		else if (dlg->state == DLG_STATE_CONFIRMED_NA) 
//			unref_dlg(dlg, 1);
//        counter_inc(dialog_ng_cnts_h.expired);
//        return;
    }

    next_state_dlg(dlg, DLG_EVENT_REQBYE, &old_state, &new_state, &unref, 0);
    //    dlg_run_event_route(dlg, NULL, old_state, new_state);     //TODO replace

    if (new_state == DLG_STATE_DELETED && old_state != DLG_STATE_DELETED) {
        LM_WARN("timeout for dlg with CallID '%.*s' and tags '%.*s'\n",
                dlg->callid.len, dlg->callid.s,
                dlg->from_tag.len, dlg->from_tag.s);

        /* dialog timeout */
        run_dlg_callbacks(DLGCB_EXPIRED, dlg, NULL, NULL, DLG_DIR_NONE, 0);

        unref_dlg(dlg, unref + 1);
        counter_add(dialog_ng_cnts_h.active, -1);
        counter_inc(dialog_ng_cnts_h.expired);
    } else {
        unref_dlg(dlg, 1);
    }

    return;
}

/*!
 * \brief Function that returns the dialog lifetime as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_lifetime(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
    int l = 0;
    char *ch = NULL;

    if (msg == NULL || res == NULL)
        return -1;

    if (CURR_DLG_ID != msg->id)
        return pv_get_null(msg, param, res);

    res->ri = CURR_DLG_LIFETIME;
    ch = int2str((unsigned long) res->ri, &l);

    res->rs.s = ch;
    res->rs.len = l;

    res->flags = PV_VAL_STR | PV_VAL_INT | PV_TYPE_INT;

    return 0;
}

/*!
 * \brief Function that returns the dialog state as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_status(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
    int l = 0;
    char *ch = NULL;

    if (msg == NULL || res == NULL)
        return -1;

    if (CURR_DLG_ID != msg->id)
        return pv_get_null(msg, param, res);

    res->ri = CURR_DLG_STATUS;
    ch = int2str((unsigned long) res->ri, &l);

    res->rs.s = ch;
    res->rs.len = l;

    res->flags = PV_VAL_STR | PV_VAL_INT | PV_TYPE_INT;

    return 0;
}

/*!
 * \brief Helper function that prints all the properties of a dialog including all the dlg_out's
 * \param dlg dialog cell
 * \return void
 */

void internal_print_all_dlg(struct dlg_cell *dlg) {

    LM_DBG("Trying to get lock for printing\n");
    lock_get(dlg->dlg_out_entries_lock);

    struct dlg_cell_out *dlg_out;
    struct dlg_entry_out *d_entry_out = &(dlg->dlg_entry_out);

    LM_DBG("----------------------------");
    LM_DBG("Dialog h_entry:h_id = [%u : %u]\n", dlg->h_entry, dlg->h_id);
	LM_DBG("Dialog age: %ld\n", act_time - dlg->init_ts);
    LM_DBG("Dialog call-id: %.*s\n", dlg->callid.len, dlg->callid.s);
    LM_DBG("Dialog state: %d\n", dlg->state);
    LM_DBG("Dialog ref counter: %d\n", dlg->ref);
    LM_DBG("Dialog did: %.*s\n", dlg->did.len, dlg->did.s);
    LM_DBG("Dialog from_tag: %.*s\n", dlg->from_tag.len, dlg->from_tag.s);
    LM_DBG("Dialog from_uri: %.*s\n", dlg->from_uri.len, dlg->from_uri.s);
    LM_DBG("Dialog caller contact: %.*s\n", dlg->caller_contact.len, dlg->caller_contact.s);
    LM_DBG("Dialog first request cseq: %.*s\n", dlg->first_req_cseq.len, dlg->first_req_cseq.s);
    LM_DBG("Dialog caller route set: %.*s\n", dlg->caller_route_set.len, dlg->caller_route_set.s);
    LM_DBG("Dialog lifetime: %d\n", dlg->lifetime);
    LM_DBG("Dialog bind_address: %.*s\n", dlg->caller_bind_addr ? dlg->caller_bind_addr->sock_str.len : 0, dlg->caller_bind_addr ? dlg->caller_bind_addr->sock_str.s : 0);
    LM_DBG("Dialog-creating transaction pointer [%p]\n", dlg->transaction);

    dlg_out = d_entry_out->first;

    while (dlg_out) {

        LM_DBG("----------");
        LM_DBG("Dialog out h_entry:h_id = [%u : %u]\n", dlg_out->h_entry, dlg_out->h_id);
        LM_DBG("Dialog out did: %.*s\n", dlg_out->did.len, dlg_out->did.s);
        LM_DBG("Dialog out to_tag: %.*s\n", dlg_out->to_tag.len, dlg_out->to_tag.s);
        LM_DBG("Dialog out caller cseq: %.*s\n", dlg_out->caller_cseq.len, dlg_out->caller_cseq.s);
        LM_DBG("Dialog out callee cseq: %.*s\n", dlg_out->callee_cseq.len, dlg_out->callee_cseq.s);
        LM_DBG("Dialog out callee contact: %.*s\n", dlg_out->callee_contact.len, dlg_out->callee_contact.s);
        LM_DBG("Dialog out callee route set: %.*s\n", dlg_out->callee_route_set.len, dlg_out->callee_route_set.s);
        LM_DBG("Dialog out branch: %.*s\n", dlg_out->branch.len, dlg_out->branch.s);
        LM_DBG("Dialog out state (deleted): %i\n", dlg_out->deleted);

        LM_DBG("----------");
        dlg_out = dlg_out->next;
    }

    LM_DBG("Releasing lock for dlgout\n");
    lock_release(dlg->dlg_out_entries_lock);

    LM_DBG("----------------------------");

}

/*!
 * \brief Helper function that prints information for all dialogs
 * \return void
 */

void print_all_dlgs() {
    //print all dialog information  - this is just for testing and is set to happen every 10 seconds

    struct dlg_cell *dlg;
    unsigned int i;

	act_time = time(0);

    LM_DBG("********************");
    LM_DBG("printing %i dialogs\n", d_table->size);

    for (i = 0; i < d_table->size; i++) {
        dlg_lock(d_table, &(d_table->entries[i]));

        for (dlg = d_table->entries[i].first; dlg; dlg = dlg->next) {
            internal_print_all_dlg(dlg);
        }
        dlg_unlock(d_table, &(d_table->entries[i]));
    }
    LM_DBG("********************");

}

/*!
 * \brief Get the current dialog based on the current SIP message
 * \param msg SIP message
 * \return current dialog, null if none.
 */

struct dlg_cell *dlg_get_msg_dialog(sip_msg_t *msg) {
    struct dlg_cell *dlg = NULL;
    str callid;
    str ftag;
    str ttag;
    unsigned int dir;

    /* Retrieve the current dialog */
    dlg = dlg_get_ctx_dialog();
    if (dlg != NULL)
        return dlg;

    if (pre_match_parse(msg, &callid, &ftag, &ttag, 0) < 0)
        return NULL;
    dir = DLG_DIR_NONE;
    dlg = get_dlg(&callid, &ftag, &ttag, &dir);
    if (dlg == NULL) {
        LM_DBG("dlg with callid '%.*s' not found\n",
                msg->callid->body.len, msg->callid->body.s);
        return NULL;
    }
    return dlg;
}


