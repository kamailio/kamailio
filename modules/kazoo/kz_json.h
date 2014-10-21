/*
 * kz_json.h
 *
 *  Created on: Aug 2, 2014
 *      Author: root
 */

#ifndef KZ_JSON_H_
#define KZ_JSON_H_

#include "../../parser/msg_parser.h"
#include <json.h>


int kz_json_get_field(struct sip_msg* msg, char* json, char* field, char* dst);
int kz_json_get_field_ex(str* json, str* field, pv_value_p dst_val);

struct json_object* kz_json_parse(const char *str);
struct json_object* kz_json_get_object(struct json_object* jso, const char *key);

#endif /* KZ_JSON_H_ */
