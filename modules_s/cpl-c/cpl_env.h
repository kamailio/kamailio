/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * 11-06-1004:  created (bogdan)
 */

#ifndef _CPL_C_ENV_H
#define _CPL_C_ENV_H

#include "../../str.h"
#include "../usrloc/usrloc.h"
#include "../tm/tm_load.h"

struct cpl_enviroment {
	char  *log_dir;         /* dir where the user log should be dumped */
	int    proxy_recurse;   /* numbers of proxy redirection accepted */
	int    proxy_route;     /* script route to be run before proxy */
	int    nat_flag;        /* flag for marking lookuped contact as NAT */
	int    case_sensitive;  /* is user part case sensitive ? */
	str    realm_prefix;    /* domain prefix to be ignored */
	int    cmd_pipe[2];     /* comunication pipe with aux. process */
	str    orig_tz;         /* a copy of the original TZ; keept as a null
                             * terminated string in "TZ=value" format;
                             * used only by run_time_switch */
	udomain_t*  lu_domain;  /* domain used for lookup */
	int lu_append_branches; /* how many branches lookup should add */
};


struct cpl_functions {
	struct tm_binds tmb;     /* Structure with pointers to tm funcs */
	usrloc_api_t ulb;        /* Structure with pointers to usrloc funcs */
	int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);
};

extern struct cpl_enviroment cpl_env;
extern struct cpl_functions  cpl_fct;

#endif


