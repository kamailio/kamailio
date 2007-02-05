/*
 * $Id$
 *
 * Perl module for OpenSER
 *
 * Copyright (C) 2006 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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

#ifndef PERL_MOD_H
#define PERL_MOD_H

#include <EXTERN.h>
#include <perl.h>

#include "../sl/sl_api.h"

extern char *filename;
extern char *modpath;

extern PerlInterpreter *my_perl;

extern struct sl_binds slb;

#endif /* PERL_MOD_H */
