/*
 * $Id$
 *
 * Copyright (C) 2007 Voice System SRL
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
 * 2007-07-10  initial version (ancuta)
*/

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../socket_info.h"
#include "../tm/dlg.h"
#include "../tm/tm_load.h"
#include "../../mi/tree.h"
#include "dlg_hash.h"
#include "dlg_req_within.h"



int free_tm_dlg(dlg_t *td)
{
	if(td)
	{
		if(td->route_set)
			free_rr(&td->route_set);
		pkg_free(td);
	}
	return 0;
}



dlg_t * build_dlg_t(struct dlg_cell * cell, int dir){

	dlg_t* td = NULL;
	str cseq;
	unsigned int loc_seq;

	td = (dlg_t*)pkg_malloc(sizeof(dlg_t));
	if(!td){
	
		LM_ERR("out of pkg memory\n");
		return NULL;
	}
	memset(td, 0, sizeof(dlg_t));

	/*local sequence number*/
	cseq = (dir == DLG_CALLER_LEG) ?	cell->cseq[DLG_CALLEE_LEG]:
										cell->cseq[DLG_CALLEE_LEG];
	if(str2int(&cseq, &loc_seq) != 0){
		LM_ERR("invalid cseq\n");
		goto error;
	}
	td->loc_seq.value = loc_seq+1;
	td->loc_seq.is_set = 1;

	/*route set*/
	if( cell->route_set[dir].s && cell->route_set[dir].len){
		
		if( parse_rr_body(cell->route_set[dir].s, cell->route_set[dir].len, 
						&td->route_set) !=0){
		 	LM_ERR("failed to parse route set\n");
			goto error;
		}
	} 

	/*remote target--- Request URI*/
	if(cell->contact[dir].s==0 || cell->contact[dir].len==0){

		LM_ERR("no contact available\n");
		goto error;
	}
	td->rem_target = cell->contact[dir];

	td->rem_uri	=   (dir == DLG_CALLER_LEG)?	cell->from_uri: cell->to_uri;
	td->loc_uri	=	(dir == DLG_CALLER_LEG)?	cell->to_uri: cell->from_uri;
	td->id.call_id = cell->callid;
	td->id.rem_tag = cell->tag[dir];
	td->id.loc_tag = (dir == DLG_CALLER_LEG) ? 	cell->tag[DLG_CALLEE_LEG]:
												cell->tag[DLG_CALLER_LEG];
	
	td->state= DLG_CONFIRMED;
	td->send_sock = cell->bind_addr[dir];

	return td;

error:
	free_tm_dlg(td);
	return NULL;
}



/*callback function to handle responses to the BYE request */
void bye_reply_cb(struct cell* t, int type, struct tmcb_params* ps){

	struct dlg_cell* dlg;
	int event, old_state, new_state, unref;

	if(ps->param == NULL || *ps->param == NULL){
		LM_ERR("invalid parameter\n");
		return;
	}
	
	if(ps->code != 200){
		LM_DBG("receiving a non 200 reply\n");
		return;
	}
	
	LM_DBG("receiving a 200 reply\n");
		
	dlg = (struct dlg_cell *)(*(ps->param));
	event = DLG_EVENT_REQBYE;
	next_state_dlg(dlg, event, &old_state, &new_state, &unref);

	
	if(new_state == DLG_STATE_DELETED && old_state != DLG_STATE_DELETED){

		LM_DBG("removing dialog with h_entry %u and h_id %u\n", 
			dlg->h_entry, dlg->h_id);
			
		/* remove from timer */
		remove_dlg_timer(&dlg->tl);

		/* dialog terminated (BYE) */
		run_dlg_callbacks( DLGCB_TERMINATED, dlg, ps->req);

		LM_DBG("first 200 ok reply\n");
		/* derefering the dialog */
		unref_dlg(dlg, unref+2);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
	}

	if(new_state == DLG_STATE_DELETED && old_state == DLG_STATE_DELETED ){
	
		LM_DBG("second 200 ok reply\n");
		unref_dlg(dlg, 1);
	}

}



