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
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2007-03-06  syncronized state machine added for dialog state. New tranzition
 *             design based on events; removed num_1xx and num_2xx (bogdan)
 * 2011-10     added support for forked calls (richard and jason)
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
#include "dlg_timer.h"

#define MI_DIALOG_NOT_FOUND 		"Requested Dialog not found"
#define MI_DIALOG_NOT_FOUND_LEN 	(sizeof(MI_DIALOG_NOT_FOUND)-1)
#define MI_DLG_OPERATION_ERR		"Operation failed"
#define MI_DLG_OPERATION_ERR_LEN	(sizeof(MI_DLG_OPERATION_ERR)-1)

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
int populate_leg_info( struct dlg_cell *dlg, struct sip_msg *msg,
	struct cell* t, unsigned int leg, str *tag);


/*!
 * \brief Function that is registered as TM callback and called on requests
 * \param t transaction, used to created the dialog
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
void dlg_onreq(struct cell* t, int type, struct tmcb_params *param);


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
void dlg_onroute(struct sip_msg* req, str *rr_param, void *param);


/*!
 * \brief Timer function that removes expired dialogs, run timeout route
 * \param tl dialog timer list
 */
void dlg_ontimeout( struct dlg_tl *tl);


/*!
 * \brief Create a new dialog from a sip message
 *
 * Create a new dialog from a SIP message, register a callback
 * to keep track of the dialog with help of the tm module.
 * This function is either called from the request callback, or
 * from the dlg_manage function in the configuration script.
 * \see dlg_onreq
 * \see w_dlg_manage
 * \param msg SIP message
 * \param t transaction
 * \return 0 on success, -1 on failure
 */ 
int dlg_new_dialog(struct sip_msg *msg, struct cell *t, const int run_initial_cbs);


/*!
 * \brief Function that returns the dialog lifetime as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_lifetime(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);


/*!
 * \brief Function that returns the dialog state as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_status(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);


/*!
 * \brief Dummy callback just to keep the compiler happy
 * \param t unused
 * \param type unused
 * \param param unused
 */
void dlg_tmcb_dummy(struct cell* t, int type, struct tmcb_params *param);

/*!
 * \brief Helper function that prints information for all dialogs
 * \return void
 */

void print_all_dlgs();

/*!
 * \brief Helper function that prints all the properties of a dialog including all the dlg_out's
 * \param dlg dialog cell
 * \return void
 */
void internal_print_all_dlg(struct dlg_cell *dlg);

/*!
 * \get the current dialog based on the current SIP message
 * \param msg SIP message
 * \return current dialog, null if none.
 */
struct dlg_cell *dlg_get_msg_dialog(sip_msg_t *msg);

#endif
