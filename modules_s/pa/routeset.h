#ifndef ROUTESET_H
#define ROUTESET_H


#include "route.h"


typedef struct routeset {
	route_t* first;
	route_t* last;
} routeset_t;


#endif /* ROUTESET_H */
