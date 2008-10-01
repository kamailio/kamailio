/*
 * $Id: pua_usrloc.h 4518 2008-07-28 15:39:28Z henningw $
 *
 * pua_dialoginfo module - publish dialog-info from dialo module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2008 Klaus Darilion IPCom
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PUA_DLGINFO_H
#define _PUA_DLGINFO_H
#include "../pua/pua_bind.h"

send_publish_t pua_send_publish;

void dialog_publish(char *state, str *entity, str *peer, str *callid, 
	unsigned int initiator, unsigned int lifetime, str *localtag, str *remotetag);

#endif
