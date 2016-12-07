/* 
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */

/*!
 * \file
 * \brief SNMP statistic module, utilities
 *
 * This file was created to group together utility functions that were useful
 * throughout the SNMPStats module, without belonging to any file in particular.
 * \ingroup snmpstats
 * - Module: \ref snmpstats
 */


#ifndef _SNMP_UTILITIES_
#define _SNMP_UTILITIES_

#include <time.h>

#include "../../str.h"
#include "../../sr_module.h"

/*!
 * This function copies an Kamailio "str" datatype into a '\\0' terminated char*
 * string. 
 *
 * \note Make sure to free the memory allocated to *copiedString, when you no
 *       longer have any use for it. (It is allocated with shm_malloc(), so make
 *       sure to deallocate it with shm_free()) 
 */
int convertStrToCharString(str *strToConvert, char **copiedString);

/*! Performs sanity checks on the parameters passed to a string configuration
 * file parameter handler. */
int stringHandlerSanityCheck( modparam_t type, void *val, char *parameterName);

/*!
 * This function is a wrapper around the standard statistic framework.  It will
 * return the value of the statistic denoted with statName, or zero if the
 * statistic was not found. 
 */
int get_statistic(char *statName);

/*! Returns a pointer to an SNMP DateAndTime OCTET STRING representation of the
 * time structure.  Note that the pointer is to static data, so it shouldn't be
 * counted on to be around if this function is called again. */
char * convertTMToSNMPDateAndTime(struct tm *timeStructure);

/*! \brief Get config framework variable 
 * type will return cfg_type - CFG_VAR_INT, CFG_VAR_STRING, CFG_VAR_STR
 * If type is CFG_VAR_UNSET then call failed and return value should be ignored.
*/
int snmp_cfg_get_int(char *arg_group, char *arg_name, unsigned int *type);

/*! Initialize config framework */
int config_context_init(void);


#endif
