/* 
 * File:   route_data.h
 * Author: jaybeepee
 *
 * Created on 15 October 2014, 4:51 PM
 */

#ifndef ROUTE_DATA_H
#define	ROUTE_DATA_H

#include "../../str.h"

typedef struct route_data {
    str     operator_key;
    str     trunk_id;
    str     external_trunk_id;
    str     ipv4;
    int     priority;
    struct route_data* next;
    struct route_data* prev;
} route_data_t;

typedef struct ix_route_list {
    route_data_t* first;
    route_data_t* last;
    int count;
} ix_route_list_t;

route_data_t* new_route_data(str* operator_key, str* trunk_id, int priority, str* gw_ip, str* external_trunk_id);
ix_route_list_t* new_route_list();
int add_route(struct ix_route_list* list, struct route_data* route);

#endif	/* ROUTE_DATA_H */

