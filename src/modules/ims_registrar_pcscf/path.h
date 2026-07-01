#ifndef IMS_REGISTRAR_PCSCF_PATH_H
#define IMS_REGISTRAR_PCSCF_PATH_H

#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"

int pcscf_build_path_uri(str *pcscf_uri, str *out, char *buf, int buf_len);
int pcscf_insert_path_on_register(struct sip_msg *msg, str *path_uri);
int pcscf_format_route_header(str *path, char *buf, int buf_len);

#endif
