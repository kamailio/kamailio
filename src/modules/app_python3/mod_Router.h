/**
 * Copyright (C) 2012 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#ifndef	__API__MOD_ROUTER_H__
#define	__API__MOD_ROUTER_H__

#include <Python.h>
#include <libgen.h>

extern PyObject *_sr_apy_main_module;
extern PyObject *_sr_apy_main_module_dict;

extern PyMethodDef RouterMethods[];

void init_mod_Router(void);
void destroy_mod_Router(void);

#endif
