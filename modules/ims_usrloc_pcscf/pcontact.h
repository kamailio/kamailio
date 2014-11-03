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

#ifndef IMPURECORD_H
#define IMPURECORD_H

#include <stdio.h>
#include <time.h>
#include "hslot.h"
#include "../../str.h"
#include "../../qvalue.h"
#include "usrloc.h"

struct hslot; 	/*!< Hash table slot */


void insert_ppublic(struct pcontact* _c, ppublic_t* _p);
int new_ppublic(str* public_identity, int is_default, ppublic_t** _p);
void free_ppublic(ppublic_t* _p);

int new_pcontact(/*str* _dom, str* public_identity, int reg_state, int barring, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, impurecord_t** _r*/);
void free_pcontact(pcontact_t* _c);
void print_pcontact(FILE* _f, pcontact_t* _r);
ppublic_t* mem_insert_ppublic(pcontact_t* _r/*, str* _c, ucontact_info_t* _ci*/);
void mem_remove_ppublic(pcontact_t* _r/*, ucontact_t* _c*/);
void mem_delete_ppublic(pcontact_t* _r/*, ucontact_t* _c*/);
void timer_pcontact(pcontact_t* _r);
int delete_ppublic(pcontact_t* _r/*, struct ucontact* _c*/);
int get_ppublic(pcontact_t* _r);
int aor_to_contact(str* aor, str* contact);
unsigned int get_hash_slot(udomain_t* _d, str* _aor, str* received_host, int received_port);
unsigned int get_aor_hash(udomain_t* _d, str* _aor, str* received_host, int received_port);


#endif
