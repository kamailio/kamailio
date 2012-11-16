#ifndef SCA_DB_H
#define SCA_DB_H

#include "../../lib/srdb1/db.h"


#define SCA_DB_SUBSCRIPTIONS_TABLE_VERSION	0

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

enum {
    SCA_DB_FLAG_NONE = 0,
    SCA_DB_FLAG_INSERT = 1,
    SCA_DB_FLAG_UPDATE,
    SCA_DB_FLAG_DELETE,
};

#define SCA_DB_BIND_STR_VALUE( cv, ct, k, v, c ) \
	((db_key_t *)(k))[ (c) ] = (str *)(ct); \
	((db_val_t *)(v))[ (c) ].type = DB1_STR; \
	((db_val_t *)(v))[ (c) ].nul = 0; \
	((db_val_t *)(v))[ (c) ].val.str_val = (str)(cv); \
	(c)++;

#define SCA_DB_BIND_INT_VALUE( cv, ct, k, v, c ) \
	((db_key_t *)(k))[ (c) ] = (str *)(ct); \
	((db_val_t *)(v))[ (c) ].type = DB1_INT; \
	((db_val_t *)(v))[ (c) ].nul = 0; \
	((db_val_t *)(v))[ (c) ].val.int_val = (int)(cv); \
	(c)++;
	
extern const str      SCA_DB_SUBSCRIBER_COL_NAME;
extern const str      SCA_DB_AOR_COL_NAME;
extern const str      SCA_DB_EVENT_COL_NAME;
extern const str      SCA_DB_EXPIRES_COL_NAME;
extern const str      SCA_DB_STATE_COL_NAME;
extern const str      SCA_DB_APP_IDX_COL_NAME;
extern const str      SCA_DB_CALL_ID_COL_NAME;
extern const str      SCA_DB_FROM_TAG_COL_NAME;
extern const str      SCA_DB_TO_TAG_COL_NAME;
extern const str      SCA_DB_NOTIFY_CSEQ_COL_NAME;
extern const str      SCA_DB_SUBSCRIBE_CSEQ_COL_NAME;

str	**sca_db_subscriptions_columns( void );
void	sca_db_subscriptions_get_value_for_column( int, db_val_t *, void * );
void	sca_db_subscriptions_set_value_for_column( int, db_val_t *, void * );
void	sca_db_subscriptions_bind_value_for_column( int, db_val_t *, void * );


#endif /* SCA_DB_H */
