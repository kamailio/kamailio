/**
 * Copyright (C) 2011 SpeakUp B.V. (alex@speakup.nl)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
       
#ifndef _SQL_TRANS_H_
#define _SQL_TRANS_H_

#include "../../pvar.h"

enum _tr_sql_type { TR_SQL_NONE=0, TR_SQL };
enum _tr_sql_subtype { 
	TR_SQL_ST_NONE=0, TR_SQL_VAL, TR_SQL_VAL_INT, TR_SQL_VAL_STR };

char* tr_parse_sql(str *in, trans_t *tr);

#endif
