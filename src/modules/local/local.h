/*
 * Various local module related functions
 *
 * Copyright (C) 2007 Juha Heinanen
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
 */


#ifndef _LOCAL_H_
#define _LOCAL_H_

#include "../../modules/domain/api.h"

/* Types */

typedef enum sip_protos uri_transport;

typedef enum { UNC, UNV, B, NA } condition_t;

/* Module parameters */
extern int diversion_reason_avp;
extern int callee_cfunc_avp;
extern int callee_cfunv_avp;
extern int callee_cfb_avp;
extern int callee_cfna_avp;
extern int callee_crunc_avp;
extern int callee_crunv_avp;
extern int callee_crb_avp;
extern int callee_crna_avp;

/* Internal module variables */

extern domain_api_t domain_api;

#endif /* _LOCAL_H_ */
