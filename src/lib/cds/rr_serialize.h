#ifndef __RR_SERIALIZE_H
#define __RR_SERIALIZE_H

#ifdef SER

/* only within ser */
#include <cds/sstr.h>
#include <cds/serialize.h>
#include <parser/parse_rr.h>

int serialize_route_set(sstream_t *ss, rr_t **_r);
int route_set2str(rr_t *rr, str_t *dst_str);
int str2route_set(const str_t *s, rr_t **rr);

#endif

#endif
