#ifndef __EXTRA_ATTRS_H
#define __EXTRA_ATTRS_H


#include "../../parser/msg_parser.h"
#include "../../lib/srdb2/db.h"
#include "../../sr_module.h"

int declare_attr_group(modparam_t type, char* param);

int init_extra_avp_queries(db_ctx_t *ctx);
int init_extra_avp_locks();

int load_extra_attrs(struct sip_msg* msg, char *_table, char* _id);
int save_extra_attrs(struct sip_msg* msg, char* _table, char *_id);
int remove_extra_attrs(struct sip_msg* msg, char *_table, char* _id);

int extra_attrs_fixup(void** param, int param_no);

int lock_extra_attrs(struct sip_msg* msg, char *_table, char* _id);
int unlock_extra_attrs(struct sip_msg* msg, char *_table, char* _id);

#endif
