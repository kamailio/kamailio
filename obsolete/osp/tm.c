/*
 * ser osp module. 
 *
 * This module enables ser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "../../modules/tm/tm_load.h"
#include "tm.h"
#include "destination.h"

struct tm_binds osp_tmb;

static void ospOnReq(struct cell* t, int type, struct tmcb_params* ps);
static void ospTmcbFunc(struct cell* t, int type, struct tmcb_params* ps);

/*
 * Load TM API
 * return 0 success, -1 failure
 */
int ospInitTm(void)
{
    load_tm_f load_tm;

    LOG(L_DBG, "osp: ospInitTm\n");

    if ((load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0)) == 0) {
        LOG(L_ERR, "osp: ERROR: failed to import load_tm\n");
        return -1;
    }

    if (load_tm(&osp_tmb) == -1) {
        LOG(L_ERR, "osp: ERROR: failed to load TM API\n");
        LOG(L_ERR, "osp: ERROR: TM is required for reporting call setup usage\n");
        return -1;
    }

    /* Register callbacks, listen for all incoming requests  */
    if (osp_tmb.register_tmcb(0, 0, TMCB_REQUEST_IN, ospOnReq, 0, 0) <= 0) {
        LOG(L_ERR, "osp: ERROR: failed to register TMCB_REQUEST_IN callback\n");
        LOG(L_ERR, "osp: ERROR: TM callbacks are required for reporting call set up usage\n");
        return -1;
    }

    return 0;
}

/*
 * Register OSP callback function
 * param t
 * param type
 * param ps
 */
static void ospOnReq(
    struct cell* t, 
    int type, 
    struct tmcb_params* ps)
{
    int tmcb_types;

    LOG(L_DBG, "osp: ospOnReq\n");

    /* install addaitional handlers */
    tmcb_types =
//        TMCB_REQUEST_FWDED |
//        TMCB_RESPONSE_FWDED |
        TMCB_ON_FAILURE | 
//        TMCB_LOCAL_COMPLETED |
        /* report on completed transactions */
        TMCB_RESPONSE_OUT |
        /* account e2e acks if configured to do so */
        TMCB_E2EACK_IN |
        /* report on missed calls */
        TMCB_ON_FAILURE_RO |
        /* get incoming replies ready for processing */
//        TMCB_RESPONSE_IN |
        0;

    if (osp_tmb.register_tmcb(0, t, tmcb_types, ospTmcbFunc, 0, 0) <= 0) {
        LOG(L_ERR, "osp: ERROR: failed to register TM callbacks\n");
        LOG(L_ERR, "osp: ERROR: TM callbacks are required for reporting call setup usage\n");
        return;
    }

    /* Also, if that is INVITE, disallow silent t-drop */
    if (ps->req->REQ_METHOD == METHOD_INVITE) {
        LOG(L_DBG, "osp: noisy_timer set for accounting\n");
        t->flags |= T_NOISY_CTIMER_FLAG;
    }
}

/*
 * OSP callback function
 * param t
 * param type
 * param ps
 */
static void ospTmcbFunc(
    struct cell* t, 
    int type, 
    struct tmcb_params* ps)
{
    LOG(L_DBG, "osp: ospTmcbFunc\n");

    if (type & TMCB_RESPONSE_OUT) {
        LOG(L_DBG, "osp: RESPONSE_OUT\n");
    } else if (type & TMCB_E2EACK_IN) {
        LOG(L_DBG, "osp: E2EACK_IN\n");
    } else if (type & TMCB_ON_FAILURE_RO) {
        LOG(L_DBG, "osp: FAILURE_RO\n");
    } else if (type & TMCB_RESPONSE_IN) {
        LOG(L_DBG, "osp: RESPONSE_IN\n");
    } else if (type & TMCB_REQUEST_FWDED) {
        LOG(L_DBG, "osp: REQUEST_FWDED\n");
    } else if (type & TMCB_RESPONSE_FWDED) {
        LOG(L_DBG, "osp: RESPONSE_FWDED\n");
    } else if (type & TMCB_ON_FAILURE) {
        LOG(L_DBG, "osp: FAILURE\n");
    } else if (type & TMCB_LOCAL_COMPLETED) {
        LOG(L_DBG, "osp: COMPLETED\n");
    } else {
        LOG(L_DBG, "osp: something else '%d'\n", type);
    }

    if (t) {
        ospRecordEvent(t->uac[t->nr_of_outgoings - 1].last_received, t->uas.status);
    } else {
        LOG(L_DBG, "osp: cell is empty\n");
    }
}
