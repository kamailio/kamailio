/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * UAC SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2005-01-31  first version (ramona)
 */


#ifndef _UAC_FROM_H_
#define _UAC_FROM_H_

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../modules/tm/t_hooks.h"

#define FROM_NO_RESTORE      (0)
#define FROM_AUTO_RESTORE    (1)
#define FROM_MANUAL_RESTORE  (2)

void init_from_replacer();

int replace_from( struct sip_msg *msg, str *from_dsp, str *from_uri);

int restore_from( struct sip_msg *msg, int is_req);

/* TM callback functions */
void tr_checker(struct cell* t, int type, struct tmcb_params *p);


#endif
