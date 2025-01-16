/*
 * Copyright (C) 2024-2025 Resaa Co.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#ifndef cross_strings_h
#define cross_strings_h

#include <string.h>

#ifdef _WIN32
static inline int strcasecmp(const char *str1, const char *str2) {
    return _stricmp(str1, str2);
}
#else
#include <strings.h>
#endif

#endif /*_cross_strings_h */

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */

