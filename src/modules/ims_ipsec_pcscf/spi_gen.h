/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
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
// SPI GEN is based on SPI list.
// It is used as a unique ID generator for the SPIs. As the range of IDs is limited it
// is important not to use generate ID which is still in use. For this reason there are
// acquire_spi() and release_spi(uint32_t id) functions.

int init_spi_gen(uint32_t spi_start_val, uint32_t spi_range , uint32_t sport_start_val, uint32_t cport_start_val, uint32_t port_range);
int clean_spi_list();
int destroy_spi_gen();
uint32_t acquire_spi(uint32_t* spi_cid , uint32_t* spi_sid , uint16_t* cport , uint16_t* sport);
int release_spi(uint32_t spi_cid , uint32_t spi_sid , uint16_t cport , uint16_t sport);

#endif /*  _SPI_GEN_H_ */
