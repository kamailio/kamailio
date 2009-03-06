#ifndef _PARSER_HELPERS_H
#define _PARSER_HELPERS_H

#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../str.h"

struct sip_uri* parse_to_uri(struct sip_msg* msg);

struct sip_uri* parse_from_uri(struct sip_msg* msg);

/*!
 * get first RR header and print comma separated bodies in oroute
 * - order = 0 normal; order = 1 reverse
 * - nb_recs - input=skip number of rr; output=number of printed rrs
 */
int print_rr_body(struct hdr_field *iroute, str *oroute, int order,
				  unsigned int * nb_recs);


#endif /* _PARSER_HELPERS_H */
