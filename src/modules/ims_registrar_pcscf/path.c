/* path.c - build and insert Path header */
#include "path.h"
#include "../../core/data_lump.h"
#include "../../core/parser/parse_uri.h"
#include <stdio.h>

int pcscf_build_path_uri(str *uri, str *out, char *buf, int buf_len)
{
	int len;

	if(!uri || !uri->s || !out || !buf)
		return -1;
	len = snprintf(buf, buf_len, "sip:%.*s;lr", uri->len, uri->s);
	if(len <= 0 || len >= buf_len)
		return -1;
	out->s = buf;
	out->len = len;
	return 0;
}

int pcscf_format_route_header(str *path, char *buf, int buf_len)
{
	int len;

	if(!path || !path->s || !buf || buf_len <= 0)
		return -1;
	len = snprintf(buf, buf_len, "Route: <%.*s>\r\n", path->len, path->s);
	if(len <= 0 || len >= buf_len)
		return -1;
	return len;
}

int pcscf_insert_path_on_register(struct sip_msg *msg, str *path_uri)
{
	char hdr[512];
	int hdr_len;
	struct lump *anchor;

	if(!msg || !path_uri || !path_uri->s)
		return -1;
	hdr_len = snprintf(
			hdr, sizeof(hdr), "Path: <%.*s>\r\n", path_uri->len, path_uri->s);
	if(hdr_len <= 0 || hdr_len >= (int)sizeof(hdr))
		return -1;
	anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, HDR_PATH_T);
	if(!anchor)
		return -1;
	if(!insert_new_lump_before(anchor, hdr, hdr_len, HDR_PATH_T))
		return -1;
	return 0;
}
