#ifndef __CLIENT_RO_CONFIG_H
#define __CLIENT_RO_CONFIG_H

typedef struct {
    str origin_host;
    str origin_realm;
    str destination_realm;
    str * service_context_id;
} client_ro_cfg;

#endif
