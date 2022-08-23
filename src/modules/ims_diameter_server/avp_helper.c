/*
 *
 * Copyright (C) 2016-2017 ng-voice GmbH, Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */

#include "../cdp/cdp_load.h"
#include "../cdp_avp/cdp_avp_mod.h"
#include "avp_helper.h"
#include "ims_diameter_server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "cJSON.h"

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h" 
#include "../../core/trim.h" 
#include "../../core/pvapi.h"
#include "../../core/dset.h"
#include "../../core/basex.h"

#define STRSIZE 8*1024
#define HEXDUMP "hexdump"

// ID of current message
static unsigned int current_msg_id = 0;
static unsigned int current_msg_id_repl = 0;

cJSON * avp2json(AAA_AVP *avp_t) {
	int l, i;
	AAA_AVP *avp_it;	
	char dest[STRSIZE];

	cJSON * avp, * array;

	avp=cJSON_CreateObject();
	LM_DBG("AVP(%p < %p >%p);code=%u,"
		"flags=%x;\nDataType=%u;VendorID=%u;DataLen=%u;\n",
		avp_t->prev,avp_t,avp_t->next,avp_t->code,avp_t->flags,
		avp_t->type,avp_t->vendorId,avp_t->data.len);
	cJSON_AddNumberToObject(avp,"avpCode", avp_t->code);
	cJSON_AddNumberToObject(avp,"vendorId",	avp_t->vendorId);
	cJSON_AddNumberToObject(avp,"DataType",	avp_t->type);
	cJSON_AddNumberToObject(avp,"Flags",	avp_t->flags);
	memset(dest, 0, STRSIZE);
	switch(avp_t->type) {
		case AAA_AVP_STRING_TYPE:
			snprintf(dest, STRSIZE, "%.*s", avp_t->data.len, avp_t->data.s);
			cJSON_AddStringToObject(avp, "string", dest);
			break;
		case AAA_AVP_INTEGER32_TYPE:
			cJSON_AddNumberToObject(avp,"int32", htonl(*((unsigned int*)avp_t->data.s)));
			break;
		case AAA_AVP_INTEGER64_TYPE:
			cJSON_AddNumberToObject(avp,"int64", htonl(*((unsigned int*)avp_t->data.s)));
			break;
		case AAA_AVP_ADDRESS_TYPE:
			i = 1;
			switch (avp_t->data.len) {
				case 4: i=i*0;
				case 6: i=i*2;
					snprintf(dest, STRSIZE,"%d.%d.%d.%d",
							(unsigned char)avp_t->data.s[i+0],
							(unsigned char)avp_t->data.s[i+1],
							(unsigned char)avp_t->data.s[i+2],
							(unsigned char)avp_t->data.s[i+3]);
					cJSON_AddStringToObject(avp, "address", dest);
					break;
				case 16: i=i*0;
				case 18: i=i*2;
					snprintf(dest, STRSIZE, "%x.%x.%x.%x.%x.%x.%x.%x",
							((avp_t->data.s[i+0]<<8)+avp_t->data.s[i+1]),
							((avp_t->data.s[i+2]<<8)+avp_t->data.s[i+3]),
							((avp_t->data.s[i+4]<<8)+avp_t->data.s[i+5]),
							((avp_t->data.s[i+6]<<8)+avp_t->data.s[i+7]),
							((avp_t->data.s[i+8]<<8)+avp_t->data.s[i+9]),
							((avp_t->data.s[i+10]<<8)+avp_t->data.s[i+11]),
							((avp_t->data.s[i+12]<<8)+avp_t->data.s[i+13]),
							((avp_t->data.s[i+14]<<8)+avp_t->data.s[i+15]));
					cJSON_AddStringToObject(avp, "address", dest);
					break;
			}
			break;
		case AAA_AVP_TIME_TYPE:
		default:
			LM_WARN("AAAConvertAVPToString: don't know how to print"
					" this data type [%d] -> tryng hexa\n",avp_t->type);
		case AAA_AVP_DATA_TYPE:
			l = 0;
			for (i=0; i < avp_t->data.len; i++) {
				l+=snprintf(dest+l,STRSIZE-l-1,"%02x", ((unsigned char*)avp_t->data.s)[i]);
			}
			cJSON_AddStringToObject(avp, "data", dest);
			if (avp_t->data.len == 4) {
				cJSON_AddNumberToObject(avp,"int32", htonl(*((unsigned int*)avp_t->data.s)));
			}
			if (avp_t->data.len > 4) {
				memset(dest, 0, STRSIZE);
				l = snprintf(dest, STRSIZE, "%.*s", avp_t->data.len, avp_t->data.s);
				LM_DBG("%.*s (%i/%i)\n", l, dest, l, (int)strlen(dest));
				if (strlen(dest) > 0) {
					cJSON_AddStringToObject(avp, "string", dest);
				} else {
					AAA_AVP_LIST list;
					list = cdp_avp->cdp->AAAUngroupAVPS(avp_t->data);
					array = cJSON_CreateArray();
					avp_it = list.head;
					while(avp_it) {
						LM_DBG("  AVP(%p < %p >%p);code=%u,"
							"flags=%x;\nDataType=%u;VendorID=%u;DataLen=%u;\n",
							avp_it->prev,avp_it,avp_it->next,avp_it->code,avp_it->flags,
							avp_it->type,avp_it->vendorId,avp_it->data.len);

						cJSON_AddItemToArray(array, avp2json(avp_it));

						avp_it = avp_it->next;
					}
					cJSON_AddItemToObject(avp, "list", array);
					cdpb.AAAFreeAVPList(&list);
				}
			}
	}
	return avp;
}

