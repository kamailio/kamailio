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

#include "spi_gen.h"
#include "spi_list.h"
#include <pthread.h>

pthread_mutex_t spis_mut;
spi_list_t used_spis;
uint32_t spi_val;
uint32_t min_spi;
uint32_t max_spi;

int init_spi_gen(uint32_t start_val, uint32_t range)
{
    if(start_val < 1) {
        return 1;
    }

    if(UINT32_MAX - range < start_val)
        return 2;

    if(pthread_mutex_init(&spis_mut, NULL))
        return 3;

    used_spis = create_list();

    spi_val = start_val;
    min_spi = start_val;
    max_spi = start_val + range;

    return 0;
}

uint32_t acquire_spi()
{
    //save the initial value for the highly unlikely case where there are no free SPIs
    uint32_t initial_val = spi_val;
    uint32_t ret = 0; // by default return invalid SPI

    if(pthread_mutex_lock(&spis_mut) != 0) {
        return ret;
    }

    while(1) {
        if(spi_in_list(&used_spis, spi_val) == 0) {
            ret = spi_val;
            spi_val++;
            break;
        }

        spi_val++; //the current SPI is not available - increment

        if(spi_val >= max_spi) { //reached the top of the range - reset
            spi_val = min_spi;
        }

        if(spi_val == initial_val) { //there are no free SPIs
            break;
        }

    }

    //found unused SPI - add it to the used list
    if(spi_add(&used_spis, ret) != 0) {
        ret = 0;
    }

    pthread_mutex_unlock(&spis_mut);

    return ret;
}

int release_spi(uint32_t id)
{
    if(pthread_mutex_lock(&spis_mut) != 0) {
        return 1;
    }

    spi_remove(&used_spis, id);

    pthread_mutex_unlock(&spis_mut);

    return 0;
}

int destroy_spi_gen()
{
    return pthread_mutex_destroy(&spis_mut);
}
