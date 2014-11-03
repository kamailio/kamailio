/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#ifndef UDOMAIN_H
#define UDOMAIN_H


#include <stdio.h>
#include "../../lib/kcore/statistics.h"
#include "../../locking.h"
#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "pcontact.h"
#include "hslot.h"
#include "usrloc.h"

struct hslot;   /*!< Hash table slot */
struct pcontact; /*!< contact record */

int new_udomain(str* _n, int _s, udomain_t** _d);
void free_udomain(udomain_t* _d);
void print_udomain(FILE* _f, udomain_t* _d);

void mem_timer_udomain(udomain_t* _d);
int mem_insert_pcontact(struct udomain* _d, str* _contact, struct pcontact_info* _ci, struct pcontact** _c);
void mem_delete_pcontact(udomain_t* _d, struct pcontact* _r);

void lock_udomain(udomain_t* _d, str *_aor, str* _received_host, unsigned short int received_port);
void unlock_udomain(udomain_t* _d, str *_aor, str* _received_host, unsigned short int received_port);

void lock_ulslot(udomain_t* _d, int i);
void unlock_ulslot(udomain_t* _d, int i);

int update_rx_regsession(struct udomain* _d, str* session_id, struct pcontact* _c);
int update_pcontact(struct udomain* _d, struct pcontact_info* _ci, struct pcontact* _c);
int insert_pcontact(struct udomain* _d, str* _contact, struct pcontact_info* _ci, struct pcontact** _r);
int get_pcontact(udomain_t* _d, str* _aor, str* _received_host, int received_port, struct pcontact** _r);
int get_pcontact_by_src(udomain_t* _d, str * _host, unsigned short _port, unsigned short _proto, struct pcontact** _c);
int assert_identity(udomain_t* _d, str * _host, unsigned short _port, unsigned short _proto, str * _identity);
int delete_pcontact(udomain_t* _d, str* _aor, str* _received_host, int _received_port, struct pcontact* _r);
int update_security(udomain_t* _d, security_type _t, security_t* _s, struct pcontact* _c);
int update_temp_security(udomain_t* _d, security_type _t, security_t* _s, struct pcontact* _c);

int preload_udomain(db1_con_t* _c, udomain_t* _d);

#endif
