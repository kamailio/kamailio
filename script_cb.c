/*
 * Script callbacks -- they add the ability to register callback
 * functions which are always called when script for request
 * processing is entered or left
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief Kamailio core :: Script callbacks 
 *
 * Script callbacks adds the ability to register callback
 * functions which are always called when script for request
 * processing is entered or left
 * \ingroup core
 * Module: \ref core
 */


#include <stdlib.h>
#include "script_cb.h"
#include "dprint.h"
#include "error.h"
#include "mem/mem.h"

/* Number of cb types = last cb type */
#define SCRIPT_CB_NUM	(MAX_CB_TYPE-1)

static struct script_cb *pre_script_cb[SCRIPT_CB_NUM];
static struct script_cb *post_script_cb[SCRIPT_CB_NUM];

/* Add a script callback to the beginning of the linked list.
 * Returns -1 on error
 */
static inline int add_callback( struct script_cb **list,
	cb_function f, void *param)
{
	struct script_cb *new_cb;

	new_cb=pkg_malloc(sizeof(struct script_cb));
	if (new_cb==0) {
		LM_CRIT("out of memory\n");
		return -1;
	}
	new_cb->cbf = f;
	new_cb->param = param;
	/* link at the beginning of the list */
	new_cb->next = *list;
	*list = new_cb;
	return 0;
}

/* Register pre- or post-script callbacks.
 * Returns -1 on error, 0 on success
 */
int register_script_cb( cb_function f, unsigned int flags, void *param )
{
	struct script_cb	**cb_array;
	int	i;

	/* type checkings */
	if ( (flags&((1<<SCRIPT_CB_NUM)-1))==0 ) {
		LOG(L_BUG, "register_script_cb: callback flag not specified\n");
		return -1;
	}
	if ( (flags&(~(PRE_SCRIPT_CB|POST_SCRIPT_CB))) >= 1<<SCRIPT_CB_NUM ) {
		LOG(L_BUG, "register_script_cb: unsupported callback flags: %u\n",
			flags);
		return -1;
	}
	if ( (flags&(PRE_SCRIPT_CB|POST_SCRIPT_CB))==0 ||
	(flags&PRE_SCRIPT_CB && flags&POST_SCRIPT_CB) ) {
		LOG(L_BUG, "register_script_cb: callback POST or PRE type must "
			"be exactly one\n");
		return -1;
	}

	if (flags&PRE_SCRIPT_CB)
		cb_array = pre_script_cb;
	else
		cb_array = post_script_cb;

	/* Add the callback to the lists.
	 * (as many times as many flags are set)
	 */
	for (i=0; i<SCRIPT_CB_NUM; i++) {
		if ((flags&(1<<i)) == 0)
			continue;
		if (add_callback(&cb_array[i], f, param) < 0)
			goto add_error;
	}
	return 0;

add_error:
	LM_ERR("failed to add callback\n");
	return -1;
}

int init_script_cb()
{
	memset(pre_script_cb, 0, SCRIPT_CB_NUM * sizeof(struct script_cb *));
	memset(post_script_cb, 0, SCRIPT_CB_NUM * sizeof(struct script_cb *));
	return 0;
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
	int	i;

	for (i=0; i<SCRIPT_CB_NUM; i++) {
		destroy_cb_list(&pre_script_cb[i]);
		destroy_cb_list(&post_script_cb[i]);
	}
}

/* Execute pre-script callbacks of a given type.
 * Returns 0 on error, 1 on success
 */
int exec_pre_script_cb( struct sip_msg *msg, enum script_cb_type type)
{
	struct script_cb	*cb;
	unsigned int	flags;

#ifdef EXTRA_DEBUG
	if (type > SCRIPT_CB_NUM) {
		LOG(L_BUG, "Uknown callback type\n");
		abort();
	}
#endif
	flags = PRE_SCRIPT_CB | (1<<(type-1));
	for (cb=pre_script_cb[type-1]; cb ; cb=cb->next ) {
		/* stop on error */
		if (cb->cbf(msg, flags, cb->param)==0)
			return 0;
	}
	return 1;
}

/* Execute post-script callbacks of a given type.
 * Always returns 1, success.
 */
int exec_post_script_cb( struct sip_msg *msg, enum script_cb_type type)
{
	struct script_cb	*cb;
	unsigned int	flags;

#ifdef EXTRA_DEBUG
	if (type > SCRIPT_CB_NUM) {
		LOG(L_BUG, "exec_pre_script_cb: Uknown callback type\n");
		abort();
	}
#endif
	flags = POST_SCRIPT_CB | (1<<(type-1));
	for (cb=post_script_cb[type-1]; cb ; cb=cb->next){
		cb->cbf(msg, flags, cb->param);
	}
	return 1;
}
