#include "utils.h"
#include <stdio.h>
#include <string.h>

#ifdef SER

int extract_server_contact(struct sip_msg *m, str *dst, int uri_only)
{
	char *tmp = "";
	if (!dst) return -1;

	switch(m->rcv.bind_address->proto){ 
		case PROTO_NONE: break;
		case PROTO_UDP: break;
		case PROTO_TCP: tmp = ";transport=tcp";	break;
		case PROTO_TLS: tmp = ";transport=tls"; break;
		case PROTO_SCTP: tmp = ";transport=sctp"; break;
		default: LOG(L_CRIT, "BUG: extract_server_contact: unknown proto %d\n", m->rcv.bind_address->proto); 
	}
	
	dst->len = 7 + m->rcv.bind_address->name.len + m->rcv.bind_address->port_no_str.len + strlen(tmp);
	if (!uri_only) dst->len += 11;
	dst->s = (char *)cds_malloc(dst->len + 1);
	if (!dst->s) {
		dst->len = 0;
		return -1;
	}
	if (uri_only) {
		snprintf(dst->s, dst->len + 1, "<sip:%.*s:%.*s%s>",
				m->rcv.bind_address->name.len, m->rcv.bind_address->name.s,
				m->rcv.bind_address->port_no_str.len, m->rcv.bind_address->port_no_str.s,
				tmp);
	}
	else {
		snprintf(dst->s, dst->len + 1, "Contact: <sip:%.*s:%.*s%s>\r\n",
				m->rcv.bind_address->name.len, m->rcv.bind_address->name.s,
				m->rcv.bind_address->port_no_str.len, m->rcv.bind_address->port_no_str.s,
				tmp);
	}

	return 0;
}

#endif
