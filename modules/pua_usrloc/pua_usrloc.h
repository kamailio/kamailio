/*
 * $Id$
 *
 * pua_urloc module - usrloc pua module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * \brief SIP-router Presence :: Usrloc module
 * \ingroup core
 * Module: \ref core
 */


#ifndef _PUA_UL_
#define _PUA_UL_
#include "../pua/pua_bind.h"

extern send_publish_t pua_send_publish;
extern send_subscribe_t pua_send_subscribe;
void ul_publish(ucontact_t* c, int type, void* param);
int pua_unset_publish(struct sip_msg* msg, unsigned int flags, void* param);

extern str pres_prefix;
extern int pua_ul_bflag;
extern int pua_ul_bmask;

#endif
