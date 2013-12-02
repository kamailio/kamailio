/*
 * TLS module
 *
 * Copyright (C) 2006 enum.at
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
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
/** log the verification failure reason.
 * @file tls_dump_vf.h
 * @ingroup: tls
 * Module: @ref tls
 */
/*
 * History:
 * --------
 *  2010-05-20  split from tls_server.c
*/

#ifndef __tls_dump_vf_h
#define __tls_dump_vf_h


void tls_dump_verification_failure(long verification_result);

#endif /*__tls_dump_vf_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
