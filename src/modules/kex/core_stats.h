/*
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief KEX :: Kamailio statistics
 * \ingroup kex
 */


#ifndef _CORE_STATS_H_
#define _CORE_STATS_H_

#include "../../core/counters.h"

#ifdef STATISTICS
/*! exported core statistics */
extern stat_export_t core_stats[];

/*! \brief received requests */
extern stat_var *rcv_reqs;

/* \brief extended received requests by method */
extern stat_var *rcv_reqs_invite;
extern stat_var *rcv_reqs_cancel;
extern stat_var *rcv_reqs_ack;
extern stat_var *rcv_reqs_bye;
extern stat_var *rcv_reqs_info;
extern stat_var *rcv_reqs_register;
extern stat_var *rcv_reqs_notify;
extern stat_var *rcv_reqs_message;
extern stat_var *rcv_reqs_options;
extern stat_var *rcv_reqs_prack;
extern stat_var *rcv_reqs_update;
extern stat_var *rcv_reqs_refer;
extern stat_var *rcv_reqs_publish;

/*! \brief received replies */
extern stat_var *rcv_rpls;

/*! \brief extended received replies */
extern stat_var *rcv_rpls_1xx;
extern stat_var *rcv_rpls_18x;
extern stat_var *rcv_rpls_2xx;
extern stat_var *rcv_rpls_3xx;
extern stat_var *rcv_rpls_4xx;
extern stat_var *rcv_rpls_401;
extern stat_var *rcv_rpls_404;
extern stat_var *rcv_rpls_407;
extern stat_var *rcv_rpls_480;
extern stat_var *rcv_rpls_486;
extern stat_var *rcv_rpls_5xx;
extern stat_var *rcv_rpls_6xx;
extern stat_var *rcv_rpls_2xx_invite;
extern stat_var *rcv_rpls_4xx_invite;

/*! \brief forwarded requests */
extern stat_var *fwd_reqs;

/*! \brief forwarded replies */
extern stat_var *fwd_rpls;

/*! \brief dropped requests */
extern stat_var *drp_reqs;

/*! \brief dropped replies */
extern stat_var *drp_rpls;

/*! \brief error requests */
extern stat_var *err_reqs;

/*! \brief error replies */
extern stat_var *err_rpls;

/*! \brief Set in parse_uri() */
extern stat_var *bad_URIs;

/*! \brief Set in parse_method() */
extern stat_var *unsupported_methods;

/*! \brief Set in get_hdr_field(). */
extern stat_var *bad_msg_hdr;

int register_core_stats(void);

#endif /*STATISTICS*/

#endif /*_CORE_STATS_H_*/
