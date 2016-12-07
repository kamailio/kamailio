/*
 * Kamailio osp module. 
 *
 * This module enables Kamailio to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
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
 * ---------
 *  2006-03-13  TM functions are loaded via API function (bogdan)
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
    if (load_tm_api(&osp_tmb) != 0) {
       LM_ERR("failed to load TM API\n");
       LM_ERR("TM is required for reporting call setup usage\n");
        return -1;
    }

    /* Register callbacks, listen for all incoming requests  */
    if (osp_tmb.register_tmcb(0, 0, TMCB_REQUEST_IN, ospOnReq, 0, 0) <= 0) {
       LM_ERR("failed to register TMCB_REQUEST_IN callback\n");
       LM_ERR("TM callbacks are required for reporting call set up usage\n");
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
       LM_ERR("failed to register TM callbacks\n");
       LM_ERR("TM callbacks are required for reporting call setup usage\n");
        return;
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
    if (type & TMCB_RESPONSE_OUT) {
        LM_DBG("RESPONSE_OUT\n");
    } else if (type & TMCB_E2EACK_IN) {
        LM_DBG("E2EACK_IN\n");
    } else if (type & TMCB_ON_FAILURE_RO) {
        LM_DBG("FAILURE_RO\n");
    } else if (type & TMCB_RESPONSE_IN) {
        LM_DBG("RESPONSE_IN\n");
    } else if (type & TMCB_REQUEST_FWDED) {
        LM_DBG("REQUEST_FWDED\n");
    } else if (type & TMCB_RESPONSE_FWDED) {
        LM_DBG("RESPONSE_FWDED\n");
    } else if (type & TMCB_ON_FAILURE) {
        LM_DBG("FAILURE\n");
    } else if (type & TMCB_LOCAL_COMPLETED) {
        LM_DBG("COMPLETED\n");
    } else {
        LM_DBG("something else '%d'\n", type);
    }

    if (t) {
        ospRecordEvent(t->uac[t->nr_of_outgoings - 1].last_received,
					   t->uas.status);
    } else {
        LM_DBG("cell is empty\n");
    }
}
