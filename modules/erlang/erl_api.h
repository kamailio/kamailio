/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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

#ifndef ERL_API_H_
#define ERL_API_H_

#include "../../sr_module.h"
#include "../../str.h"
#include "../../xavp.h"
#include <ei.h>

typedef int (*erl_rpc_f)(ei_x_buff* reply, const str *module,const str *function, const ei_x_buff *args);
typedef int (*erl_reg_send_f)(const str *server,const ei_x_buff *msg);
typedef int (*erl_reply_f)(const ei_x_buff *msg);
typedef int (*erl_send_f)(const erlang_pid *pid,const ei_x_buff *msg);

/* data serialization */
typedef int (*xavp2xbuff_f)(ei_x_buff *xbuff, sr_xavp_t *xavp);
typedef int (*xbuff2xavp_f)(sr_xavp_t **xavp, ei_x_buff *xbuff);

typedef struct erl_api_s {
	erl_rpc_f rpc;
	erl_reg_send_f reg_send;
	erl_send_f send;
	erl_reply_f reply;
	xavp2xbuff_f xavp2xbuff;
	xbuff2xavp_f xbuff2xavp;
} erl_api_t;

typedef  int (*load_erl_f)( erl_api_t* );

/*!
* \brief API bind function exported by the module - it will load the other functions
 * \param erl_api Erlang API export binding
 * \return 1
 */
int load_erl( erl_api_t *erl_api );

/*!
 * \brief Function to be called directly from other modules to load the Erlang API
 * \param erl_api Erlang API export binding
 * \return 0 on success, -1 if the API loader could not imported
 */
inline static int erl_load_api( erl_api_t *erl_api )
{
        load_erl_f load_erl_v;

        /* import the Erlang API auto-loading function */
        if ( !(load_erl_v=(load_erl_f)find_export("load_erl", 0, 0))) {
                LM_ERR("failed to import load_erl\n");
                return -1;
        }
        /* let the auto-loading function load all Erlang stuff */
        load_erl_v( erl_api );

        return 0;
}

/**
 * debugging macro
 * uses LM_DBG to print ei_x_buff
 */
#define EI_X_BUFF_PRINT(buf) \
do{ \
	char *mbuf = NULL; \
	int i = 0, v=0; \
	ei_decode_version((buf)->buff,&i,&v);\
	i=v?i:0; \
	ei_s_print_term(&mbuf, (buf)->buff, &i); \
	LM_DBG(#buf": %s\n", mbuf); \
	free(mbuf); \
} while(0)

#endif /* ERL_API_H_ */
