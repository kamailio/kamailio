/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifndef _AVPOPS_PARSE_H_
#define _AVPOPS_PARSE_H_

#include "../../str.h"
#include "../../usr_avp.h"
#include "avpops_impl.h"
#include "avpops_db.h"


struct fis_param *avpops_parse_pvar(char *s);

int   parse_avp_db(char *s, struct db_param *dbp, int allow_scheme);

int   parse_avp_aliases(char *s, char c1, char c2);

struct fis_param* parse_intstr_value(char *p, int len);

int parse_avp_db_scheme(char *s, struct db_scheme *scheme);

struct fis_param*  parse_check_value(char *s);

struct fis_param*  parse_op_value(char *s);

#endif

