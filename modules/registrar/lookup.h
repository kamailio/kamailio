/*
 * Lookup contacts in usrloc
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
 */

/*!
 * \file
 * \brief SIP registrar module - lookup contacts in usrloc
 * \ingroup registrar   
 */  


#ifndef LOOKUP_H
#define LOOKUP_H

#include "../../parser/msg_parser.h"
#include "../../modules/usrloc/usrloc.h"


/*! \brief
 * Lookup contact in the database and rewrite Request-URI
 * or not according to _mode value:
 *  0: rewrite
 *  1: don't rewrite
 * \return: -1 : not found
 *          -2 : found but method not allowed
 *          -3 : error
 */
int lookup_helper(struct sip_msg* _m, udomain_t* _d, str* _uri, int _mode);

/*! \brief
 * Lookup a contact in usrloc and rewrite R-URI if found
 */
int lookup(struct sip_msg* _m, udomain_t* _d, str* _uri);

/*! \brief
 * Lookup a contact in usrloc and add the records to the dset structure
 */
int lookup_to_dset(struct sip_msg* _m, udomain_t* _d, str* _uri);

/*! \brief
 * Lookup r-uri and additional branches in usrloc
 */
int lookup_branches(sip_msg_t *msg, udomain_t *d);


/*! \brief
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered(struct sip_msg* _m, udomain_t* _d, str* _uri);
int registered3(struct sip_msg* _m, udomain_t* _d, str* _uri, int match_flag);
int registered4(struct sip_msg* _m, udomain_t* _d, str* _uri, int match_flag, int match_action_flag);

#endif /* LOOKUP_H */
