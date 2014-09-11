/*
 * $Id$
 *
 * Copyright (C) 2007 Voice System SRL
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
 * History:
 * --------
 * 2007-07-10  initial version (ancuta)
 * 2008-04-04  added direction reporting in dlg callbacks (bogdan)
 * 2011-10      added support for early dialog termination (jason)
 */

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../socket_info.h"
#include "../../modules/tm/dlg.h"
#include "../../modules/tm/tm_load.h"
#include "../../lib/kmi/tree.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../locking.h"
#include "dlg_timer.h"
#include "dlg_hash.h"
#include "dlg_req_within.h"
#include "dlg_db_handler.h"

#define MAX_FWD_HDR        "Max-Forwards: " MAX_FWD CRLF
#define MAX_FWD_HDR_LEN    (sizeof(MAX_FWD_HDR) - 1)

extern str dlg_extra_hdrs;
extern int dlg_db_mode;

int free_tm_dlg(dlg_t *td) {
    if (td) {
        if (td->route_set)
            free_rr(&td->route_set);
        pkg_free(td);
    }
    return 0;
}

dlg_t * build_dlg_t(struct dlg_cell * cell, int dir) {

    dlg_t* td = NULL;
    str cseq;
    unsigned int loc_seq;
    str route_set;
    str contact;

    struct dlg_cell_out *dlg_out = 0;
    struct dlg_entry_out* dlg_entry_out = 0;

    /* if trying to send by to callee we need to get the corresponding dlg_out cell */
    lock_get(cell->dlg_out_entries_lock);
    dlg_entry_out = &cell->dlg_entry_out;

    dlg_out = dlg_entry_out->first;
    //must be concurrent call - lets choose - TODO - ie. check if there is more

    if (!dlg_out) {
        LM_ERR("Trying to send BYE for dialog with no callee leg\n");
        lock_release(cell->dlg_out_entries_lock);
        return NULL;
    }

    td = (dlg_t*) pkg_malloc(sizeof (dlg_t));
    if (!td) {

        LM_ERR("out of pkg memory\n");
        lock_release(cell->dlg_out_entries_lock);
        return NULL;
    }
    memset(td, 0, sizeof (dlg_t));

    if (dir == DLG_CALLER_LEG) {
        cseq = cell->first_req_cseq;
        route_set = cell->caller_route_set;
        contact = cell->caller_contact;
        td->rem_uri = cell->from_uri;
        td->loc_uri = dlg_out->to_uri;
        td->id.rem_tag = cell->from_tag;
        td->id.loc_tag = dlg_out->to_tag;
        td->send_sock = cell->caller_bind_addr;
    } else {
        cseq = dlg_out->callee_cseq;
        route_set = dlg_out->callee_route_set;
        contact = dlg_out->callee_contact;
        td->rem_uri = dlg_out->to_uri;
        td->loc_uri = cell->from_uri;
        td->id.rem_tag = dlg_out->to_tag;
        td->id.loc_tag = cell->from_tag;
        td->send_sock = dlg_out->callee_bind_addr;
    }

    if (str2int(&cseq, &loc_seq) != 0) {
        LM_ERR("invalid cseq\n");
        goto error;
    }

    /*we don not increase here the cseq as this will be done by TM*/
    td->loc_seq.value = loc_seq;
    td->loc_seq.is_set = 1;

    /*route set*/
    if (route_set.s && route_set.len) {

        if (parse_rr_body(route_set.s, route_set.len, &td->route_set) != 0) {
            LM_ERR("failed to parse route set\n");
            goto error;
        }
    }

    if (contact.s == 0 || contact.len == 0) {

        LM_ERR("no contact available\n");
        goto error;
    }

    td->id.call_id = cell->callid;
    td->rem_target = contact;
    td->state = DLG_CONFIRMED;

    lock_release(cell->dlg_out_entries_lock);
    return td;

error:
    lock_release(cell->dlg_out_entries_lock);
    free_tm_dlg(td);
    return NULL;
}

