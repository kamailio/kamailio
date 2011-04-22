#include "dmq.h"
#include "dmqnode.h"
#include "peer.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../ut.h"

int add_notification_peer();
int dmq_notification_callback(struct sip_msg* msg, peer_reponse_t* resp);
int extract_node_list(dmq_node_list_t* update_list, struct sip_msg* msg);
str* build_notification_body();
int build_node_str(dmq_node_t* node, char* buf, int buflen);
int request_initial_nodelist();