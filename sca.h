#include "sca_common.h"

#include "sca_hash.h"

#ifndef SCA_H
#define SCA_H

struct _sca_config {
    str		*domain;
    str		*outbound_proxy;
    int		hash_table_size;
    int		call_info_max_expires;
    int		line_seize_max_expires;
    int		purge_expired_interval;
};
typedef struct _sca_config	sca_config;

struct _sca_mod {
    sca_config		*cfg;
    sca_hash_table	*subscriptions;
    sca_hash_table	*appearances;

    struct tm_binds	*tm_api;
    sl_api_t		*sl_api;
};
typedef struct _sca_mod		sca_mod;

extern sca_mod		*sca;

#endif /* SCA_H */