/*callback function to handle responses to the BYE request */
void bye_reply_cb(struct cell* t, int type, struct tmcb_params* ps) {

    struct dlg_cell* dlg;
    int event, old_state, new_state, unref, ret;
    struct dlg_cell_out *dlg_out = 0;

    if (ps->param == NULL || *ps->param == NULL) {
        LM_ERR("invalid parameter\n");
        return;
    }

    if (ps->code < 200) {
        LM_DBG("receiving a provisional reply\n");
        return;
    }

    LM_DBG("receiving a final reply %d\n", ps->code);

    dlg = (struct dlg_cell *) (*(ps->param));
    event = DLG_EVENT_REQBYE;

    //get the corresponding dlg out structure for this REQ
    struct dlg_entry_out *dlg_entry_out = &dlg->dlg_entry_out;
    lock_get(dlg->dlg_out_entries_lock);
    dlg_out = dlg_entry_out->first; //TODO check for concurrent call
    if (!dlg_out)
        return;

    next_state_dlg(dlg, event, &old_state, &new_state, &unref, &dlg_out->to_tag);

    lock_release(dlg->dlg_out_entries_lock);

    if (new_state == DLG_STATE_DELETED && old_state != DLG_STATE_DELETED) {

        LM_DBG("removing dialog with h_entry %u and h_id %u\n",
                dlg->h_entry, dlg->h_id);

        /* remove from timer */
        ret = remove_dialog_timer(&dlg->tl);
        if (ret < 0) {
            LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
                    "with clid '%.*s'\n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s);
        } else if (ret > 0) {
            LM_WARN("inconsitent dlg timer data on dlg %p [%u:%u] "
                    "with clid '%.*s'\n",
                    dlg, dlg->h_entry, dlg->h_id,
                    dlg->callid.len, dlg->callid.s);
        } else {
            unref++;
        }
        /* dialog terminated (BYE) */
        run_dlg_callbacks(DLGCB_TERMINATED, dlg, ps->req, ps->rpl, DLG_DIR_NONE, 0);

        /* derefering the dialog */
        unref_dlg(dlg, unref + 1);
    }

    if (new_state == DLG_STATE_DELETED && old_state == DLG_STATE_DELETED) {
        /* trash the dialog from DB and memory */
        if (dlg_db_mode)
        	remove_dialog_in_from_db(dlg);

        /* force delete from mem */
        unref_dlg(dlg, 1);
    }

}

static inline int build_extra_hdr(struct dlg_cell * cell, str *extra_hdrs,
        str *str_hdr) {
    char *p;

    str_hdr->len = MAX_FWD_HDR_LEN + dlg_extra_hdrs.len;
    if (extra_hdrs && extra_hdrs->len > 0)
        str_hdr->len += extra_hdrs->len;

    str_hdr->s = (char*) pkg_malloc(str_hdr->len * sizeof (char));
    if (!str_hdr->s) {
        LM_ERR("out of pkg memory\n");
        goto error;
    }

    memcpy(str_hdr->s, MAX_FWD_HDR, MAX_FWD_HDR_LEN);
    p = str_hdr->s + MAX_FWD_HDR_LEN;
    if (dlg_extra_hdrs.len) {
        memcpy(p, dlg_extra_hdrs.s, dlg_extra_hdrs.len);
        p += dlg_extra_hdrs.len;
    }
    if (extra_hdrs && extra_hdrs->len > 0)
        memcpy(p, extra_hdrs->s, extra_hdrs->len);

    return 0;

error:
    return -1;
}

/* cell- pointer to a struct dlg_cell
 * dir- direction: the request will be sent to:
 * 		DLG_CALLER_LEG (0): caller
 * 		DLG_CALLEE_LEG (1): callee
 */
static inline int send_bye(struct dlg_cell * cell, int dir, str *hdrs) {
    uac_req_t uac_r;
    dlg_t* dialog_info;
    str met = {"BYE", 3};
    int result;
    /* do not send BYE request for non-confirmed dialogs (not supported) */
    if (cell->state != DLG_STATE_CONFIRMED) {
        LM_ERR("terminating only 1 side of non-confirmed dialogs not supported by this function\n");
        return -1;
    }

    /*verify direction*/
    if ((dialog_info = build_dlg_t(cell, dir)) == 0) {
        LM_ERR("failed to create dlg_t\n");
        goto err;
    }

    LM_DBG("sending BYE to %s\n", (dir == DLG_CALLER_LEG) ? "caller" : "callee");

    ref_dlg(cell, 1);

    memset(&uac_r, '\0', sizeof (uac_req_t));
    set_uac_req(&uac_r, &met, hdrs, NULL, dialog_info, TMCB_LOCAL_COMPLETED,
            bye_reply_cb, (void*) cell);

    result = d_tmb.t_request_within(&uac_r);

    if (result < 0) {
        LM_ERR("failed to send the BYE request\n");
        goto err1;
    }

    free_tm_dlg(dialog_info);

    LM_DBG("BYE sent to %s\n", (dir == 0) ? "caller" : "callee");
    return 0;

err1:
    unref_dlg(cell, 1);
err:
    if (dialog_info)
        free_tm_dlg(dialog_info);
    return -1;
}

