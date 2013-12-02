/*
 * TLS module - select interface
 *
 * Copyright (C) 2005,2006 iptelorg GmbH
 * Copyright (C) 2006 enum.at
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the sip-router software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 */
/*!
 * \file
 * \brief SIP-router TLS support :: select interface
 * \ingroup tls
 * Module: \ref tls
 */


#ifndef _TLS_SELECT_H
#define _TLS_SELECT_H

#include "../../select.h"
#include "../../pvar.h"

extern select_row_t tls_sel[];

extern pv_export_t tls_pv[];

#endif /* _TLS_SELECT_H */
