#ifndef ROUTE_H
#define ROUTE_H

#include "/home/janakj/sip_router/str.h"


typedef struct route {
	str uri;
	unsigned char lr;
	struct route* next;
} route_t;


#endif /* ROUTE_H */
