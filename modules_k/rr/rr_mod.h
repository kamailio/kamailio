/*
 * $Id$
 *
 * Record-Route & Route module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ---------
 * 2003-03-15 License added (janakj)
 */

#ifndef RR_MOD_H
#define RR_MOD_H

#ifdef ENABLE_USER_CHECK
#include "../../str.h"
extern str i_user;
#endif

extern int append_fromtag;
extern int enable_double_rr;
extern int enable_full_lr;
extern int add_username;

#endif /* RR_MOD_H */
