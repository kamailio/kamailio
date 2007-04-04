/*
 * $Id: request_winfo.h 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_xmpp module - presence SIP - XMPP Gateway
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-03-29  initial version (anca)
 */
#ifndef XMPP_SWINFO_H
#define XMPP_SWINFO_H

#include "../../parser/msg_parser.h"

int request_winfo(struct sip_msg* msg, char* uri, char* expires);

#endif
