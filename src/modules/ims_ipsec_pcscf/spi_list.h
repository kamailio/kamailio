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

#ifndef _SPI_LIST_H_

#include <stdlib.h>
#include <stdint.h>

//
// Single linked list implementation. The elements are kept sorted via insertion sort.
//


typedef struct _spi_node spi_node_t;

struct _spi_node {
    spi_node_t* next;
    uint32_t id;
};

typedef struct _spi_list {
    spi_node_t* head;
    spi_node_t* tail;
} spi_list_t;


spi_list_t create_list();
void destroy_list(spi_list_t* lst);
int spi_add(spi_list_t* list, uint32_t id);
int spi_remove(spi_list_t* list, uint32_t id);
int spi_in_list(spi_list_t* list, uint32_t id);

#endif /* _SPI_LIST_H_ */