int AAAmsg2json(AAAMessage * request, str * dest) {
	cJSON *root;
	AAA_AVP *avp_t;
	root=cJSON_CreateArray();

	avp_t=request->avpList.head;
	while(avp_t) {
		cJSON_AddItemToArray(root, avp2json(avp_t));
		avp_t = avp_t->next;
	}

	char * out = cJSON_Print(root);
	cJSON_Delete(root);

	if (dest->s) {
		pkg_free(dest->s);
	}

	dest->len = strlen(out);
	dest->s = pkg_malloc(dest->len + 1);
	if (dest->s) {
		memcpy(dest->s, out, dest->len);
		dest->s[dest->len] = '\0';
		free(out);
	} else {
		LM_WARN("Failed to allocate %d bytes for the JSON\n", dest->len);
		free(out);
		return -1;
	}
	return 1;
}


int pv_get_request(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	if (msg->id != current_msg_id) {
		current_msg_id = msg->id;
		AAAmsg2json(request, &requestjson);
	}
	return pv_get_strval(msg, param, res, &requestjson);
}


/**
 * Create and add an AVP to a Diameter message.
 * @param m - Diameter message to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
int diameterserver_add_avp(AAAMessage *m, char *d, int len, int avp_code, int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
	if(m==NULL) {
		LM_ERR("invalid diamemter message parameter\n");
		return 0;
	}
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR("%s: Failed creating avp\n", func);
        return 0;
    }
    if (cdpb.AAAAddAVPToMessage(m, avp, m->avpList.tail) != AAA_ERR_SUCCESS) {
        LM_ERR("%s: Failed adding avp to message\n", func);
       cdpb.AAAFreeAVP(&avp);
        return 0;
    }
    return 1;
}


/**
 * Create and add an AVP to a list of AVPs.
 * @param list - the AVP list to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
int diameterserver_add_avp_list(AAA_AVP_LIST *list, char *d, int len, int avp_code,
	int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
	LM_ERR("%s: Failed creating avp\n", func);
	return 0;
    }
    if (list->tail) {
	avp->prev = list->tail;
	avp->next = 0;
	list->tail->next = avp;
	list->tail = avp;
    } else {
	list->head = avp;
	list->tail = avp;
	avp->next = 0;
	avp->prev = 0;
    }

    return 1;
}

unsigned int parse_hex_half_digit(const char * str) {
  if (*str>='0' && *str<='9') {
    return (*str)-'0';
  } else if (*str>='A' && *str<='F') {
    return 10+(*str)-'A';
  } else if (*str>='a' && *str<='f') {
    return 10+(*str)-'a';
  }
  return 0;
  
}

char* parse_hexdump(const char * hexdump) {
  const char *src = hexdump;
  char *hexdump_copy = strdup(hexdump);
  unsigned char *dst = (unsigned char*)hexdump_copy;
  while (*src) {
    unsigned h=parse_hex_half_digit(src++);
    h=h<<4;
    if (!*src) {
      return hexdump_copy;
    }
    h+=parse_hex_half_digit(src++);
    *dst++ = (unsigned char) h;
  }
  return hexdump_copy;
}

void parselist(AAAMessage *response, AAA_AVP_LIST *list, cJSON * item, int level) {
	int flags;
	char x[4];
	AAA_AVP_LIST avp_list;
	str avp_list_s;

	LM_DBG("------------------------------------------------------------------\n");
	LM_DBG("%i) Item %s (%i / %s)\n", level, item->string, item->valueint, item->valuestring);
	// LM_ERR("Got JSON:\n%s\n",  cJSON_Print(item));

	if (cJSON_GetObjectItem(item,"avpCode")) {
		LM_DBG("%i) avp-Code: %i\n", level, cJSON_GetObjectItem(item,"avpCode")->valueint);
	}
	if (cJSON_GetObjectItem(item,"vendorId")) {
		LM_DBG("%i) vendorId: %i\n", level, cJSON_GetObjectItem(item,"vendorId")->valueint);
	}
	flags = 0;
	if (cJSON_GetObjectItem(item,"Flags")) {
		LM_DBG("%i) Flags: %i\n", level, cJSON_GetObjectItem(item,"Flags")->valueint);
		flags = cJSON_GetObjectItem(item,"Flags")->valueint;
	}
	if (cJSON_GetObjectItem(item,"string")) {
		LM_DBG("%i) String: %s\n", level, cJSON_GetObjectItem(item,"string")->valuestring);
	}
	if (cJSON_GetObjectItem(item,HEXDUMP)) {
		LM_DBG("%i) String: %s\n", level, cJSON_GetObjectItem(item,HEXDUMP)->valuestring);
	}
	if (cJSON_GetObjectItem(item,"int32")) {
		LM_DBG("%i) Integer: %i\n", level, cJSON_GetObjectItem(item,"int32")->valueint);
	}

	if (!cJSON_GetObjectItem(item,"avpCode")) {
		LM_WARN("mandatory field missing: avpCode\n");
                return;
	}
	if (!cJSON_GetObjectItem(item,"vendorId")) {
		LM_WARN("mandatory field missing: vendorId (avpCode %i)\n", cJSON_GetObjectItem(item,"avpCode")->valueint);
		return;
	}

	if ((response == 0) && (list == 0)) {
		LM_WARN("No response nor list provided?!? (%i:%i)\n", cJSON_GetObjectItem(item,"avpCode")->valueint,
			cJSON_GetObjectItem(item,"vendorId")->valueint);
		return;
	}

	if (cJSON_GetObjectItem(item,"list")) {
		LM_DBG("%i) It has a list...\n", level);
		int i;
		avp_list.head = 0;
		avp_list.tail = 0;

		for (i = 0 ; i < cJSON_GetArraySize(cJSON_GetObjectItem(item,"list")) ; i++) {
			cJSON * subitem = cJSON_GetArrayItem(cJSON_GetObjectItem(item,"list"), i);
			parselist(0, &avp_list, subitem, level + 1);
		}
		avp_list_s = cdpb.AAAGroupAVPS(avp_list);
		cdpb.AAAFreeAVPList(&avp_list);

		if(list) {
			diameterserver_add_avp_list(list, avp_list_s.s, avp_list_s.len,
					cJSON_GetObjectItem(item, "avpCode")->valueint, flags,
					cJSON_GetObjectItem(item, "vendorId")->valueint, AVP_FREE_DATA,
					__FUNCTION__);
		} else {
			diameterserver_add_avp(response, avp_list_s.s, avp_list_s.len,
					cJSON_GetObjectItem(item, "avpCode")->valueint, flags,
					cJSON_GetObjectItem(item, "vendorId")->valueint, AVP_FREE_DATA,
					__FUNCTION__);
		}
	} else if (cJSON_GetObjectItem(item,"int32")) {
		set_4bytes(x, cJSON_GetObjectItem(item,"int32")->valueint);
		if (list) {
			diameterserver_add_avp_list(list, x, 4, cJSON_GetObjectItem(item,"avpCode")->valueint, flags,
			  cJSON_GetObjectItem(item,"vendorId")->valueint, AVP_DUPLICATE_DATA, __FUNCTION__);
		} else {
			diameterserver_add_avp(response, x, 4, cJSON_GetObjectItem(item,"avpCode")->valueint, flags,
			  cJSON_GetObjectItem(item,"vendorId")->valueint, AVP_DUPLICATE_DATA, __FUNCTION__);
		}
	} else if (cJSON_GetObjectItem(item,"string")) {
		if (list) {
			diameterserver_add_avp_list(list, cJSON_GetObjectItem(item,"string")->valuestring,
			 strlen(cJSON_GetObjectItem(item,"string")->valuestring), cJSON_GetObjectItem(item,"avpCode")->valueint, flags,
			 cJSON_GetObjectItem(item,"vendorId")->valueint, AVP_DUPLICATE_DATA, __FUNCTION__);
		} else {
			diameterserver_add_avp(response, cJSON_GetObjectItem(item,"string")->valuestring,
			 strlen(cJSON_GetObjectItem(item,"string")->valuestring), cJSON_GetObjectItem(item,"avpCode")->valueint, flags,
			 cJSON_GetObjectItem(item,"vendorId")->valueint, AVP_DUPLICATE_DATA, __FUNCTION__);
		}
	} else if (cJSON_GetObjectItem(item,HEXDUMP)) {
                char * binary_form = parse_hexdump(cJSON_GetObjectItem(item,HEXDUMP)->valuestring);
		if (list) {
                        diameterserver_add_avp_list(list, binary_form,
			 strlen(cJSON_GetObjectItem(item,HEXDUMP)->valuestring) / 2, cJSON_GetObjectItem(item,"avpCode")->valueint, flags,
			 cJSON_GetObjectItem(item,"vendorId")->valueint, AVP_DUPLICATE_DATA, __FUNCTION__);
		} else {
                        diameterserver_add_avp(response, binary_form,
			 strlen(cJSON_GetObjectItem(item,HEXDUMP)->valuestring) / 2, cJSON_GetObjectItem(item,"avpCode")->valueint, flags,
			 cJSON_GetObjectItem(item,"vendorId")->valueint, AVP_DUPLICATE_DATA, __FUNCTION__);
		}
                free(binary_form);
	} else {
		LM_WARN("Not a string, int32, list, hexdump? Invalid field definition... (%i:%i)\n",
			cJSON_GetObjectItem(item,"avpCode")->valueint, cJSON_GetObjectItem(item,"vendorId")->valueint);
	}
}

int addAVPsfromJSON(AAAMessage *response, str * json) {
	if (json == NULL) {
		json = &responsejson;
	}
	if (json->len <= 0) {
		LM_WARN("No JSON Response\n");
		return 0;
	}
	cJSON * root = cJSON_Parse(json->s);
	if (root) {
		int i;
		for (i = 0 ; i < cJSON_GetArraySize(root) ; i++) {
			cJSON * subitem = cJSON_GetArrayItem(root, i);
			parselist(response, 0, subitem, 1);
		}
		// parselist(root, 0);
		cJSON_Delete(root);
		return 1;
	}
	return 0;
}

int pv_get_command(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	return pv_get_uintval(msg, param, res, request->commandCode);
}


int pv_get_application(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	return pv_get_uintval(msg, param, res, request->applicationId);
}

int pv_get_response(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	if ((msg->id != current_msg_id_repl) || (responsejson.len < 0)) {
		return pv_get_null(msg, param, res);
	}
	return pv_get_strval(msg, param, res, &responsejson);
}

int pv_set_response(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val) {
	if (val == NULL)
		return 0;
	if (val->flags&PV_VAL_STR) {
		LM_DBG("Setting response to \"%.*s\" (String)\n", val->rs.len, val->rs.s);
		responsejson.s = val->rs.s;
		responsejson.len = val->rs.len;
		current_msg_id_repl = msg->id;
		return 0;
	}
	return 0;
}

