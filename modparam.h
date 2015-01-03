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
* \brief Kamailio core :: Module parameter handling (modparam)
* \ingroup core
* Module: \ref core
*/


#ifndef modparam_h
#define modparam_h

#include "sr_module.h"

int set_mod_param(char* _mod, char* _name, modparam_t _type, void* _val);

int set_mod_param_regex(char* regex, char* name, modparam_t type, void* val);

#endif
