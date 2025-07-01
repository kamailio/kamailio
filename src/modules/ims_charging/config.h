#ifndef __CLIENT_RO_CONFIG_H
#define __CLIENT_RO_CONFIG_H

typedef struct
{
	str origin_host;
	str origin_realm;
	str destination_host;
	str destination_realm;
	str *service_context_id;
	int strip_plus_from_e164;
	int use_pani_from_term_invite;
	int node_func;
} client_ro_cfg;

#endif
