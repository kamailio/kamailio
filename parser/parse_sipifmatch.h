#ifndef PARSE_SIPIFMATCH_H
#define PARSE_SIPIFMATCH_H

#include "../str.h"
#include "hf.h"

typedef struct etag {
	str text;       /* Original string representation */
} etag_t;


/*
 * Parse Sipifmatch HF body
 */
int parse_sipifmatch(struct hdr_field* _h);


/*
 * Release memory
 */
void free_sipifmatch(str** _e);


#endif /* PARSE_SIPIFMATCH_H */