/*static void early_transaction_destroyed(struct cell* t, int type, struct tmcb_params *param) {
    struct dlg_cell *dlg = (struct dlg_cell *) (*param->param);

    if (!dlg)
        return;

    LM_DBG("Early transaction destroyed\n");
}*/

/* side =
 * 0: caller
 * 1: callee
 * 2: all
 */
int dlg_terminate(struct dlg_cell *dlg, struct sip_msg *msg, str *reason, int side, str *extra_hdrs) {

    struct cell* t;
    str default_reason = {"call failed", 11};
    int cfg_cmd = 0;
    str default_extra_headers = {0,0};

    if (!dlg) {
        LM_ERR("calling end_dialog with NULL pointer dlg\n");
        return -1;
    }

    if (!extra_hdrs)
        extra_hdrs = &default_extra_headers;


    if (msg) {
        //assume called from cfg command -> dlg_terminate, as opposed to internal API or mi interface
        cfg_cmd = 1;
    }

    if (!reason || reason->len <= 0 || !reason->s) {
        reason = &default_reason;
    }

    if (dlg->state != DLG_STATE_CONFIRMED) {
        if (side != 2) {
            LM_ERR("can't terminate only 1 side of an early dialog\n");
            return -1;
        }
        if (dlg->transaction) {
            LM_DBG("terminating early dialog with %d outbound forks\n",
                    dlg->transaction->nr_of_outgoings);

            t = dlg->transaction;

            if (t && t!=(void*) -1  && t->uas.request) {
                if (t->method.len!=6 || t->method.s[0]!='I' || t->method.s[1]!='N' || t->method.s[2]!='V')
		{
			//well this is the transaction of a subsequent request within the dialog
			//and the dialog is not confirmed yet, so its a PRACK or an UPDATE
			//could also be an options, but the important thing is how am i going to get
			//the transaction of the invite, that is the one i have to cancel
			LM_WARN("this is not my transaction so where am i?\n");
                        return 1; //TODO - need to check why we got in here once before? this crashed on t_reply as t seemed invalid
		}

                //TODO: here we are assuming none of the CALLEE's have sent a 200, in
                //which case we would have to send an ACK, BYE
                //so right now - we are sending 488 to caller and CANCEL's to all CALLEEs

                LM_DBG("tearing down dialog in EARLY state - no clients responded > 199\n");
                if (cfg_cmd) {
                        d_tmb.t_reply(msg,488,reason->s);
                        d_tmb.t_release(msg);
                } else {
                        d_tmb.t_reply(t->uas.request,488,reason->s);
                        d_tmb.t_release(t->uas.request);
                }
            }
        } else {
            LM_WARN("can't terminate early dialog without a transaction\n");
            return -1;
        }
    } else {
        LM_DBG("terminating confirmed dialog\n");
        if (side == DLG_CALLER_LEG /* 0 */ || side == DLG_CALLEE_LEG /* 1 */) {
            if (dlg_bye(dlg, (extra_hdrs->len > 0) ? extra_hdrs : NULL, side) < 0)
                return -1;

        } else {
            if (dlg_bye_all(dlg, (extra_hdrs->len > 0) ? extra_hdrs : NULL) < 0)
                return -1;
        }
    }
    return 1;
}

/*parameters from MI: callid, from tag, to tag*/
/* TODO: add reason parameter to mi interface */
struct mi_root * mi_terminate_dlg(struct mi_root *cmd_tree, void *param) {

    struct mi_node* node;
    struct dlg_cell * dlg = NULL;
    str mi_extra_hdrs = {NULL, 0};
    int status, msg_len;
    char *msg;

