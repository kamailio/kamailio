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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tm.h"
#include "destination.h"
#include "usage.h"
#include "../tm/tm_load.h"

struct tm_binds _tmb;

static void onreq( struct cell* t, int type, struct tmcb_params *ps );
static void tmcb_func( struct cell* t, int type, struct tmcb_params *ps );


int mod_init_tm()
{
        load_tm_f load_tm;

        INFO("osp/tm - initializing\n");

        /* import the TM auto-loading function */
        if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
                ERR("osp:mod_init_tm: can't import load_tm\n");
                ERR("osp:mod_init_tm: tm is required for reporting call set up usage info\n");
                return -1;
        }
        /* let the auto-loading function load all TM stuff */
        if (load_tm( &_tmb )==-1) return -1;

        /* register callbacks*/
        /* listen for all incoming requests  */
        if ( _tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, onreq, 0 ) <=0 ) {
                ERR("osp:mod_init_tm: cannot register TMCB_REQUEST_IN callback\n");
                ERR("osp:mod_init_tm: tm callbacks are required for reporting call set up usage info\n");
                return -1;
        }

        return 0;
}

static void onreq( struct cell* t, int type, struct tmcb_params *ps )
{
        int tmcb_types;

        DBG("osp: onreq: Registering transaction call backs\n\n");

        /* install addaitional handlers */
        tmcb_types =
		     /* TMCB_REQUEST_FWDED | */
		     /* TMCB_RESPONSE_FWDED | */
			TMCB_ON_FAILURE | 
		     /* TMCB_LOCAL_COMPLETED  | */
			/* report on completed transactions */
			TMCB_RESPONSE_OUT |
			/* account e2e acks if configured to do so */
			TMCB_E2EACK_IN |
			/* report on missed calls */
			TMCB_ON_FAILURE_RO //|
                /* get incoming replies ready for processing */
                /*TMCB_RESPONSE_IN */
		;

	if (_tmb.register_tmcb( 0, t, tmcb_types, tmcb_func, 0 )<=0) {
		ERR("osp:onreq: cannot register for tm callbacks\n");
                ERR("osp:onreq: tm callbacks are required for reporting call set up usage info\n");
		return;
	}

        /* also, if that is INVITE, disallow silent t-drop */
        if (ps->req->REQ_METHOD==METHOD_INVITE) {
                DBG("noisy_timer set for accounting\n");
                t->flags |= T_NOISY_CTIMER_FLAG;
        }
}



static void tmcb_func( struct cell* t, int type, struct tmcb_params *ps )
{
        if (type&TMCB_RESPONSE_OUT) {
                DBG("osp:tmcb: on-RESPONSE_OUT-out\n");
        } else if (type&TMCB_E2EACK_IN) {
                DBG("osp:tmcb: on-E2EACK_IN\n");
        } else if (type&TMCB_ON_FAILURE_RO) {
                DBG("osp:tmcb: on-FAILURE_RO\n");
        } else if (type&TMCB_RESPONSE_IN) {
                DBG("osp:tmcb: on-RESPONSE_IN\n");
        } else if (type&TMCB_REQUEST_FWDED) {
                DBG("osp:tmcb: on-REQUEST_FWDED\n");
        } else if (type&TMCB_RESPONSE_FWDED) {
                DBG("osp:tmcb: on-RESPONSE_FWDED\n");
        } else if (type&TMCB_ON_FAILURE) {
                DBG("osp:tmcb: on-FAILURE\n");
        } else if (type&TMCB_LOCAL_COMPLETED) {
                DBG("osp:tmcb: on-COMPLETED\n");
        } else {
                DBG("osp:tmcb: on-something-else: %d\n",type);
        }

	if (t) {
		recordEvent(t->uac[t->nr_of_outgoings-1].last_received,t->uas.status);
	} else {
                DBG("osp:tmcb: cell is empty\n");
	}
}


