/*
 * Copyright (C) 2006 Voice System SRL
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


/*!
 * \file
 * \brief Functions related to dialog handling
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef _DIALOG_DLG_HANDLERS_H_
#define _DIALOG_DLG_HANDLERS_H_

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../modules/tm/t_hooks.h"
#include "dlg_hash.h"
#include "dlg_timer.h"

#define MAX_DLG_RR_PARAM_NAME 32

/* values for the sequential match mode */
#define SEQ_MATCH_STRICT_ID  0
#define SEQ_MATCH_FALLBACK   1
#define SEQ_MATCH_NO_ID      2


/*!
 * \brief Initialize the dialog handlers
 * \param rr_param_p added record-route parameter
 * \param dlg_flag_p dialog flag
 * \param timeout_avp_p AVP for timeout setting
 * \param default_timeout_p default timeout
 * \param seq_match_mode_p matching mode
 */
void init_dlg_handlers(char *rr_param, int dlg_flag,
		pv_spec_t *timeout_avp, int default_timeout,
		int seq_match_mode);


/*!
 * \brief Shutdown operation of the module
 */
void destroy_dlg_handlers(void);


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
int populate_leg_info(dlg_cell_t *dlg, sip_msg_t *msg,
	tm_cell_t *t, unsigned int leg, str *tag);


/*!
 * \brief Function that is registered as TM callback and called on requests
 * \param t transaction, used to created the dialog
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
void dlg_onreq(tm_cell_t *t, int type, struct tmcb_params *param);


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
void dlg_onroute(sip_msg_t *req, str *rr_param, void *param);


/*!
 * \brief Timer function that removes expired dialogs, run timeout route
 * \param tl dialog timer list
 */
void dlg_ontimeout(dlg_tl_t *tl);


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
int dlg_new_dialog(sip_msg_t *req, tm_cell_t *t, const int run_initial_cbs);


/*!
 * \brief Function that returns the dialog lifetime as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_lifetime(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);


/*!
 * \brief Function that returns the dialog state as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_status(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);


/*!
 * \brief Dummy callback just to keep the compiler happy
 * \param t unused
 * \param type unused
 * \param param unused
 */
void dlg_tmcb_dummy(tm_cell_t *t, int type, struct tmcb_params *param);

/*!
 * \brief Get the dialog structure for the SIP message
 */
dlg_cell_t *dlg_get_msg_dialog(sip_msg_t *msg);

/*!
 * \brief Get the dialog structure and direction for the SIP message
 */
dlg_cell_t *dlg_lookup_msg_dialog(sip_msg_t *msg, unsigned int *dir);

/*!
 * \brief Clone dialog internal unique id to shared memory
 */
dlg_iuid_t *dlg_get_iuid_shm_clone(dlg_cell_t *dlg);

/*!
 * \brief Free dialog internal unique id stored in shared memory
 */
void dlg_iuid_sfree(void *iuid);

/*!
 *
 */
int dlg_manage(sip_msg_t *msg);

#endif
