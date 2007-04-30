/*
 * $Id$
 *
 * Copyright (C) 2006 Voice System SRL
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
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2006-11-28  Added num_100s and num_200s to dlg_cell, to aid in adding 
 *             statistics tracking of the number of early, and active dialogs.
 *             (Jeffrey Magder - SOMA Networks)
 * 2007-03-06  syncronized state machine added for dialog state. New tranzition
 *             design based on events; removed num_1xx and num_2xx (bogdan)
 */


#ifndef _DIALOG_DLG_HASH_H_
#define _DIALOG_DLG_HASH_H_

#include "../../locking.h"
#include "dlg_timer.h"
#include "dlg_cb.h"


#define DLG_STATE_UNCONFIRMED  1
#define DLG_STATE_EARLY        2
#define DLG_STATE_CONFIRMED_NA 3
#define DLG_STATE_CONFIRMED    4
#define DLG_STATE_DELETED      5

#define DLG_EVENT_TDEL         1
#define DLG_EVENT_RPL1xx       2
#define DLG_EVENT_RPL2xx       3
#define DLG_EVENT_RPL3xx       4
#define DLG_EVENT_REQACK       5
#define DLG_EVENT_REQBYE       6
#define DLG_EVENT_REQ          7

struct dlg_cell
{
	volatile int         ref;
	struct dlg_cell      *next;
	struct dlg_cell      *prev;
	unsigned int         h_id;
	unsigned int         h_entry;
	unsigned int         state;
	unsigned int         lifetime;
	unsigned int         start_ts;
	struct dlg_tl        tl;
	str                  callid;
	str                  from_uri;
	str                  to_uri;
	str                  from_tag;
	str                  to_tag;
	struct dlg_head_cbl  cbs;
};


struct dlg_entry
{
	struct dlg_cell    *first;
	struct dlg_cell    *last;
	unsigned int        next_id;
	gen_lock_set_t     *lock_set;
	unsigned int       lock_idx;
};



struct dlg_table
{
	unsigned int       size;
	struct dlg_entry   *entries;
	unsigned int       locks_no;
	gen_lock_set_t     *locks;
};




int init_dlg_table(unsigned int size);

void destroy_dlg_table();

struct dlg_cell* build_new_dlg(str *callid, str *from_uri,
		str *to_uri, str *from_tag);

int dlg_set_totag(struct dlg_cell *dlg, str *tag);

struct dlg_cell* lookup_dlg( unsigned int h_entry, unsigned int h_id);

struct dlg_cell* get_dlg( str *callid, str *ftag, str *ttag);

void link_dlg(struct dlg_cell *dlg, int n);

void unref_dlg(struct dlg_cell *dlg, int cnt);

void next_state_dlg(struct dlg_cell *dlg, int event,
		int *old_state, int *new_state, int *unref);

struct mi_root * mi_print_dlgs(struct mi_root *cmd, void *param );

static inline int match_dialog(struct dlg_cell *dlg, str *callid,
													str *ftag, str *ttag ) {
	if (dlg->callid.len!=callid->len ||
	dlg->from_tag.len!=ftag->len ||
	dlg->to_tag.len!=ttag->len ||
	strncmp(dlg->callid.s,callid->s,callid->len)!=0 ||
	strncmp(dlg->from_tag.s,ftag->s,ftag->len)!=0 ||
	strncmp(dlg->to_tag.s,ttag->s,ttag->len)!=0)
		return 0;
	return 1;
}

#endif
