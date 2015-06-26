#ifndef KZ_HASH_H
#define KZ_HASH_H

#include "../../lock_ops.h"
#include "kz_amqp.h"

int kz_hash_init();
void kz_hash_destroy();

int kz_cmd_store(kz_amqp_cmd_ptr cmd);
kz_amqp_cmd_ptr kz_cmd_retrieve(str* message_id);


#endif

