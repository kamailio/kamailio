#include "dmq.h"
#include "dmqnode.h"

int add_notification_peer();
int dmq_notification_callback(struct sip_msg* msg);
dmq_node_list_t* extract_node_list(struct sip_msg* msg);