int build_extra_hdr(struct dlg_cell * cell, int dir, str ** hdr){

	str * str_hdr = NULL;
	int len;

	str_hdr = (str *)pkg_malloc(sizeof(str));
	if(!str_hdr){
		LM_ERR("out of pkg memory");
		goto error;
	}
	str_hdr->s = 0;
	str_hdr->len = 0;

	str_hdr->s = (char*)pkg_malloc(MAX_SIZE* sizeof(char));
	if(!str_hdr->s){
		LM_ERR("out of pkg memory\n");

		goto error;
	}
	
	strncpy(str_hdr->s ,"Max-Forwards: ", 14);
	str_hdr->len = 14;
	len= sprintf(str_hdr->s+str_hdr->len, "%d", MAX_FWD);
	if(len<= 0){
		LM_ERR("failed to set MAX_FWD\n");
		goto error;
	}
	
	str_hdr->len += len;
	strncpy(str_hdr->s + str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	*hdr = str_hdr;
	return 0;

error: 
	if(str_hdr)
		pkg_free(str_hdr);

	if(str_hdr->s)
		pkg_free(str_hdr->s);

	return -1;
}



/* cell- pointer to a struct dlg_cell
 * dir- direction: the request will be sent to:
 * 		DLG_CALLER_LEG (0): caller
 * 		DLG_CALLEE_LEG (1): callee
 */
int send_bye(struct dlg_cell * cell, int dir){

	/*verify direction*/
	dlg_t* dialog_info;
	str met = {"BYE", 3};
	str* str_hdr = NULL;
	struct dlg_entry *d_entry;
	int result;
	
	if((dialog_info = build_dlg_t(cell, dir)) == 0){
		LM_ERR("failed to create dlg_t\n");
		goto err;
	}

	if((build_extra_hdr(cell, dir, &str_hdr)) != 0){
		LM_ERR("failed to create extra headers\n");
		goto err;
	}

	LM_DBG("sending bye to %s\n", (dir==0)?"caller":"callee");

	d_entry = &(d_table->entries[cell->h_entry]);
	dlg_lock( d_table, d_entry);
	ref_dlg_unsafe(cell, 1);
	dlg_unlock( d_table, d_entry);

	result = d_tmb.t_request_within
		(&met,         /* method*/
		str_hdr,       /* extra headers*/
		NULL,          /* body*/
		dialog_info,   /* dialog structure*/
		bye_reply_cb,  /* callback function*/
		(void*)cell);  /* callback parameter*/

	if(result < 0){
		LM_ERR("failed to send the bye request\n");
		goto err1;
	}

	pkg_free(dialog_info);
	pkg_free(str_hdr);

	LM_DBG("bye sent to %s\n", (dir==0)?"caller":"callee");
	return 0;

err1:
	unref_dlg(cell, 1);
err:
	if(dialog_info){
		free_tm_dlg(dialog_info);
	}

	if(str_hdr){
		pkg_free(str_hdr);	
	}
	return -1;
}



/*parameters from MI: h_entry, h_id of the requested dialog*/
struct mi_root * mi_terminate_dlg(struct mi_root *cmd_tree, void *param ){

	struct mi_node* node;
	unsigned int h_entry, h_id;
	struct dlg_cell * dlg = NULL;


	if( d_table ==NULL)
		goto end;

	node = cmd_tree->node.kids;
	if(node == NULL || node->value.s == NULL || node->value.len ==0)
		goto error;
	
	if(strno2int(&node->value, &h_entry) <0 )
		goto error;

		node = node->next;
	if(node == NULL || node->value.s == NULL || node->value.len == 0)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if( strno2int( &node->value, &h_id) <0)
		goto error;

	LM_DBG("h_entry %u h_id %u\n", h_entry, h_id);

	dlg = lookup_dlg(h_entry, h_id);

	if(dlg){

		if((send_bye(dlg,DLG_CALLER_LEG)!=0)||(send_bye(dlg,DLG_CALLEE_LEG)!=0))

			return init_mi_tree(500, MI_DLG_OPERATION_ERR, MI_DLG_OPERATION_ERR_LEN);

		return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	}

end:

	return init_mi_tree(404, MI_DIALOG_NOT_FOUND, MI_DIALOG_NOT_FOUND_LEN);
	
error:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

}
