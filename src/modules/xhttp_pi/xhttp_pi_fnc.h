/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
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

/*!
 * \file
 * \brief XHTTP_PI :: 
 * \ingroup xhttp_pi
 * Module: \ref xhttp_pi
 */

#ifndef _XHTTP_PI_FNC_H
#define _XHTTP_PI_FNC_H

#include "../../lib/srdb1/db.h"
#include "xhttp_pi.h"

/**< no validation required */
#define PH_FLAG_NONE        0
/**< validate as socket: [proto:]host[:port] */
#define PH_FLAG_P_HOST_PORT (1<<0)
/**< validate as socket: [proto:]IPv4[:port] */
#define PH_FLAG_P_IPV4_PORT (1<<1)
/**< validate as IPv4 */
#define PH_FLAG_IPV4        (1<<2)
/**< validate as SIP URI */
#define PH_FLAG_URI     (1<<3)
/**< validate as SIP URI w/ IPv4 host */
#define PH_FLAG_URI_IPV4HOST    (1<<4)

typedef short int ph_val_flags;

typedef struct ph_db_url_ {
    str id;
    str db_url;
    db1_con_t *http_db_handle;
    db_func_t http_dbf;
}ph_db_url_t;

typedef struct ph_table_col_ {
    str field;
    db_type_t type;
    ph_val_flags validation;
}ph_table_col_t;
typedef struct ph_db_table_ {
    str id;
    str name;
    ph_db_url_t *db_url;
    ph_table_col_t *cols;
    int cols_size;
}ph_db_table_t;

typedef struct ph_vals_ {
    str *ids;  /* String to display for the given value */
    str *vals; /* prepopulated value for a specific field */
    int vals_size;
}ph_vals_t;

typedef struct ph_cmd_ {
    str name;
    unsigned int type;
    ph_db_table_t *db_table;
    db_op_t *c_ops;
    db_key_t *c_keys;
    db_type_t *c_types;
    ph_vals_t *c_vals; /* array of prepopulated values */
    int c_keys_size;
    db_key_t *q_keys;
    db_type_t *q_types;
    ph_vals_t *q_vals; /* array of prepopulated values */
	str *link_cmd;     /* cmd to be executed for query links */
    int q_keys_size;
    db_key_t *o_keys;
    int o_keys_size;
}ph_cmd_t;

typedef struct ph_mod_ {
    str module;
    ph_cmd_t *cmds;
    int cmds_size;
}ph_mod_t;

typedef struct ph_framework_ {
    ph_db_url_t *ph_db_urls;
    int ph_db_urls_size;
    ph_db_table_t *ph_db_tables;
    int ph_db_tables_size;
    ph_mod_t *ph_modules;
    int ph_modules_size;
}ph_framework_t;


int ph_init_async_lock(void);
void ph_destroy_async_lock(void);

int ph_init_cmds(ph_framework_t **framework_data, const char* filename);
int ph_parse_url(const str* url_str, int* mod, int* cmd, str* arg);
int ph_run_pi_cmd(pi_ctx_t *ctx);


#endif

