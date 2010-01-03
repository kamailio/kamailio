/*! \file
 * \brief Parser :: Parse if-match header
 *
 * \ingroup parser
 */

#ifndef PARSE_SIPIFMATCH_H
#define PARSE_SIPIFMATCH_H

#include "../str.h"
#include "hf.h"

typedef struct etag {
	str text;       /* Original string representation */
} etag_t;


/*! \brief
 * Parse Sipifmatch HF body
 */
int parse_sipifmatch(struct hdr_field* _h);


/*! \brief
 * Release memory
 */
void free_sipifmatch(str** _e);


#endif /* PARSE_SIPIFMATCH_H */
