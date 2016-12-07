/*
 * Copyright (C) 2013 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __JAVA_SIG_PARSER_H__
#define	__JAVA_SIG_PARSER_H__

#include "../../str.h"
#include "../../sr_module.h"

#include <jni.h>

#define	JAVA_INV_SUPP_TYPE_OBJECTS

#define	JAVA_INV_SUPP_TYPE_BOOLEAN
#define	JAVA_INV_SUPP_TYPE_BYTE
#define	JAVA_INV_SUPP_TYPE_CHARACTER
#define	JAVA_INV_SUPP_TYPE_DOUBLE
#define	JAVA_INV_SUPP_TYPE_FLOAT
#define	JAVA_INV_SUPP_TYPE_INTEGER
#define	JAVA_INV_SUPP_TYPE_LONG
#define	JAVA_INV_SUPP_TYPE_SHORT
#define	JAVA_INV_SUPP_TYPE_STRING
#define	JAVA_INV_SUPP_TYPE_VOID

int is_sig_allowed(char *);
jvalue *get_value_by_sig_type(char *, char *);


#endif
