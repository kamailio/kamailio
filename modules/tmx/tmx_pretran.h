/*
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*! \file
 * \brief TMX :: pretran
 *
 * \ingroup tm
 * - Module: \ref tm
 */
		       
#ifndef _TMX_PRETRANS_H_
#define _TMX_PRETRANS_H_

#include "../../parser/msg_parser.h"

int tmx_init_pretran_table(void);
int tmx_check_pretran(sip_msg_t *msg);
void tmx_pretran_unlink(void);

#endif
