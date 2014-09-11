/* 
 * $Id: perlvdb.h 770 2007-01-22 10:16:34Z bastian $
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

#ifndef _PERLVDB_H
#define _PERLVDB_H 


#include "../../lib/srdb1/db.h"
#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

/* lock_ops.h defines union semun, perl does not need to redefine it */
#ifdef USE_SYSV_SEM
# define HAS_UNION_SEMUN
#endif

#undef OP_LT
#undef OP_GT
#undef OP_EQ

#undef load_module

#include <EXTERN.h>
#include <perl.h>

#include "perlvdb_conv.h"
#include "perlvdb_oohelpers.h"
#include "perlvdbfunc.h"

#define PERL_VDB_BASECLASS	"Kamailio::VDB"

#define PERL_VDB_USETABLEMETHOD	"use_table"
#define PERL_VDB_INSERTMETHOD	"_insert"
#define PERL_VDB_REPLACEMETHOD	"_replace"
#define PERL_VDB_UPDATEMETHOD	"_update"
#define PERL_VDB_DELETEMETHOD	"_delete"
#define PERL_VDB_QUERYMETHOD	"_query"

#define PERL_VDB_COLDEFSMETHOD	"coldefs"
#define PERL_VDB_TYPEMETHOD	"type"
#define PERL_VDB_NAMEMETHOD	"name"
#define PERL_VDB_ROWSMETHOD	"rows"
#define PERL_VDB_DATAMETHOD	"data"

extern PerlInterpreter* my_perl;

extern SV* vdbmod;

#endif /* _PERLVDB_H */
