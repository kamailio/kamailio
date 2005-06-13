/*
 * $Id$
 *
 * Sdp mangler module
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
 * --------
 *  2003-04-07 first version.  
 */


#ifndef MANGLER_MOD_H
#define MANGLER_MOD_H

#include "../../str.h"
#include "../../parser/msg_parser.h"	/* struct sip_msg */
#include "sdp_mangler.h"
#include "contact_ops.h"


extern regex_t *portExpression;
extern regex_t *ipExpression;
extern char *contact_flds_separator;


#endif
