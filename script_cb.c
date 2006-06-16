/*
 * $Id$
 *
 * Script callbacks -- they add the ability to register callback
 * functions which are always called when script for request
 * processing is entered or left
 *
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
 *  2003-03-29  cleaning pkg allocation introduced (jiri)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2005-02-13  script callbacks devided into request and reply types (bogdan)
 */


#include <stdlib.h>
#include "script_cb.h"
#include "dprint.h"
#include "error.h"
#include "mem/mem.h"

static struct script_cb *pre_req_cb=0;
static struct script_cb *post_req_cb=0;

static struct script_cb *pre_rpl_cb=0;
static struct script_cb *post_rpl_cb=0;

static unsigned int cb_id=0;


static inline int add_callback( struct script_cb **list,
	cb_function f, void *param)
{
	struct script_cb *new_cb;

	new_cb=pkg_malloc(sizeof(struct script_cb));
	if (new_cb==0) {
		LOG(L_ERR, "ERROR:add_script_callback: out of memory\n");
		return -1;
	}
	new_cb->cbf = f;
	new_cb->id = cb_id++;
	new_cb->param = param;
	/* link at the beginning of the list */
	new_cb->next = *list;
	*list = new_cb;
	return 0;
}


int register_script_cb( cb_function f, int type, void *param )
{
	/* type checkings */
	if ( (type&(REQ_TYPE_CB|RPL_TYPE_CB))==0 ) {
		LOG(L_CRIT,"BUG:register_script_cb: REQUEST or REPLY "
			"type not specified\n");
		goto error;
	}
	if ( (type&(PRE_SCRIPT_CB|POST_SCRIPT_CB))==0 ||
	(type&PRE_SCRIPT_CB && type&POST_SCRIPT_CB) ) {
		LOG(L_CRIT,"BUG:register_script_cb: callback POST or PRE type must "
			"be exactly one\n");
		goto error;
	}

	if (type&REQ_TYPE_CB) {
		/* callback for request script */
		if (type&PRE_SCRIPT_CB) {
			if (add_callback( &pre_req_cb, f, param)<0)
				goto add_error;
		} else if (type&POST_SCRIPT_CB) {
			if (add_callback( &post_req_cb, f, param)<0)
				goto add_error;
		}
	}
	if (type&RPL_TYPE_CB) {
		/* callback (also) for reply script */
		if (type&PRE_SCRIPT_CB) {
			if (add_callback( &pre_rpl_cb, f, param)<0)
				goto add_error;
		} else if (type&POST_SCRIPT_CB) {
			if (add_callback( &post_rpl_cb, f, param)<0)
				goto add_error;
		}
	}

	return 0;
add_error:
	LOG(L_ERR,"ERROR:register_script_cb: failed to add callback\n");
error:
	return -1;
}


static inline void destroy_cb_list(struct script_cb **list)
{
	struct script_cb *foo;

	while( *list ) {
		foo = *list;
		*list = (*list)->next;
		pkg_free( foo );
	}
}


void destroy_script_cb()
{
	destroy_cb_list( &pre_req_cb  );
	destroy_cb_list( &post_req_cb );
	destroy_cb_list( &pre_rpl_cb  );
	destroy_cb_list( &post_req_cb );
}


static inline int exec_pre_cb( struct sip_msg *msg, struct script_cb *cb)
{
	for ( ; cb ; cb=cb->next ) {
		/* stop on error */
		if (cb->cbf(msg, cb->param)==0)
			return 0;
	}
	return 1;
}


static inline int exec_post_cb( struct sip_msg *msg, struct script_cb *cb)
{
	for ( ; cb ; cb=cb->next){
		cb->cbf( msg, cb->param);
	}
	return 1;
}


int exec_pre_req_cb( struct sip_msg *msg)
{
	return exec_pre_cb( msg, pre_req_cb);
}

int exec_pre_rpl_cb( struct sip_msg *msg)
{
	return exec_pre_cb( msg, pre_rpl_cb);
}

int exec_post_req_cb( struct sip_msg *msg)
{
	return exec_post_cb( msg, post_req_cb);
}

int exec_post_rpl_cb( struct sip_msg *msg)
{
	return exec_post_cb( msg, post_rpl_cb);
}

