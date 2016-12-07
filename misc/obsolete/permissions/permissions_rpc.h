/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _PERMISSIONS_RPC_H
#define _PERMISSIONS_RPC_H

#include "../../rpc.h"
#include "trusted_rpc.h"
#include "im_rpc.h"
#include "ip_set_rpc.h"

rpc_export_t permissions_rpc[] = {
	{"trusted.reload", trusted_reload, trusted_reload_doc, 0},
	{"trusted.dump",   trusted_dump,   trusted_dump_doc,   RET_ARRAY},
	{"ipmatch.reload", im_reload,      im_reload_doc, 0},
	{"ipset.clean",    rpc_ip_set_clean, rpc_ip_set_clean_doc, 0},
	{"ipset.add",      rpc_ip_set_add, rpc_ip_set_add_doc, 0},
	{"ipset.commit",   rpc_ip_set_commit, rpc_ip_set_commit_doc, 0},
	{"ipset.list",     rpc_ip_set_list, rpc_ip_set_list_doc, RET_ARRAY},
	{"ipset.print",    rpc_ip_set_print, rpc_ip_set_print_doc, RET_ARRAY},
	{0, 0, 0, 0}
};

#endif /* _PERMISSIONS_RPC_H */
