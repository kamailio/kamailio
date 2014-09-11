/*
 * Record-Route & Route module interface
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-03-15 License added (janakj)
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */

#ifndef RR_MOD_H
#define RR_MOD_H

#include "../../usr_avp.h"
#include "../domain/domain.h"
#include "../../sr_module.h"

extern avp_ident_t user_part_avp_ident;
extern avp_ident_t next_route_avp_ident;

extern int append_fromtag;
extern int enable_double_rr;
extern int enable_full_lr;
extern int rr_force_send_socket;

extern domain_get_did_t dm_get_did;
extern fparam_t* fparam_username;

#define AVP_FLAG_DIALOG_COOKIE "dialog_cookie"

#endif /* RR_MOD_H */
