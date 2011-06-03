#ifndef HTABLE_H
#define HTABLE_H
#include "../dmq/dmq.h"
#include "ht_serialize.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#define MAX_HT_SERIALIZE_BUF 2048
extern int ht_use_dmq;
extern dmq_api_t ht_dmq_bind;
extern dmq_peer_t* ht_dmq_peer;
extern dmq_resp_cback_t ht_dmq_resp_cback;
#endif