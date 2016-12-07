/*
 * $Id$
 *
 * Functions that process REGISTER message 
 * and store data in usrloc
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>

#include "../../dprint.h"

#include "reg_mod.h"
#include "lookup.h"
#include "save.h"
#include "api.h"

/**
 *
 */
//int regapi_save(struct sip_msg *msg, char *table)
//{
//	udomain_t* d;
//
//	if(ul.get_udomain(table, &d)<0)
//	{
//		LM_ERR("usrloc domain [%s] not found\n", table);
//		return -1;
//	}
//	return save(msg, d);
//}
//
///**
// *
// */
//int regapi_lookup(struct sip_msg *msg, char *table)
//{
//	udomain_t* d;
//
//	if(ul.get_udomain(table, &d)<0)
//	{
//		LM_ERR("usrloc domain [%s] not found\n", table);
//		return -1;
//	}
//	return lookup(msg, d);
//}
//
///**
// *
// */
//int regapi_registered(struct sip_msg *msg, char *table)
//{
//	udomain_t* d;
//
//	if(ul.get_udomain(table, &d)<0)
//	{
//		LM_ERR("usrloc domain [%s] not found\n", table);
//		return -1;
//	}
//	return term_impu_registered(msg, (char*)d, NULL);
//}
//
///**
// *
// */
//int bind_registrar(registrar_api_t* api)
//{
//	if (!api) {
//		ERR("Invalid parameter value\n");
//		return -1;
//	}
//	api->save   = regapi_save;
//	api->lookup = regapi_lookup;
//	api->registered = regapi_registered;
//
//	return 0;
//}
