/*
 * $Id$
 *
 * Functions that process REGISTER message 
 * and store data in usrloc
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
 * -------
 * 2003-03-21  save_noreply added, provided by Maxim Sobolev <sobomax@sippysoft.com> (janakj)
 */


#ifndef SAVE_H
#define SAVE_H


#include "../../parser/msg_parser.h"


/*
 * Process REGISTER request and save it's contacts
 */
int save(struct sip_msg* _m, char* _t, char* aor_filter);


/*
 * Process REGISTER request and save it's contacts, do not send any replies
 */
int save_noreply(struct sip_msg* _m, char* _t, char* aor_filter);


/*
 * Process REGISTER request and save it's contacts, do not send any replies
 */
int save_memory(struct sip_msg* _m, char* _t, char* aor_filter);

/*
 * Update memory cache only and do not send reply back
 */
int save_mem_nr(struct sip_msg* msg, char* table, char* aor_filter);

#endif /* SAVE_H */
