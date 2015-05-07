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
    str     incoming_trunk_id;
    str     outgoing_trunk_id;
    str     external_trunk_id;
    str     route_id;
    str     is_ported;
    struct route_data* next;
    struct route_data* prev;
} route_data_t;

typedef struct ix_route_list {
    route_data_t* first;
    route_data_t* last;
    int count;
} ix_route_list_t;

route_data_t* new_route_data(str* incoming_trunk_id, str* outgoing_trunk_id, str* route_id, str* external_trunk_id, str* is_ported);
ix_route_list_t* new_route_list();
int add_route(struct ix_route_list* list, struct route_data* route);
int free_route_data(route_data_t* route_data);
int free_route_list (ix_route_list_t* ix_route_list);

#endif	/* ROUTE_DATA_H */

