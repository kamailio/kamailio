#include "sca_common.h"

#include "sca_db.h"
#include "sca_hash.h"

#ifndef SCA_H
#define SCA_H

struct _sca_config {
    str		*domain;
    str		*outbound_proxy;
    str		*db_url;
    str		*subs_table;
    str		*state_table;
    int		db_update_interval;
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

    db_func_t		*db_api;
    struct tm_binds	*tm_api;
    sl_api_t		*sl_api;
};
typedef struct _sca_mod		sca_mod;

extern sca_mod		*sca;

#endif /* SCA_H */
