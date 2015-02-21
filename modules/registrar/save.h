/*
 * Functions that process REGISTER message 
 * and store data in usrloc
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 */
/*!
 * \file
 * \brief SIP registrar module - process REGISTER message
 * \ingroup registrar   
 */  


#ifndef SAVE_H
#define SAVE_H


#include "../../parser/msg_parser.h"
#include "../../modules/usrloc/usrloc.h"


/*! \brief
 * Process REGISTER request and save it's contacts
 */
int save(struct sip_msg* _m, udomain_t* _d, int _cflags, str* _uri);
int unregister(struct sip_msg* _m, udomain_t* _d, str* _uri, str *_ruid);
int set_q_override(struct sip_msg* _m, int _q);

#endif /* SAVE_H */
