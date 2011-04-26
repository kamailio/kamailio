#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../ut.h"
#include "dmq.h"
#include "dmqnode.h"
#include "peer.h"
#include "dmq_funcs.h"

int add_notification_peer();
int dmq_notification_callback(struct sip_msg* msg, peer_reponse_t* resp);
int extract_node_list(dmq_node_list_t* update_list, struct sip_msg* msg);
str* build_notification_body();
int build_node_str(dmq_node_t* node, char* buf, int buflen);
int request_nodelist(dmq_node_t* node);
dmq_node_t* add_server_and_notify(str* server_address);

/* helper functions */
extern int notification_callback_f(struct sip_msg* msg, int code, dmq_node_t* node, void* param);
extern dmq_resp_cback_t notification_callback;