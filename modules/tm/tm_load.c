/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History:
 * --------
 * 2003-03-06  voicemail changes accepted (jiri)
 * 2003-04-14  t_relay_to split in udp and tcp (jiri)
 */

#include "defs.h"


#include "tm_load.h"
#include "uac.h"

#define LOAD_ERROR "ERROR: tm_bind: TM module function "

int tm_init = 0;

int load_tm( struct tm_binds *tmb)
{
	if (!tm_init) {
		LOG(L_ERR, "tm:load_tm: Module not initialized yet, make sure that all modules that need"
		    " tm module are loaded after tm in the configuration file\n");
		return -1;
	}

	memset(tmb, 0, sizeof(struct tm_binds));
	if (!( tmb->register_tmcb=(register_tmcb_f) 
		find_export("register_tmcb", NO_SCRIPT, 0)) ) {
		LOG(L_ERR, LOAD_ERROR "'register_tmcb' not found\n");
		return -1;
	}
	if (!( tmb->t_newtran=(tnewtran_f)find_export("t_newtran", 0, 0)) ) {
		LOG(L_ERR, LOAD_ERROR "'t_newtran' not found\n");
		return -1;
	}
#ifdef USE_TCP
	if (!( tmb->t_relay_to_tcp=find_export(T_RELAY_TO_TCP, 2, 0)) ) {
		LOG(L_ERR, LOAD_ERROR "'t_relay_to_tcp' not found\n");
		return -1;
	}
#endif
	if (!( tmb->t_relay_to_udp=find_export(T_RELAY_TO_UDP, 2, 0)) ) {
		LOG(L_ERR, LOAD_ERROR "'t_relay_to_udp' not found\n");
		return -1;
	}
	if (!( tmb->t_relay=find_export(T_RELAY, 0, 0)) ) {
		LOG(L_ERR, LOAD_ERROR "'t_relay' not found\n");
		return -1;
	}
/*	if (!(tmb->t_reply=(treply_f)find_export(T_REPLY, 2, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_reply' not found\n");
		return -1;
	}*/
	tmb->t_reply = w_t_reply_wrp;
	
	if (!(tmb->t_reply_with_body=(treply_wb_f)find_export
	(T_REPLY_WB, NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_reply_with_body' not found\n");
		return -1;
	}
	if (!(tmb->t_is_local=(tislocal_f)find_export(T_IS_LOCAL,NO_SCRIPT,0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_get_trans_ident' not found\n");
		return -1;
	}
	if (!(tmb->t_get_trans_ident=(tget_ti_f)find_export
	(T_GET_TI, NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_get_trans_ident' not found\n");
		return -1;
	}
	if (!(tmb->t_lookup_ident=(tlookup_ident_f)find_export
	(T_LOOKUP_IDENT, NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_lookup_ident' not found\n");
		return -1;
	}
	if (!(tmb->t_addblind=(taddblind_f)find_export(T_ADDBLIND,NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "'addblind' not found\n");
		return -1;
	}
	if (!(tmb->t_forward_nonack=(tfwd_f)find_export(T_FORWARD_NONACK,2,0))) {
		LOG( L_ERR, LOAD_ERROR "'t_forward_nonack' not found\n");
		return -1;
	}
	if (!(tmb->t_request_within=(reqwith_t)find_export
	("t_request_within", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_request_within' not found\n");
		return -1;
	}
	if (!(tmb->t_request_outside=(reqout_t)find_export
	("t_request_outside", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_request_outside' not found\n");
		return -1;
	}
	if (!(tmb->t_request=(req_t)find_export("t_request", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_request' not found\n");
		return -1;
	}
	if (!(tmb->new_dlg_uac=(new_dlg_uac_f)find_export
	("new_dlg_uac", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'new_dlg_uac' not found\n");
		return -1;
	}
	if (!(tmb->dlg_response_uac=(dlg_response_uac_f)find_export
	("dlg_response_uac", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'dlg_response_uac' not found\n");
		return -1;
	}
	if (!(tmb->new_dlg_uas=(new_dlg_uas_f)find_export
	("new_dlg_uas", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'new_dlg_uas' not found\n");
		return -1;
	}
	if (!(tmb->update_dlg_uas=(update_dlg_uas_f)find_export
	("update_dlg_uas", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'update_dlg_uas' not found\n");
		return -1;
	}
	if (!(tmb->dlg_request_uas=(dlg_request_uas_f)find_export
	("dlg_request_uas", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'dlg_request_uas' not found\n");
		return -1;
	}
	if (!(tmb->set_dlg_target=(set_dlg_target_f)find_export
	("set_dlg_target", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'set_dlg_target' not found\n");
		return -1;
	}
	if (!(tmb->free_dlg=(free_dlg_f)find_export("free_dlg", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'free_dlg' not found\n");
		return -1;
	}
	if (!(tmb->print_dlg=(print_dlg_f)find_export("print_dlg",NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "'print_dlg' not found\n");
		return -1;
	}
	if (!(tmb->t_gett=(tgett_f)find_export(T_GETT,NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "'" T_GETT "' not found\n");
		return -1;
	}
	if (!(tmb->calculate_hooks=(calculate_hooks_f)find_export("calculate_hooks",NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "' calculate_hooks ' not found\n");
		return -1;
	}
	if (!(tmb->t_uac=(t_uac_t)find_export("t_uac", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_uac' not found\n");
		return -1;
	}
	if (!(tmb->t_uac_with_ids=(t_uac_with_ids_t)find_export("t_uac_with_ids", NO_SCRIPT, 0)) ) {
		LOG( L_ERR, LOAD_ERROR "'t_uac_with_ids' not found\n");
		return -1;
	}
	if (!(tmb->t_release=(trelease_f)find_export("t_release",0,0))) {
		LOG( L_ERR, LOAD_ERROR "'t_release' not found\n");
		return -1;
	}
	if (!(tmb->t_unref=(tunref_f)find_export("t_unref",NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "'t_unref' not found\n");
		return -1;
	}
	if (!(tmb->run_failure_handlers=
	    (run_failure_handlers_f)find_export("run_failure_handlers",NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "'run_failure_handlers' not found\n");
		return -1;
	}
	if (!(tmb->cancel_uacs=(cancel_uacs_f)find_export("cancel_uacs",NO_SCRIPT,0))) {
		LOG( L_ERR, LOAD_ERROR "'cancel_uacs' not found\n");
		return -1;
	}
	if (! (tmb->cancel_all_uacs=(cancel_all_uacs_f)find_export(
			"cancel_all_uacs", NO_SCRIPT, 0))) {
		LOG( L_ERR, LOAD_ERROR "'cancel_all_uacs' not found\n");
		return -1;
	}

	tmb->prepare_request_within = prepare_req_within;
	tmb->send_prepared_request = send_prepared_request;
	
#ifdef DIALOG_CALLBACKS
	tmb->register_new_dlg_cb=register_new_dlg_cb;
	tmb->register_dlg_tmcb=register_dlg_tmcb;
#endif
#ifdef WITH_AS_SUPPORT
	if (! (tmb->ack_local_uac=(ack_local_uac_f)find_export("ack_local_uac",
			NO_SCRIPT, 0))) {
		LOG( L_ERR, LOAD_ERROR "'ack_uac' not found\n");
		return -1;
	}
	if (! (tmb->t_get_canceled_ident=(t_get_canceled_ident_f)find_export(
			"t_get_canceled_ident", NO_SCRIPT, 0))) {
		LOG( L_ERR, LOAD_ERROR "'t_get_canceled_ident' not found\n");
		return -1;
	}
#endif
	if (! (tmb->t_suspend=(t_suspend_f)find_export("t_suspend",
			NO_SCRIPT, 0))) {
		LOG( L_ERR, LOAD_ERROR "'t_suspend' not found\n");
		return -1;
	}
	if (! (tmb->t_continue=(t_continue_f)find_export("t_continue",
			NO_SCRIPT, 0))) {
		LOG( L_ERR, LOAD_ERROR "'t_continue' not found\n");
		return -1;
	}

	tmb->t_get_reply_totag = t_get_reply_totag;
	tmb->t_get_picked_branch = t_get_picked_branch;
	tmb->t_lookup_callid = t_lookup_callid;
	tmb->generate_callid = generate_callid;
	tmb->generate_fromtag = generate_fromtag;
	tmb->t_lookup_request = t_lookup_request;
	tmb->t_lookup_original = t_lookupOriginalT;
	tmb->t_check = t_check;
	tmb->unref_cell = unref_cell;
	tmb->prepare_to_cancel = prepare_to_cancel;
	tmb->get_stats = tm_get_stats;
	tmb->get_table = tm_get_table;

#ifdef WITH_TM_CTX
	tmb->tm_ctx_get = tm_ctx_get;
#endif
	return 1;
}
