/*
 * Various lcr related functions
 *
 * Copyright (C) 2009-2010 Juha Heinanen
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the SIP Router software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*!
 * \file
 * \brief SIP-router lcr :: RPC API functions
 * \ingroup lcr
 * Module: \ref lcr
 */

#ifndef _LCR_RPC_H
#define _LCR_RPC_H

#ifndef NO_RPC_SUPPORT
#define RPC_SUPPORT /* support SIP Router RPCs by default */
#endif

#ifdef RPC_SUPPORT

#include "../../rpc.h"

extern rpc_export_t lcr_rpc[];

#endif /* RPC_SUPPORT */

#endif /* _LCR_RPC_H */
