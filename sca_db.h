#ifndef SCA_DB_H
#define SCA_DB_H

#include "../../lib/srdb1/db.h"


#define SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS	11

enum {
    SCA_DB_SUBS_SUBSCRIBER_COL = 0,
    SCA_DB_SUBS_AOR_COL = 1,
    SCA_DB_SUBS_EVENT_COL,
    SCA_DB_SUBS_EXPIRES_COL,
    SCA_DB_SUBS_STATE_COL,
    SCA_DB_SUBS_APP_IDX_COL,
    SCA_DB_SUBS_CALL_ID_COL,
    SCA_DB_SUBS_FROM_TAG_COL,
    SCA_DB_SUBS_TO_TAG_COL,
    SCA_DB_SUBS_NOTIFY_CSEQ_COL,
    SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL,

    SCA_DB_SUBS_BOUNDARY,
};

str	**sca_db_subscriptions_columns( void );
void	sca_db_subscriptions_get_value_for_column( int, db_val_t *, void * );


#endif /* SCA_DB_H */
