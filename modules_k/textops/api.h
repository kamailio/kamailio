/* Copyright (C) 2008 Telecats BV
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2008-07-14 first version (Ardjan Zwartjes)
 */


#ifndef TEXTOPS_API_H_
#define TEXTOPS_API_H_
#include "../../str.h"
#include "../../sr_module.h"


typedef int (*append_hf_t)(struct sip_msg*, str*);
typedef int (*remove_hf_t)(struct sip_msg*, str*);
typedef int (*search_append_t)(struct sip_msg*, str*, str*);
typedef int (*search_t)(struct sip_msg*, str*);

/*
 * Struct with the textops api.
 */
struct textops_binds{
	append_hf_t	append_hf; // Append a header to the message.
	remove_hf_t	remove_hf; // Remove a header with the specified name from the message.
	search_append_t search_append; // Append a str after a match of the specified regex.
	search_t search; // Check if the regex matches a part of the message.
};

typedef int (*load_textops_f)(struct textops_binds*);

/*
 * function exported by module - it will load the other functions
 */
int load_textops(struct textops_binds*);

/*
 * Function to be called direclty from other modules to load
 * the textops API.
 */
inline static int load_textops_api(struct textops_binds *tob){
	load_textops_f load_textops_exports;
	if(!(load_textops_exports=(load_textops_f)find_export("load_textops",0,0))){
		LM_ERR("Failed to import load_textops\n");
		return -1;
	}
	return load_textops_exports(tob);
}

#endif /*TEXT_OPS_API_H_*/
