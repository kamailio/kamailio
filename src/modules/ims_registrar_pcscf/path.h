/*
 * Copyright (C) 2026 toharishs@gmail.com
 *
 * The initial version of this code is written by Harish S
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
 *
 */

#ifndef IMS_REGISTRAR_PCSCF_PATH_H
#define IMS_REGISTRAR_PCSCF_PATH_H

#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"

int pcscf_build_path_uri(str *pcscf_uri, str *out, char *buf, int buf_len);
int pcscf_insert_path_on_register(struct sip_msg *msg, str *path_uri);
int pcscf_format_route_header(str *path, char *buf, int buf_len);

#endif
