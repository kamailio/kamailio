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

#include "spi_gen.h"
#include "spi_list.h"
#include <pthread.h>

pthread_mutex_t sport_mut;  // server port mutex
pthread_mutex_t cport_mut;  // client port mutex
spi_list_t used_sports;     // list with used server ports
spi_list_t used_cports;     // list with used client ports
uint32_t sport_val;         // the last acquired server port
uint32_t cport_val;         // the last acquired client port
uint32_t min_sport;
uint32_t min_cport;
uint32_t max_sport;
uint32_t max_cport;

int init_port_gen(uint32_t sport_start_val, uint32_t cport_start_val, uint32_t range)
{
    if(sport_start_val < 1 || cport_start_val < 1){
        return 1;
    }

    if((UINT32_MAX - range < sport_start_val) || (UINT32_MAX - range < cport_start_val)){
        return 2;
    }

    if(pthread_mutex_init(&sport_mut, NULL) || pthread_mutex_init(&cport_mut, NULL)){
        return 3;
    }

    used_sports = create_list();
    used_cports = create_list();

    sport_val = min_sport = sport_start_val;
    cport_val = min_cport = cport_start_val;
    max_sport = sport_start_val + range;
    max_cport = cport_start_val + range;

    return 0;
}

uint32_t acquire_port(spi_list_t* used_ports, pthread_mutex_t* port_mut, uint32_t* port_val, uint32_t min_port, uint32_t max_port)
{
    //save the initial value for the highly unlikely case where there are no free server PORTs
    uint32_t initial_val = *port_val;
    uint32_t ret = 0; // by default return invalid port

    if(pthread_mutex_lock(port_mut) != 0) {
        return ret;
    }

    while(1){
        if(spi_in_list(used_ports, *port_val) == 0) {
            ret = *port_val;
            (*port_val)++;
            break;
        }

        (*port_val)++; //the current server port is not available - increment

        if(*port_val >= max_port) { //reached the top of the range - reset
            *port_val = min_port;
        }

        if(*port_val == initial_val) { //there are no free server ports
            pthread_mutex_unlock(port_mut);
            return ret;
        }
    }

    // found unused server port - add it to the used list
    if(spi_add(used_ports, ret) != 0) {
        ret = 0;
    }

    pthread_mutex_unlock(port_mut);
    return ret;
}

uint32_t acquire_sport()
{
    return acquire_port(&used_sports, &sport_mut, &sport_val, min_sport, max_sport);
}

uint32_t acquire_cport()
{
    return acquire_port(&used_cports, &cport_mut, &cport_val, min_cport, max_cport);
}

int release_sport(uint32_t port)
{
    if(pthread_mutex_lock(&sport_mut) != 0){
        return 1;
    }

    spi_remove(&used_sports, port);

    pthread_mutex_unlock(&sport_mut);
    return 0;
}

int release_cport(uint32_t port)
{
    if(pthread_mutex_lock(&cport_mut) != 0){
        return 1;
    }

    spi_remove(&used_cports, port);

    pthread_mutex_unlock(&cport_mut);
    return 0;
}

int destroy_port_gen()
{
    int ret = pthread_mutex_destroy(&sport_mut);
    if(ret != 0){
        return ret;
    }

    return pthread_mutex_destroy(&cport_mut);
}
