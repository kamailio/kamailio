/* Copyright (C) 2008 Telecats BV
 *
 * This file is part of Kamailio, a free SIP server.
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

/*!
 * \file
 * \brief API
 * \ingroup textops
 * Module: \ref textops
 */



#ifndef TEXTOPS_API_H_
#define TEXTOPS_API_H_
#include "../../str.h"
#include "../../sr_module.h"


typedef int (*append_hf_t)(struct sip_msg*, str*);
typedef int (*remove_hf_t)(struct sip_msg*, str*);
typedef int (*search_append_t)(struct sip_msg*, str*, str*);
typedef int (*search_t)(struct sip_msg*, str*);
typedef int (*is_privacy_t)(struct sip_msg*, str*);

/*
 * Struct with the textops api.
 */
typedef struct textops_binds {
	append_hf_t	append_hf; // Append a header to the message.
	remove_hf_t	remove_hf; // Remove a header with the specified name from the message.
	search_append_t search_append; // Append a str after a match of the specified regex.
	search_t search; // Check if the regex matches a part of the message.
	is_privacy_t	is_privacy;
} textops_api_t;

typedef int (*bind_textops_f)(textops_api_t*);

/*
 * function exported by module - it will load the other functions
 */
int bind_textops(textops_api_t*);

/*
 * Function to be called direclty from other modules to load
 * the textops API.
 */
inline static int load_textops_api(textops_api_t *tob){
	bind_textops_f bind_textops_exports;
	if(!(bind_textops_exports=(bind_textops_f)find_export("bind_textops",0,0))){
		LM_ERR("Failed to import bind_textops\n");
		return -1;
	}
	return bind_textops_exports(tob);
}

#endif /*TEXT_OPS_API_H_*/
