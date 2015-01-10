/*
 * TLS module - select interface
 *
 * Copyright (C) 2005,2006 iptelorg GmbH
 * Copyright (C) 2006 enum.at
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 */
/*!
 * \file
 * \brief Kamailio TLS support :: select interface
 * \ingroup tls
 * Module: \ref tls
 */


#ifndef _TLS_SELECT_H
#define _TLS_SELECT_H

#include "../../select.h"
#include "../../pvar.h"
#include "../../tcp_conn.h"

extern select_row_t tls_sel[];

extern pv_export_t tls_pv[];

void tls_set_pv_con(struct tcp_connection *c);

#endif /* _TLS_SELECT_H */
