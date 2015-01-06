/**
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*!
 * \file
 * \brief Call transfer
 * \ingroup dialog
 * Module: \ref dialog
 */
		       
#ifndef _DLG_TRANSFER_H_
#define _DLG_TRANSFER_H_

#include "dlg_hash.h"

typedef struct _dlg_transfer_ctx {
	int state;
	str from;
	str to;
	struct dlg_cell *dlg;
} dlg_transfer_ctx_t;

int dlg_bridge(str *from, str *to, str *op, str *bd);
int dlg_transfer(struct dlg_cell *dlg, str *to, int side);
int dlg_bridge_init_hdrs(void);
void dlg_bridge_destroy_hdrs(void);

#endif
