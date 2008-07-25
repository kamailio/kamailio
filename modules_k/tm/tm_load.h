/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * History:
 * --------
 * 2003-03-06  voicemail changes accepted
 * 2005-05-30  light version of tm_load() - no find_export() (bogdan)
 * 2007-01-25  t_forward_nonack removed as it merged into t_relay,
 *             t_replicate completely removed (bogdan)
 * 2008-04-04  added support for local and remote dispaly name in TM dialogs
 *             (by Andrei Pisau <andrei.pisau at voice-system dot ro> )
 *
 */

/*! \file
 * \brief TM :: ??
 *
 * \ingroup tm
 * - Module: \ref tm
 */

#ifndef _TM_BIND_H
#define _TM_BIND_H

#include "../../sr_module.h"
#include "t_hooks.h"
#include "uac.h"
#include "t_fwd.h"
#include "t_reply.h"
#include "t_lookup.h"
#include "t_cancel.h"
#include "dlg.h"
#include "h_table.h"


struct tm_binds {
	register_tmcb_f  register_tmcb;
	cmd_function     t_relay;
	tnewtran_f       t_newtran;
	treply_f         t_reply;
	treply_wb_f      t_reply_with_body;
	tislocal_f       t_is_local;
	tget_ti_f        t_get_trans_ident;
	tlookup_ident_f  t_lookup_ident;
	taddblind_f      t_addblind;
	treply_f         t_reply_unsafe;
	reqwith_t        t_request_within;
	reqout_t         t_request_outside;
	req_t            t_request;
	new_dlg_uac_f      new_dlg_uac;
	dlg_add_extra_f    dlg_add_extra;
	dlg_response_uac_f dlg_response_uac;
	new_dlg_uas_f      new_dlg_uas;
	dlg_request_uas_f  dlg_request_uas;
	free_dlg_f         free_dlg;
	print_dlg_f        print_dlg;
	tgett_f            t_gett;
	tget_picked_f      t_get_picked;
	tunrefcell_f       unref_cell;
	tlookuporiginalt_f t_lookup_original_t;
	tuaccancel_f       t_cancel_uac;
	setkr_f t_setkr;
};


typedef int(*load_tm_f)( struct tm_binds *tmb );
int load_tm( struct tm_binds *tmb);


static inline int load_tm_api( struct tm_binds *tmb )
{
	load_tm_f load_tm;

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", 0, 0))) {
		LM_ERR("failed to import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( tmb )==-1)
		return -1;

	return 0;
}


#endif
