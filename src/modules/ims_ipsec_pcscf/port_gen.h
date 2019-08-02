/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
 * Copyright (C) 2019 Aleksandar Yosifov
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

#ifndef _SPI_GEN_H_

#include <stdint.h>

//
// PORT GEN is based on SPI list, because the logics of the SPI gen and PORT gen are basically the same.
// It is used as an unique port generator for the TCP client and server ports.

int init_port_gen(uint32_t sport_start_val, uint32_t cport_start_val, uint32_t range);
int clean_port_lists();
int destroy_port_gen();
uint32_t acquire_sport(); // acquare server port
uint32_t acquire_cport(); // acquare client port
int release_sport(uint32_t port); // release server port
int release_cport(uint32_t port); // release client port

#endif /*  _SPI_GEN_H_ */
