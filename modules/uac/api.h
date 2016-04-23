/*
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
#ifndef UAC_API_H_
#define UAC_API_H_
#include "../../sr_module.h"


typedef int (*uac_replace_from_t)(sip_msg_t *, str *, str *);
typedef int (*uac_replace_to_t)(sip_msg_t *, str *, str *);
typedef int (*uac_req_send_t)(void);

typedef struct uac_binds {
	uac_replace_from_t	replace_from;
	uac_replace_to_t	replace_to;
	uac_req_send_t      req_send;
} uac_api_t;

typedef int (*bind_uac_f)(uac_api_t*);

int bind_uac(uac_api_t*);

inline static int load_uac_api(uac_api_t *uacb){
	bind_uac_f bind_uac_exports;
	if(!(bind_uac_exports=(bind_uac_f)find_export("bind_uac",1,0))){
		LM_ERR("Failed to import bind_uac\n");
		return -1;
	}
	return bind_uac_exports(uacb);
}

#endif /* UAC_API_H_ */
