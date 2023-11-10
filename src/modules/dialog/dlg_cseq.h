/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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


/*!
 * \file
 * \brief CSeq handling
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef _DLG_CSEQ_H_
#define _DLG_CSEQ_H_

#include "../../core/parser/msg_parser.h"
#include "dlg_hash.h"

int dlg_register_cseq_callbacks(void);

int dlg_cseq_update(sip_msg_t *msg);
int dlg_cseq_refresh(sip_msg_t *msg, dlg_cell_t *dlg, unsigned int direction);

#endif
