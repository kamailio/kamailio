/*
 * $Id$
 *
 * TLS module - management interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/** tls module management interface (rpc).
 * @file tls_rpc.h
 * @ingroup tls
 * Module: @ref tls
 */

#ifndef _TLS_RPC_H
#define _TLS_RPC_H

#include "../../rpc.h"

extern rpc_export_t tls_rpc[];

#endif /* _TLS_RPC_H */
