/*
 * header file of curlcon.c
 *
 * Copyright (C) 2015 Olle E. Johansson, Edvina AB
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
 * \brief Kamailio http_client :: Connection handler include file
 * \ingroup http_client
 * Module: \ref http_client
 */

#include "http_client.h"

#ifndef CURLCON_H
#define CURLCON_H

extern curl_con_t *_curl_con_root;

/*! Count the number of connections 
 */
unsigned int curl_connection_count();

/*! Check if CURL connection exists
 */
int http_connection_exists(str *name);

int http_client_load_config(str *config_file);

int curl_parse_param(char *val);

curl_con_t *curl_get_connection(str *name);
curl_con_pkg_t *curl_get_pkg_connection(curl_con_t *con);

#endif
