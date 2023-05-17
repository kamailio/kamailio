/*
 * Copyright (C) 2014 Andrey Rybkin <rybkin.a@bks.tv>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../modules/usrloc/usrloc.h"
#include "../usrloc/ul_callback.h"
#include "../../modules/sl/sl.h"
#include "../../core/mod_fix.h"

#include "usrloc_sync.h"

static int mod_init(void);
static int child_init(int);

int dmq_usrloc_enable = 0;
int _dmq_usrloc_sync = 1;
int _dmq_usrloc_replicate_socket_info = 0;
int _dmq_usrloc_batch_size = 0;
int _dmq_usrloc_batch_msg_contacts = 1;
int _dmq_usrloc_batch_msg_size = 60000;
int _dmq_usrloc_batch_usleep = 0;
str _dmq_usrloc_domain = str_init("location");
int _dmq_usrloc_delete = 1;

usrloc_api_t dmq_ul;

MODULE_VERSION

static param_export_t params[] = {{"enable", INT_PARAM, &dmq_usrloc_enable},
		{"sync", INT_PARAM, &_dmq_usrloc_sync},
		{"replicate_socket_info", INT_PARAM,
				&_dmq_usrloc_replicate_socket_info},
		{"batch_msg_contacts", INT_PARAM, &_dmq_usrloc_batch_msg_contacts},
		{"batch_msg_size", INT_PARAM, &_dmq_usrloc_batch_msg_size},
		{"batch_size", INT_PARAM, &_dmq_usrloc_batch_size},
		{"batch_usleep", INT_PARAM, &_dmq_usrloc_batch_usleep},
		{"usrloc_domain", PARAM_STR, &_dmq_usrloc_domain},
		{"usrloc_delete", INT_PARAM, &_dmq_usrloc_delete}, {0, 0, 0}};

struct module_exports exports = {
		"dmq_usrloc",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		0,				 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* RPC method exports */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module initialization function */
		child_init,		 /* per-child init function */
		0				 /* module destroy function */
};


static int mod_init(void)
{
	bind_usrloc_t bind_usrloc;
	LM_INFO("dmq usrloc replication mode = %d\n", dmq_usrloc_enable);

	if(_dmq_usrloc_batch_msg_size > 60000) {
		LM_ERR("batch_msg_size too high[%d] setting to [60000]\n",
				_dmq_usrloc_batch_msg_size);
		_dmq_usrloc_batch_msg_size = 60000;
	}
	if(_dmq_usrloc_batch_msg_contacts > 150) {
		LM_ERR("batch_msg_contacts too high[%d] setting to [150]\n",
				_dmq_usrloc_batch_msg_contacts);
		_dmq_usrloc_batch_msg_contacts = 150;
	}

	if(dmq_usrloc_enable) {

		bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
		if(!bind_usrloc) {
			LM_ERR("can't bind usrloc\n");
			return -1;
		}
		if(bind_usrloc(&dmq_ul) < 0) {
			LM_ERR("Can't bind ul\n");
			return -1;
		}
		if(dmq_ul.register_ulcb != NULL) {
			if(dmq_ul.register_ulcb(ULCB_MAX, dmq_ul_cb_contact, 0) < 0) {
				LM_ERR("can not register callback for expired contacts\n");
				return -1;
			}
		}
		if(!usrloc_dmq_initialize()) {
			LM_DBG("dmq_usrloc initialized\n");
		} else {
			LM_ERR("Error in dmq_usrloc_initialize()\n");
		}
	}
	return 0;
}

static int child_init(int rank)
{

	if(rank == PROC_MAIN) {
		LM_DBG("child_init PROC_MAIN\n");
		return 0;
	}
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN) {
		LM_DBG("child_init PROC_INIT\n");
		return 0;
	}
	return 0;
}
