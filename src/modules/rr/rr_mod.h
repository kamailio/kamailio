/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/*!
 * \file
 * \brief Route & Record-Route module interface
 * \ingroup rr
 */

#ifndef RR_MOD_H
#define RR_MOD_H

#include "../outbound/api.h"

#ifdef ENABLE_USER_CHECK
#include "../../str.h"
extern str i_user;
#endif

/*! should request's from-tag is appended to record-route */
extern int append_fromtag;
/*! insert two record-route header instead of one */
extern int enable_double_rr;
/*! work around some broken UAs */
extern int enable_full_lr;
/*! add username to record-route URI */
extern int add_username;
extern int enable_socket_mismatch_warning;
extern ob_api_t rr_obb;

#endif /* RR_MOD_H */
