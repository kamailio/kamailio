#ifndef _PARSER_HELPERS_H
#define _PARSER_HELPERS_H

#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"

struct sip_uri* parse_to_uri(struct sip_msg* msg);

struct sip_uri* parse_from_uri(struct sip_msg* msg);

#endif /* _PARSER_HELPERS_H */
