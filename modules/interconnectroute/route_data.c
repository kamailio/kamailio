#include <string.h>
#include "route_data.h"

#include "../../mem/mem.h"

/* creates a new structure to hold route data in pkg memory 
   NB: remember to free when done!
 */

int free_route_data(route_data_t* route_data) {
    if(route_data) pkg_free(route_data);
        return 1;
}

int free_route_list (ix_route_list_t* ix_route_list) {
    route_data_t* tmp, *tmp1;
    tmp = ix_route_list->first;
    while(tmp) {
	tmp1 = tmp->next;
	free_route_data(tmp);
	tmp = tmp1;
    }
    return 1;
}

route_data_t* new_route_data(str* incoming_trunk_id, str* outgoing_trunk_id, str* route_id) {
    struct route_data* route_data;
    char *p;
    int len = sizeof(struct route_data) + incoming_trunk_id->len + outgoing_trunk_id->len;
    if(route_id) len = len + route_id->len;
    
    route_data = (struct route_data*)pkg_malloc(len);
    if (!route_data) {
	LM_ERR("no more pkg memory\n");
	return NULL;
    }
    
    memset(route_data, 0, len);
    
    p = (char*)(route_data + 1);
    
    route_data->incoming_trunk_id.s = p;
    memcpy(p, incoming_trunk_id->s, incoming_trunk_id->len);
    route_data->incoming_trunk_id.len = incoming_trunk_id->len;
    p+= incoming_trunk_id->len;
    
    route_data->outgoing_trunk_id.s = p;
    memcpy(p, outgoing_trunk_id->s, outgoing_trunk_id->len);
    route_data->outgoing_trunk_id.len = outgoing_trunk_id->len;
    p+= outgoing_trunk_id->len;
    
    if(route_id) {
	route_data->route_id.s = p;
	memcpy(p, route_id->s, route_id->len);
	route_data->route_id.len = route_id->len;
	p+=route_id->len;
    }
    
    if (p != (((char*) route_data) + len)) {
        LM_CRIT("buffer overflow\n");
        pkg_free(route_data);
        return 0;
    }
    
    return route_data;
}

ix_route_list_t* new_route_list() {
    ix_route_list_t* route_list = pkg_malloc(sizeof(ix_route_list_t));
    
    memset(route_list, 0, sizeof(ix_route_list_t));
    
    return route_list;
}

int add_route(struct ix_route_list* list, struct route_data* route) 
{
    if (list->count == 0) {
	list->first = list->last = route;
	list->count++;
	return 1;
    }
    
    route->prev = list->last;
    list->last->next = route;
    list->last = route;
    list->count++;
    
    return 1;
}