/*
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
 * \brief comparison functions
 * \ingroup libkcore
 */

#ifndef _CMPAPI_H_
#define _CMPAPI_H_

#include <string.h>
#include "../../str.h"

/**
 * Comparison functions
 * - return:
 *   - < 0 - less
 *   - ==0 - equal
 *   - > 0 - greater
 * cmp_str* - for strings does case sensitive comparison
 * cmpi_str* - for strings does case insensitive comparison
 *
 */

#define cmp_strz(a, b) strcmp((a), (b))
#define cmpi_strz(a, b) strcasecmp((a), (b))

int cmp_str(str *s1, str *s2);
int cmpi_str(str *s1, str *s2);

int cmp_hdrname_str(str *s1, str *s2);
int cmp_hdrname_strzn(str *s1, char *s2, size_t n);
int cmp_uri_str(str *s1, str *s2);
int cmp_aor_str(str *s1, str *s2);

#endif

