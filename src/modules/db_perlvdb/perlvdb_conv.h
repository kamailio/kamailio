/* 
 * $Id: perlvdb_conv.h 770 2007-01-22 10:16:34Z bastian $
 *
 * Perl virtual database module interface
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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

#ifndef _PERLVDB_CONV_H
#define _PERLVDB_CONV_H 

#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db_key.h"

#include "db_perlvdb.h"

#include <XSUB.h>

#define PERL_CLASS_VALUE	"Kamailio::VDB::Value"
#define PERL_CLASS_PAIR		"Kamailio::VDB::Pair"
#define PERL_CLASS_REQCOND	"Kamailio::VDB::ReqCond"

#define PERL_CONSTRUCTOR_NAME	"new"

/* Converts a set of pairs to perl SVs.
 * For insert, and update (second half)
 */
AV *pairs2perlarray(const db_key_t* keys, const db_val_t* vals, const int n);

/* Converts a set of cond's to perl SVs.
 * For delete, update (first half), query
 */
AV *conds2perlarray(const db_key_t* keys, const db_op_t* ops, const db_val_t* vals, const int n);

/* Converts a set of key names to a perl array.
 * Needed in query.
 */
AV *keys2perlarray(const db_key_t* keys, const int n);

SV *val2perlval(const db_val_t* val);
SV *pair2perlpair(const db_key_t key, const db_val_t* val);
SV *cond2perlcond(const db_key_t key, const db_op_t op, const db_val_t* val);

int perlresult2dbres(SV *perlres, db1_res_t **r);

#endif /* _PERLVDB_CONV_H */
