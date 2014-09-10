/*
 * Copyright (C) 2014 Federico Cabiddu (federico.cabiddu@gmail.com)
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

#ifndef _T_TS_HANDLERS_H
#define _T_TS_HANDLERS_H

#include "../../modules/tm/t_hooks.h"

/*!
 * \brief add transaction structure to tm callbacks
 * \param t current transaction
 * \param req current sip request
 * \param tma_t current transaction
 * \return 0 on success, -1 on failure
 */
int ts_set_tm_callbacks(struct cell *t, sip_msg_t *req, ts_transaction_t *ts);

/*
 *
 */
void ts_onreply(struct cell* t, int type, struct tmcb_params *param);

#endif