    str callid = {NULL, 0};
    str ftag = {NULL, 0};
    str ttag = {NULL, 0};

    if (d_table == NULL)
        goto end;

    node = cmd_tree->node.kids;

    if (node == NULL || node->next == NULL  || node->next->next == NULL)
        return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

    if (!node->value.s || !node->value.len) {
        goto error;
    } else {
        callid = node->value;
    }
    node = node->next;
    if (!node->value.s || !node->value.len) {
        goto error;
    } else {
        ftag = node->value;
    }
    node = node->next;
    if (!node->value.s || !node->value.len) {
        goto error;
    } else {
        ttag = node->value;
    }

    if (node->next) {
        node = node->next;
        if (node->value.len && node->value.s)
            mi_extra_hdrs = node->value;
    }

    unsigned int dir = DLG_DIR_NONE;
    LM_DBG("Looking for callid [%.*s]\n", callid.len, callid.s);
    dlg = get_dlg(&callid, &ftag, &ttag, &dir); //increments ref count!

    if (dlg) {
        LM_DBG("Found dialog to terminate and it is in state [%i]\n", dlg->state);

        if (dlg_terminate(dlg, 0, NULL/*reson*/, /* all sides of a dialog*/ 2, &mi_extra_hdrs) < 0) {
            status = 500;
            msg = MI_DLG_OPERATION_ERR;
            msg_len = MI_DLG_OPERATION_ERR_LEN;
        } else {
            status = 200;
            msg = MI_OK_S;
            msg_len = MI_OK_LEN;
        }
        unref_dlg(dlg, 1);

        return init_mi_tree(status, msg, msg_len);
    }
end:
    return init_mi_tree(404, MI_DIALOG_NOT_FOUND, MI_DIALOG_NOT_FOUND_LEN);

error:
    return init_mi_tree(400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

}

int dlg_bye(struct dlg_cell *dlg, str *hdrs, int side) {
    str all_hdrs = {0, 0};
    int ret;

    if (side == DLG_CALLER_LEG) {
        if (dlg->dflags & DLG_FLAG_CALLERBYE)
            return -1;
        dlg->dflags |= DLG_FLAG_CALLERBYE;
    } else {
        if (dlg->dflags & DLG_FLAG_CALLEEBYE)
            return -1;
        dlg->dflags |= DLG_FLAG_CALLEEBYE;
    }
    if ((build_extra_hdr(dlg, hdrs, &all_hdrs)) != 0) {
        LM_ERR("failed to build dlg headers\n");
        return -1;
    }
    ret = send_bye(dlg, side, &all_hdrs);
    pkg_free(all_hdrs.s);
    return ret;
}

/* Wrapper for terminating dialog from API - from other modules */
int w_api_terminate_dlg(str *callid, str *ftag, str *ttag, str *hdrs, str* reason) {
    struct dlg_cell *dlg;

    unsigned int dir = DLG_DIR_NONE;
    dlg = get_dlg(callid, ftag, ttag, &dir); //increments ref count!

    if (!dlg) {
        LM_ERR("Asked to tear down non existent dialog\n");
        return -1;
    }

    unref_dlg(dlg, 1);

    return dlg_terminate(dlg, NULL, NULL/*reason*/, 2, hdrs);

}

int dlg_bye_all(struct dlg_cell *dlg, str *hdrs) {
    str all_hdrs = {0, 0};
    int ret;

    if ((build_extra_hdr(dlg, hdrs, &all_hdrs)) != 0) {
        LM_ERR("failed to build dlg headers\n");
        return -1;
    }

    ret = send_bye(dlg, DLG_CALLER_LEG, &all_hdrs);
    ret |= send_bye(dlg, DLG_CALLEE_LEG, &all_hdrs);

    pkg_free(all_hdrs.s);
    return ret;

}


/* Wrapper for terminating dialog from API - from other modules */
int w_api_lookup_terminate_dlg(unsigned int h_entry, unsigned int h_id, str *hdrs) {
    struct dlg_cell *dlg;

    dlg = lookup_dlg(h_entry, h_id); //increments ref count!

    if (!dlg) {
        LM_ERR("Asked to tear down non existent dialog\n");
        return -1;
    }

    unref_dlg(dlg, 1);

    return dlg_terminate(dlg, NULL, NULL/*reason*/, 2, hdrs);

}

