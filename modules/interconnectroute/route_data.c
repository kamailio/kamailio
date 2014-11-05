#include <string.h>
#include "route_data.h"

#include "../../mem/mem.h"

/* creates a new structure to hold route data in pkg memory 
   NB: remember to free when done!
 */
route_data_t* new_route_data(str* operator_key, str* trunk_id, int priority, str* gw_ip, str* external_trunk_id) {
    struct route_data* route_data;
    char *p;
    int len = sizeof(struct route_data) + operator_key->len + trunk_id->len + gw_ip->len + external_trunk_id->len;
    route_data = (struct route_data*)pkg_malloc(len);
    if (!route_data) {
	LM_ERR("no more pkg memory\n");
	return NULL;
    }
    
    memset(route_data, 0, len);
    
    route_data->priority = priority;
    p = (char*)(route_data + 1);
    
    route_data->operator_key.s = p;
    route_data->operator_key.len = operator_key->len;
    memcpy(p, operator_key->s, operator_key->len);
    p+=operator_key->len;
    
    route_data->trunk_id.s = p;
    memcpy(p, trunk_id->s, trunk_id->len);
    route_data->trunk_id.len = trunk_id->len;
    p+= trunk_id->len;
    
    route_data->ipv4.s = p;
    memcpy(p, gw_ip->s, gw_ip->len);
    route_data->ipv4.len = gw_ip->len;
    p+=gw_ip->len;
    
    route_data->external_trunk_id.s = p;
    memcpy(p, external_trunk_id->s, external_trunk_id->len);
    route_data->external_trunk_id.len = external_trunk_id->len;
    p+=external_trunk_id->len;  
    
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