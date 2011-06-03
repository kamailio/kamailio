#ifndef HT_SERIALIZE_H
#define HT_SERIALIZE_H

#include "ht_var.h"
#include "../../str.h"
#include "../../basex.h"

int serialize_ht_pair(str* key, pv_value_t* val, str* htname, str* s);
int deserialize_ht_pair(str* key, pv_value_t* val, str* htname, str* src);
#endif