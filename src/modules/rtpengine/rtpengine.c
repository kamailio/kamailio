/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifndef __USE_BSD
#define __USE_BSD
#endif
#include <netinet/ip.h>
#ifndef __FAVOR_BSD
#define __FAVOR_BSD
#endif
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "../../core/flags.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/error.h"
#include "../../core/fmsg.h"
#include "../../core/forward.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parser_f.h"
#include "../../core/parser/sdp/sdp.h"
#include "../../core/resolve.h"
#include "../../core/timer.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../../core/pt.h"
#include "../../core/timer_proc.h"
#include "../../core/pvar.h"
#include "../../core/lvalue.h"
#include "../../core/msg_translator.h"
#include "../../core/usr_avp.h"
#include "../../core/socket_info.h"
#include "../../core/mod_fix.h"
#include "../../core/dset.h"
#include "../../core/route.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"
#include "../../core/char_msg_val.h"
#include "../../core/utils/srjson.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/rand/fastrand.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/crypto/api.h"
#include "../../modules/lwsc/api.h"
#include "rtpengine.h"
#include "rtpengine_funcs.h"
#include "rtpengine_hash.h"
#include "rtpengine_dmq.h"
#include "bencode.h"
#include "config.h"
#include "api.h"

MODULE_VERSION

#if !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define PF_LOCAL PF_UNIX
#endif

/* NAT UAC test constants */
#define NAT_UAC_TEST_C_1918 0x01
#define NAT_UAC_TEST_RCVD 0x02
#define NAT_UAC_TEST_V_1918 0x04
#define NAT_UAC_TEST_S_1918 0x08
#define NAT_UAC_TEST_RPORT 0x10

#define COOKIE_SIZE 128
#define HOSTNAME_SIZE 100

#define DEFAULT_RTPP_SET_ID 0

#define RTPENGINE_DTMF_EVENT_BUFFER 32768

/* clang-format off */
enum {
	RPC_FOUND_ALL = 2,
	RPC_FOUND_ONE = 1,
	RPC_FOUND_NONE = 0,
};

#define CPORT "22222"

struct ng_flags_parse {
	int via, to, packetize, transport, directional;
	bencode_item_t *dict, *flags, *direction, *replace, *rtcp_mux, *sdes, *t38,
			*received_from, *codec, *codec_strip, *codec_offer,
			*codec_transcode, *codec_mask, *codec_set, *codec_except, *codec_accept,
			*codec_consume, *from_tags;
	str call_id, from_tag, to_tag;
};

static const char *command_strings[] = {
	[OP_OFFER] = "offer",
	[OP_ANSWER] = "answer",
	[OP_DELETE] = "delete",
	[OP_START_RECORDING] = "start recording",
	[OP_QUERY] = "query",
	[OP_PING] = "ping",
	[OP_STOP_RECORDING] = "stop recording",
	[OP_BLOCK_DTMF] = "block DTMF",
	[OP_UNBLOCK_DTMF] = "unblock DTMF",
	[OP_BLOCK_MEDIA] = "block media",
	[OP_UNBLOCK_MEDIA] = "unblock media",
	[OP_SILENCE_MEDIA] = "silence media",
	[OP_UNSILENCE_MEDIA] = "unsilence media",
	[OP_START_FORWARDING] = "start forwarding",
	[OP_STOP_FORWARDING] = "stop forwarding",
	[OP_PLAY_MEDIA] = "play media",
	[OP_STOP_MEDIA] = "stop media",
	[OP_PLAY_DTMF] = "play DTMF",
	[OP_SUBSCRIBE_REQUEST]= "subscribe request",
	[OP_SUBSCRIBE_ANSWER] = "subscribe answer",
	[OP_UNSUBSCRIBE]    = "unsubscribe",
};

static const char *sip_type_strings[] = {
		[SIP_REQUEST] = "sip_request",
		[SIP_REPLY] = "sip_reply",
};

struct minmax_mos_stats {
	str mos_param;
	str at_param;
	str packetloss_param;
	str jitter_param;
	str roundtrip_param;
	str roundtrip_leg_param;
	str samples_param;

	pv_elem_t *mos_pv;
	pv_elem_t *at_pv;
	pv_elem_t *packetloss_pv;
	pv_elem_t *jitter_pv;
	pv_elem_t *roundtrip_pv;
	pv_elem_t *roundtrip_leg_pv;
	pv_elem_t *samples_pv;
};
struct minmax_mos_label_stats {
	int got_any_pvs;

	str label_param;
	pv_elem_t *label_pv;

	struct minmax_mos_stats min, max, average;
};
struct minmax_stats_vals {
	long long int mos;
	long long int at;
	long long int packetloss;
	long long int jitter;
	long long int roundtrip;
	long long int roundtrip_leg;
	long long int samples;
	long long int
			avg_samples; /* our own running count to average the averages */
};

#define RTPE_LIST_VERSION_DELAY 10

typedef struct rtpe_list_version {
	int vernum;
	time_t vertime;
} rtpe_list_version_t;
/* clang-format on */

static rtpe_list_version_t *_rtpe_list_version = NULL;
static int _rtpe_list_vernum_local = 0;

static char *gencookie();
static int rtpp_test(struct rtpp_node *, int, int);
static int start_recording_f(struct sip_msg *, char *, char *);
static int stop_recording_f(struct sip_msg *, char *, char *);
static int block_dtmf_f(struct sip_msg *, char *, char *);
static int unblock_dtmf_f(struct sip_msg *, char *, char *);
static int block_media_f(struct sip_msg *, char *, char *);
static int unblock_media_f(struct sip_msg *, char *, char *);
static int silence_media_f(struct sip_msg *, char *, char *);
static int unsilence_media_f(struct sip_msg *, char *, char *);
static int start_forwarding_f(struct sip_msg *, char *, char *);
static int stop_forwarding_f(struct sip_msg *, char *, char *);
static int play_media_f(struct sip_msg *, char *, char *);
static int stop_media_f(struct sip_msg *, char *, char *);
static int play_dtmf_f(struct sip_msg *, char *, char *);
static int rtpengine_answer1_f(struct sip_msg *, char *, char *);
static int rtpengine_offer1_f(struct sip_msg *, char *, char *);
static int rtpengine_delete1_f(struct sip_msg *, char *, char *);
static int rtpengine_manage1_f(struct sip_msg *, char *, char *);
static int rtpengine_query1_f(struct sip_msg *, char *, char *);
static int rtpengine_info1_f(struct sip_msg *, char *, char *);
static void rtpengine_ping_check_timer(unsigned int ticks, void *);

static int w_rtpengine_query_v(sip_msg_t *msg, char *pfmt, char *pvar);
static int fixup_rtpengine_query_v(void **param, int param_no);
static int fixup_free_rtpengine_query_v(void **param, int param_no);

static int rtpengine_subscribe_request_wrap_f(struct sip_msg *msg, char *str1,
		char *str2, char *str3, char *str4, char *str5);
static int fixup_rtpengine_subscribe_request_v(void **param, int param_no);
static int fixup_free_rtpengine_subscribe_request_v(void **param, int param_no);

static int rtpengine_subscribe_answer_wrap_f(
		struct sip_msg *msg, char *str1, char *str2);
static int rtpengine_unsubscribe_wrap_f(
		struct sip_msg *msg, char *str1, char *str2);


static int parse_flags(struct ng_flags_parse *, struct sip_msg *,
		enum rtpe_operation *, const char *);
static int parse_viabranch_with_param(struct ng_flags_parse *ng_flags,
		struct sip_msg *msg, char *branch_buf, str *p_viabranch,
		str *dst_viabranch);
static int parse_viabranch(struct ng_flags_parse *ng_flags, struct sip_msg *msg,
		str *viabranch, char *branch_buf);
static int parse_from_to_tags(struct ng_flags_parse *ng_flags,
		enum rtpe_operation op, struct sip_msg *msg);

static int bind_rtpengine(rtpengine_api_t *api);

static void rtpengine_copy_streams_to_xavp(
		str *stream_xavp, bencode_item_t *streams);
static int rtpengine_subscribe_request_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op);
static int rtpengine_subscribe_answer_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op);
static int rtpengine_unsubscribe_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op);
static int rtpengine_offer_answer(
		struct sip_msg *msg, void *d, enum rtpe_operation op, int more);
static int rtpengine_subscribe_request(struct rtpengine_session *sess,
		str **to_tag, str *flags, unsigned int subscribe_flags, str *ret_body,
		struct rtpengine_streams *ret_streams);
static int rtpengine_subscribe_answer(
		struct rtpengine_session *sess, str *to_tag, str *flags, str *body);
static int rtpengine_unsubscribe(
		struct rtpengine_session *sess, str *to_tag, str *flags);
static bencode_item_t *w_rtpengine_subscribe_wrap(
		struct rtpengine_session *sess, enum rtpe_operation op, str *to_tag,
		str *flags, unsigned int subscribe_flags, str *body);
static int fixup_set_id(void **param, int param_no);
static int fixup_free_set_id(void **param, int param_no);
static int set_rtpengine_set_f(struct sip_msg *msg, char *str1, char *str2);
static struct rtpp_set *select_rtpp_set(unsigned int id_set);
static struct rtpp_node *select_rtpp_node_new(
		str, str, int, struct rtpp_node **, int);
static struct rtpp_node *select_rtpp_node_old(
		str, str, int, enum rtpe_operation);
static struct rtpp_node *select_rtpp_node(
		str, str, int, struct rtpp_node **, int, enum rtpe_operation);
static int is_queried_node(struct rtpp_node *, struct rtpp_node **, int);
static int build_rtpp_socks(int lmode, int rtest);
static char *send_rtpp_command(struct rtpp_node *, bencode_item_t *, int *);
static int get_extra_id(struct sip_msg *msg, str *id_str);

static int rtpengine_set_store(modparam_t type, void *val);
static int rtpengine_set_dtmf_events_sock(modparam_t type, void *val);
static int rtpengine_add_rtpengine_set(char *rtp_proxies, unsigned int weight,
		int disabled, unsigned int ticks);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int get_ip_type(char *str_addr);
static int get_ip_scope(char *str_addr); // useful for link-local ipv6
static int bind_force_send_ip(int sock_idx);

static int add_rtpp_node_info(
		void *ptrs, struct rtpp_node *crt_rtpp, struct rtpp_set *rtpp_list);
static int rtpp_test_ping(struct rtpp_node *node);

static void rtpengine_dtmf_events_loop(void);
static int rtpengine_raise_dtmf_event(char *buffer, int len);

/* Pseudo-Variables */
static int pv_get_rtpestat_f(struct sip_msg *, pv_param_t *, pv_value_t *);
static int set_rtp_inst_pvar(struct sip_msg *msg, const str *const uri);
static int pv_parse_var(str *inp, pv_elem_t **outp, int *got_any);
static int mos_label_stats_parse(struct minmax_mos_label_stats *mmls);
static void parse_call_stats(bencode_item_t *, struct sip_msg *);

static int control_cmd_tos = -1;
static int rtpengine_allow_op = 0;
static struct rtpp_node **queried_nodes_ptr = NULL;

static unsigned int myseqn = 0;
static str extra_id_pv_param = {NULL, 0};
static char *setid_avp_param = NULL;
int hash_table_tout = 3600;
static int hash_table_size = 256;
static unsigned int setid_default = DEFAULT_RTPP_SET_ID;

static char **rtpp_strings = 0;
static int rtpp_sets = 0; /*used in rtpengine_set_store()*/
static int rtpp_set_count = 0;
static unsigned int current_msg_id = (unsigned int)-1;
/* RTPEngine balancing list */
static struct rtpp_set_head *rtpp_set_list = 0;
static struct rtpp_set *active_rtpp_set = 0;
static struct rtpp_set *selected_rtpp_set_1 = 0;
static struct rtpp_set *selected_rtpp_set_2 = 0;
static struct rtpp_set *default_rtpp_set = 0;

static str body_intermediate;

static str rtp_inst_pv_param = {NULL, 0};
static pv_spec_t *rtp_inst_pvar = NULL;

/* array with the sockets used by rtpproxy (per process)*/
static unsigned int *rtpp_no = 0;
static gen_lock_t *rtpp_no_lock = 0;
static int *rtpp_socks = 0;
static unsigned int rtpp_socks_size = 0;

static avp_flags_t setid_avp_type;
static avp_name_t setid_avp;

static str write_sdp_pvar_str = {NULL, 0};
static pv_spec_t *write_sdp_pvar = NULL;
static int write_sdp_pvar_mode = 0;

static str read_sdp_pvar_str = {NULL, 0};
static pv_spec_t *read_sdp_pvar = NULL;

static str media_duration_pvar_str = {NULL, 0};
static pv_spec_t *media_duration_pvar = NULL;

static str dtmf_event_callid_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_callid_pvar = NULL;

static str dtmf_event_source_tag_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_source_tag_pvar = NULL;

static str dtmf_event_timestamp_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_timestamp_pvar = NULL;

static str dtmf_event_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_pvar = NULL;

static str dtmf_event_source_label_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_source_label_pvar = NULL;

static str dtmf_event_tags_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_tags_pvar = NULL;

static str dtmf_event_type_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_type_pvar = NULL;

static str dtmf_event_source_ip_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_source_ip_pvar = NULL;

static str dtmf_event_duration_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_duration_pvar = NULL;

static str dtmf_event_volume_pvar_str = {NULL, 0};
static pv_spec_t *dtmf_event_volume_pvar = NULL;

static str rtpe_event_callback = STR_NULL;

#define RTPENGINE_SESS_LIMIT_MSG "Parallel session limit reached"
#define RTPENGINE_SESS_LIMIT_MSG_LEN (sizeof(RTPENGINE_SESS_LIMIT_MSG) - 1)

#define RTPENGINE_SESS_OUT_OF_PORTS_MSG "Ran out of ports"
#define RTPENGINE_SESS_OUT_OF_PORTS_MSG_LEN \
	(sizeof(RTPENGINE_SESS_OUT_OF_PORTS_MSG) - 1)

char *force_send_ip_str = "";
int force_send_ip_af = AF_UNSPEC;

static str _rtpe_wsapi = STR_NULL;
lwsc_api_t _rtpe_lwscb = {0};

static enum hash_algo_t hash_algo = RTP_HASH_CALLID;

static str rtpengine_dtmf_event_sock;
static int rtpengine_dtmf_event_fd;
int dtmf_event_rt = -1; /* default disabled */
static int rtpengine_ping_mode = 1;
static int rtpengine_ping_interval = 60;
static int rtpengine_enable_dmq = 0;

/* clang-format off */
typedef struct rtpp_set_link {
	struct rtpp_set *rset;
	pv_spec_t *rpv;
} rtpp_set_link_t;
/* clang-format on */

/* tm */
static struct tm_binds tmb;

static pv_elem_t *extra_id_pv = NULL;


static struct minmax_mos_label_stats global_mos_stats, side_A_mos_stats,
		side_B_mos_stats;
int got_any_mos_pvs;
struct crypto_binds rtpengine_cb;


/* clang-format off */
static cmd_export_t cmds[] = {
	{"set_rtpengine_set", (cmd_function)set_rtpengine_set_f, 1,
		fixup_set_id, fixup_free_set_id, ANY_ROUTE},
	{"set_rtpengine_set", (cmd_function)set_rtpengine_set_f, 2,
		fixup_set_id, fixup_free_set_id, ANY_ROUTE},
	{"start_recording", (cmd_function)start_recording_f, 0, 0, 0, ANY_ROUTE},
	{"start_recording", (cmd_function)start_recording_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"stop_recording", (cmd_function)stop_recording_f, 0, 0, 0, ANY_ROUTE},
	{"stop_recording", (cmd_function)stop_recording_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"block_dtmf", (cmd_function)block_dtmf_f, 0, 0, 0, ANY_ROUTE},
	{"unblock_dtmf", (cmd_function)unblock_dtmf_f, 0, 0, 0, ANY_ROUTE},
	{"block_media", (cmd_function)block_media_f, 0, 0, 0, ANY_ROUTE},
	{"unblock_media", (cmd_function)unblock_media_f, 0, 0, 0, ANY_ROUTE},
	{"silence_media", (cmd_function)silence_media_f, 0, 0, 0, ANY_ROUTE},
	{"unsilence_media", (cmd_function)unsilence_media_f, 0, 0, 0, ANY_ROUTE},
	{"block_dtmf", (cmd_function)block_dtmf_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"unblock_dtmf", (cmd_function)unblock_dtmf_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"block_media", (cmd_function)block_media_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"unblock_media", (cmd_function)unblock_media_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"silence_media", (cmd_function)silence_media_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"unsilence_media", (cmd_function)unsilence_media_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"start_forwarding", (cmd_function)start_forwarding_f, 0, 0, 0, ANY_ROUTE},
	{"stop_forwarding", (cmd_function)stop_forwarding_f, 0, 0, 0, ANY_ROUTE},
	{"start_forwarding", (cmd_function)start_forwarding_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"stop_forwarding", (cmd_function)stop_forwarding_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"play_media", (cmd_function)play_media_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"play_media", (cmd_function)play_media_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"stop_media", (cmd_function)stop_media_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"stop_media", (cmd_function)stop_media_f, 0, 0, 0, ANY_ROUTE},
	{"play_dtmf", (cmd_function)play_dtmf_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_offer", (cmd_function)rtpengine_offer1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_offer", (cmd_function)rtpengine_offer1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_offer", (cmd_function)rtpengine_offer1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_answer", (cmd_function)rtpengine_answer1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_answer", (cmd_function)rtpengine_answer1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_answer", (cmd_function)rtpengine_answer1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_info", (cmd_function)rtpengine_info1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_info", (cmd_function)rtpengine_info1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_info", (cmd_function)rtpengine_info1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_manage", (cmd_function)rtpengine_manage1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_manage", (cmd_function)rtpengine_manage1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_manage", (cmd_function)rtpengine_manage1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_delete", (cmd_function)rtpengine_delete1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_delete", (cmd_function)rtpengine_delete1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_delete", (cmd_function)rtpengine_delete1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_destroy", (cmd_function)rtpengine_delete1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_destroy", (cmd_function)rtpengine_delete1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_destroy", (cmd_function)rtpengine_delete1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_query", (cmd_function)rtpengine_query1_f, 0, 0, 0, ANY_ROUTE},
	{"rtpengine_query", (cmd_function)rtpengine_query1_f, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_query", (cmd_function)rtpengine_query1_f, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_query_v", (cmd_function)w_rtpengine_query_v, 2,
		fixup_rtpengine_query_v, fixup_free_rtpengine_query_v, ANY_ROUTE},
	{"bind_rtpengine", (cmd_function)bind_rtpengine, 0, 0, 0, 0},
	{"rtpengine_subscribe_request", (cmd_function)rtpengine_subscribe_request_wrap_f, 4,
	fixup_rtpengine_subscribe_request_v, fixup_free_rtpengine_subscribe_request_v, ANY_ROUTE},
	{"rtpengine_subscribe_request", (cmd_function)rtpengine_subscribe_request_wrap_f, 5,
	fixup_rtpengine_subscribe_request_v, fixup_free_rtpengine_subscribe_request_v, ANY_ROUTE},
	{"rtpengine_subscribe_answer", (cmd_function)rtpengine_subscribe_answer_wrap_f, 1,
	fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_subscribe_answer", (cmd_function)rtpengine_subscribe_answer_wrap_f, 2,
	fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"rtpengine_unsubscribe", (cmd_function)rtpengine_unsubscribe_wrap_f, 1,
	fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"rtpengine_unsubscribe", (cmd_function)rtpengine_unsubscribe_wrap_f, 2,
	fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{{"rtpstat", (sizeof("rtpstat") - 1)}, /* RTP-Statistics */
			PVT_OTHER, pv_get_rtpestat_f, 0, 0, 0, 0, 0},
	{{"rtpestat", (sizeof("rtpestat") - 1)}, /* RTP-Statistics */
			PVT_OTHER, pv_get_rtpestat_f, 0, 0, 0, 0, 0},
	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"rtpengine_sock", PARAM_STRING | PARAM_USE_FUNC,
			(void *)rtpengine_set_store},
	{"rtpengine_disable_tout", PARAM_INT,
			&default_rtpengine_cfg.rtpengine_disable_tout},
	{"aggressive_redetection", PARAM_INT,
			&default_rtpengine_cfg.aggressive_redetection},
	{"rtpengine_retr", PARAM_INT, &default_rtpengine_cfg.rtpengine_retr},
	{"queried_nodes_limit", PARAM_INT,
			&default_rtpengine_cfg.queried_nodes_limit},
	{"rtpengine_tout_ms", PARAM_INT,
			&default_rtpengine_cfg.rtpengine_tout_ms},
	{"rtpengine_allow_op", PARAM_INT, &rtpengine_allow_op},
	{"control_cmd_tos", PARAM_INT, &control_cmd_tos},
	{"ping_mode", PARAM_INT, &rtpengine_ping_mode},
	{"ping_interval", PARAM_INT, &rtpengine_ping_interval},
	{"db_url", PARAM_STR, &rtpp_db_url},
	{"table_name", PARAM_STR, &rtpp_table_name},
	{"setid_col", PARAM_STR, &rtpp_setid_col},
	{"url_col", PARAM_STR, &rtpp_url_col},
	{"weight_col", PARAM_STR, &rtpp_weight_col},
	{"disabled_col", PARAM_STR, &rtpp_disabled_col},
	{"extra_id_pv", PARAM_STR, &extra_id_pv_param},
	{"setid_avp", PARAM_STRING, &setid_avp_param},
	{"force_send_interface", PARAM_STRING, &force_send_ip_str},
	{"rtp_inst_pvar", PARAM_STR, &rtp_inst_pv_param},
	{"write_sdp_pv", PARAM_STR, &write_sdp_pvar_str},
	{"write_sdp_pv_mode", PARAM_INT, &write_sdp_pvar_mode},
	{"read_sdp_pv", PARAM_STR, &read_sdp_pvar_str},
	{"hash_table_tout", PARAM_INT, &hash_table_tout},
	{"hash_table_size", PARAM_INT, &hash_table_size},
	{"setid_default", PARAM_INT, &setid_default},
	{"media_duration", PARAM_STR, &media_duration_pvar_str},
	{"hash_algo", PARAM_INT, &hash_algo},
	{"dtmf_events_sock", PARAM_STRING | PARAM_USE_FUNC,
			(void *)rtpengine_set_dtmf_events_sock},
	{"dtmf_event_callid", PARAM_STR, &dtmf_event_callid_pvar_str},
	{"dtmf_event_source_tag", PARAM_STR, &dtmf_event_source_tag_pvar_str},
	{"dtmf_event_timestamp", PARAM_STR, &dtmf_event_timestamp_pvar_str},
	{"dtmf_event", PARAM_STR, &dtmf_event_pvar_str},
	{"dtmf_event_source_label", PARAM_STR, &dtmf_event_source_label_pvar_str},
	{"dtmf_event_tags", PARAM_STR, &dtmf_event_tags_pvar_str},
	{"dtmf_event_type", PARAM_STR, &dtmf_event_type_pvar_str},
	{"dtmf_event_source_ip", PARAM_STR, &dtmf_event_source_ip_pvar_str},
	{"dtmf_event_duration", PARAM_STR, &dtmf_event_duration_pvar_str},
	{"dtmf_event_volume", PARAM_STR, &dtmf_event_volume_pvar_str},
	{"event_callback",  PARAM_STR, &rtpe_event_callback},
	{"enable_dmq", PARAM_INT, &rtpengine_enable_dmq},
	/* MOS stats output */
	/* global averages */
	{"mos_min_pv", PARAM_STR, &global_mos_stats.min.mos_param},
	{"mos_min_at_pv", PARAM_STR, &global_mos_stats.min.at_param},
	{"mos_min_packetloss_pv", PARAM_STR,
			&global_mos_stats.min.packetloss_param},
	{"mos_min_jitter_pv", PARAM_STR, &global_mos_stats.min.jitter_param},
	{"mos_min_roundtrip_pv", PARAM_STR,
			&global_mos_stats.min.roundtrip_param},
	{"mos_min_roundtrip_leg_pv", PARAM_STR,
			&global_mos_stats.min.roundtrip_leg_param},
	{"mos_max_pv", PARAM_STR, &global_mos_stats.max.mos_param},
	{"mos_max_at_pv", PARAM_STR, &global_mos_stats.max.at_param},
	{"mos_max_packetloss_pv", PARAM_STR,
			&global_mos_stats.max.packetloss_param},
	{"mos_max_jitter_pv", PARAM_STR, &global_mos_stats.max.jitter_param},
	{"mos_max_roundtrip_pv", PARAM_STR,
			&global_mos_stats.max.roundtrip_param},
	{"mos_max_roundtrip_leg_pv", PARAM_STR,
			&global_mos_stats.max.roundtrip_leg_param},
	{"mos_average_pv", PARAM_STR, &global_mos_stats.average.mos_param},
	{"mos_average_packetloss_pv", PARAM_STR,
			&global_mos_stats.average.packetloss_param},
	{"mos_average_jitter_pv", PARAM_STR,
			&global_mos_stats.average.jitter_param},
	{"mos_average_roundtrip_pv", PARAM_STR,
			&global_mos_stats.average.roundtrip_param},
	{"mos_average_roundtrip_leg_pv", PARAM_STR,
			&global_mos_stats.average.roundtrip_leg_param},
	{"mos_average_samples_pv", PARAM_STR,
			&global_mos_stats.average.samples_param},

	/* designated side A */
	{"mos_A_label_pv", PARAM_STR, &side_A_mos_stats.label_param},
	{"mos_min_A_pv", PARAM_STR, &side_A_mos_stats.min.mos_param},
	{"mos_min_at_A_pv", PARAM_STR, &side_A_mos_stats.min.at_param},
	{"mos_min_packetloss_A_pv", PARAM_STR,
			&side_A_mos_stats.min.packetloss_param},
	{"mos_min_jitter_A_pv", PARAM_STR, &side_A_mos_stats.min.jitter_param},
	{"mos_min_roundtrip_A_pv", PARAM_STR,
			&side_A_mos_stats.min.roundtrip_param},
	{"mos_min_roundtrip_leg_A_pv", PARAM_STR,
			&side_A_mos_stats.min.roundtrip_leg_param},
	{"mos_max_A_pv", PARAM_STR, &side_A_mos_stats.max.mos_param},
	{"mos_max_at_A_pv", PARAM_STR, &side_A_mos_stats.max.at_param},
	{"mos_max_packetloss_A_pv", PARAM_STR,
			&side_A_mos_stats.max.packetloss_param},
	{"mos_max_jitter_A_pv", PARAM_STR, &side_A_mos_stats.max.jitter_param},
	{"mos_max_roundtrip_A_pv", PARAM_STR,
			&side_A_mos_stats.max.roundtrip_param},
	{"mos_max_roundtrip_leg_A_pv", PARAM_STR,
			&side_A_mos_stats.max.roundtrip_leg_param},
	{"mos_average_A_pv", PARAM_STR, &side_A_mos_stats.average.mos_param},
	{"mos_average_packetloss_A_pv", PARAM_STR,
			&side_A_mos_stats.average.packetloss_param},
	{"mos_average_jitter_A_pv", PARAM_STR,
			&side_A_mos_stats.average.jitter_param},
	{"mos_average_roundtrip_A_pv", PARAM_STR,
			&side_A_mos_stats.average.roundtrip_param},
	{"mos_average_roundtrip_leg_A_pv", PARAM_STR,
			&side_A_mos_stats.average.roundtrip_leg_param},
	{"mos_average_samples_A_pv", PARAM_STR,
			&side_A_mos_stats.average.samples_param},

	/* designated side B */
	{"mos_B_label_pv", PARAM_STR, &side_B_mos_stats.label_param},
	{"mos_min_B_pv", PARAM_STR, &side_B_mos_stats.min.mos_param},
	{"mos_min_at_B_pv", PARAM_STR, &side_B_mos_stats.min.at_param},
	{"mos_min_packetloss_B_pv", PARAM_STR,
			&side_B_mos_stats.min.packetloss_param},
	{"mos_min_jitter_B_pv", PARAM_STR, &side_B_mos_stats.min.jitter_param},
	{"mos_min_roundtrip_B_pv", PARAM_STR,
			&side_B_mos_stats.min.roundtrip_param},
	{"mos_min_roundtrip_leg_B_pv", PARAM_STR,
			&side_B_mos_stats.min.roundtrip_leg_param},
	{"mos_max_B_pv", PARAM_STR, &side_B_mos_stats.max.mos_param},
	{"mos_max_at_B_pv", PARAM_STR, &side_B_mos_stats.max.at_param},
	{"mos_max_packetloss_B_pv", PARAM_STR,
			&side_B_mos_stats.max.packetloss_param},
	{"mos_max_jitter_B_pv", PARAM_STR, &side_B_mos_stats.max.jitter_param},
	{"mos_max_roundtrip_B_pv", PARAM_STR,
			&side_B_mos_stats.max.roundtrip_param},
	{"mos_max_roundtrip_leg_B_pv", PARAM_STR,
			&side_B_mos_stats.max.roundtrip_leg_param},
	{"mos_average_B_pv", PARAM_STR, &side_B_mos_stats.average.mos_param},
	{"mos_average_packetloss_B_pv", PARAM_STR,
			&side_B_mos_stats.average.packetloss_param},
	{"mos_average_jitter_B_pv", PARAM_STR,
			&side_B_mos_stats.average.jitter_param},
	{"mos_average_roundtrip_B_pv", PARAM_STR,
			&side_B_mos_stats.average.roundtrip_param},
	{"mos_average_roundtrip_leg_B_pv", PARAM_STR,
			&side_B_mos_stats.average.roundtrip_leg_param},
	{"mos_average_samples_B_pv", PARAM_STR,
			&side_B_mos_stats.average.samples_param},

	{"wsapi", PARAM_STR, &_rtpe_wsapi},

	{0, 0, 0}
};

struct module_exports exports = {
	"rtpengine",	 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* cmd (cfg function) exports */
	params,			 /* param exports */
	0,				 /* RPC method exports */
	mod_pvs,		 /* pseudo-variables exports */
	0,				 /* response handling function */
	mod_init,		 /* module init function */
	child_init,		 /* per-child init function */
	mod_destroy		 /* module destroy function */
};
/* clang-format on */

/* check if the node is already queried */
static int is_queried_node(struct rtpp_node *node,
		struct rtpp_node **queried_nodes_list, int queried_nodes)
{
	int i;

	if(!queried_nodes_list) {
		return 0;
	}

	for(i = 0; i < queried_nodes; i++) {
		if(node == queried_nodes_list[i]) {
			return 1;
		}
	}

	return 0;
}

/* hide the node from display and disable it permanent */
int rtpengine_delete_node(struct rtpp_node *rtpp_node)
{
	rtpp_node->rn_displayed = 0;
	rtpp_node->rn_disabled = 1;
	rtpp_node->rn_recheck_ticks = RTPENGINE_MAX_RECHECK_TICKS;

	return 1;
}


/**
 * @brief Deletes all nodes in a given RTP engine set.
 *
 * @details Iterates over all nodes in the specified RTP engine set,
 * calling rtpengine_delete_node on each node to remove it. It ensures thread
 * safety by acquiring and releasing the set's lock.
 *
 * @param rtpp_list The RTP engine set from which to delete nodes.
 * @return 1 on success.
 */
int rtpengine_delete_node_set(struct rtpp_set *rtpp_list)
{
	struct rtpp_node *rtpp_node;

	lock_get(rtpp_list->rset_lock);
	for(rtpp_node = rtpp_list->rn_first; rtpp_node != NULL;
			rtpp_node = rtpp_node->rn_next) {
		rtpengine_delete_node(rtpp_node);
	}
	lock_release(rtpp_list->rset_lock);

	return 1;
}


/**
 * @brief Deletes all nodes across all RTP engine sets.
 *
 * @details Iterates over all RTP engine sets, acquiring and releasing the
 * set's lock for each set. It then calls rtpengine_delete_node_set to delete all
 * nodes within each set. This ensures thread safety and complete removal of all
 * nodes.
 *
 * @return 1 on success.
 */
int rtpengine_delete_node_all()
{
	struct rtpp_set *rtpp_list;

	if(!rtpp_set_list) {
		return 1;
	}

	lock_get(rtpp_set_list->rset_head_lock);
	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != NULL;
			rtpp_list = rtpp_list->rset_next) {
		rtpengine_delete_node_set(rtpp_list);
	}
	lock_release(rtpp_set_list->rset_head_lock);

	return 1;
}


/**
 * @brief Determines the type of IP address from a given string.
 *
 * @details Attempts to resolve the given string as an IP address and
 * determines its type (IPv4 or IPv6). It uses getaddrinfo to perform the
 * resolution and checks the family of the returned address information.
 *
 * @param str_addr The string representation of the IP address to check.
 * @return The family of the IP address (AF_INET for IPv4, AF_INET6 for IPv6),
 *         or -1 if the address is invalid or of an unknown format.
 */
static int get_ip_type(char *str_addr)
{
	struct addrinfo hint, *info = NULL;
	int ret;

	memset(&hint, '\0', sizeof hint);
	hint.ai_family = PF_UNSPEC;
	hint.ai_flags = AI_NUMERICHOST;

	ret = getaddrinfo(str_addr, NULL, &hint, &info);
	if(ret) {
		/* Invalid ip addinfos */
		return -1;
	}

	if(info->ai_family == AF_INET) {
		LM_DBG("%s is an ipv4 addinfos\n", str_addr);
	} else if(info->ai_family == AF_INET6) {
		LM_DBG("%s is an ipv6 addinfos\n", str_addr);
	} else {
		LM_DBG("%s is an unknown addinfos format AF=%d\n", str_addr,
				info->ai_family);
		freeaddrinfo(info);
		return -1;
	}

	ret = info->ai_family;

	freeaddrinfo(info);

	return ret;
}


/**
 * @brief Determines the scope of a given IPv6 address.
 *
 * @details Iterates through all network interfaces to find the scope of a given IPv6 address.
 * It uses getifaddrs to get a list of all network interfaces and their addresses, then iterates
 * through the list to find the interface that matches the given address. It uses getnameinfo to
 * convert the binary address to a string representation for comparison.
 *
 * @param str_addr The string representation of the IPv6 address to find the scope for.
 * @return The scope ID of the IPv6 address if found, -1 otherwise.
 */
static int get_ip_scope(char *str_addr)
{
	struct ifaddrs *ifaddr, *ifa;
	struct sockaddr_in6 *in6;
	char str_if_ip[NI_MAXHOST];
	int ret = -1;

	if(getifaddrs(&ifaddr) == -1) {
		LM_ERR("getifaddrs() failed: %s\n", gai_strerror(ret));
		return -1;
	}

	for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		in6 = (struct sockaddr_in6 *)ifa->ifa_addr;

		if(ifa->ifa_addr == NULL)
			continue;

		if(ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ret = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), str_if_ip,
				NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if(ret != 0) {
			LM_ERR("getnameinfo() failed: %s\n", gai_strerror(ret));
			return -1;
		}

		if(strstr(str_if_ip, str_addr)) {
			LM_INFO("dev: %-8s address: <%s> scope %d\n", ifa->ifa_name,
					str_if_ip, in6->sin6_scope_id);
			ret = in6->sin6_scope_id;
			break;
		}
	}

	freeifaddrs(ifaddr);

	return ret;
}


static int bind_force_send_ip(int sock_idx)
{
	struct sockaddr_in tmp, ip4addr;
	struct sockaddr_in6 tmp6, ip6addr;
	char str_addr[INET_ADDRSTRLEN];
	char str_addr6[INET6_ADDRSTRLEN];
	socklen_t sock_len = sizeof(struct sockaddr);
	int ret, scope;

	switch(force_send_ip_af) {
		case AF_INET:
			memset(&ip4addr, 0, sizeof(ip4addr));
			ip4addr.sin_family = AF_INET;
			ip4addr.sin_port = htons(0);
			if(inet_pton(AF_INET, force_send_ip_str, &ip4addr.sin_addr) != 1) {
				LM_ERR("failed to parse IPv4 address %s\n", force_send_ip_str);
				return -1;
			}

			if(bind(rtpp_socks[sock_idx], (struct sockaddr *)&ip4addr,
					   sizeof(ip4addr))
					< 0) {
				LM_ERR("can't bind socket to required ipv4 interface\n");
				return -1;
			}

			memset(&tmp, 0, sizeof(tmp));
			if(getsockname(rtpp_socks[sock_idx], (struct sockaddr *)&tmp,
					   &sock_len))
				LM_ERR("could not determine local socket name\n");
			else {
				inet_ntop(AF_INET, &tmp.sin_addr, str_addr, INET_ADDRSTRLEN);
				LM_DBG("Binding on %s:%d\n", str_addr, ntohs(tmp.sin_port));
			}

			break;

		case AF_INET6:
			if((scope = get_ip_scope(force_send_ip_str)) < 0) {
				LM_ERR("can't get the ipv6 interface scope\n");
				return -1;
			}
			memset(&ip6addr, 0, sizeof(ip6addr));
			ip6addr.sin6_family = AF_INET6;
			ip6addr.sin6_port = htons(0);
			ip6addr.sin6_scope_id = scope;
			if(inet_pton(AF_INET6, force_send_ip_str, &ip6addr.sin6_addr)
					!= 1) {
				LM_ERR("failed to parse IPv6 address %s\n", force_send_ip_str);
				return -1;
			}

			if((ret = bind(rtpp_socks[sock_idx], (struct sockaddr *)&ip6addr,
						sizeof(ip6addr)))
					< 0) {
				LM_ERR("can't bind socket to required ipv6 interface\n");
				LM_ERR("ret=%d (%s:%d)\n", ret, strerror(errno), errno);
				return -1;
			}

			memset(&tmp6, 0, sizeof(tmp6));
			if(getsockname(rtpp_socks[sock_idx], (struct sockaddr *)&tmp6,
					   &sock_len))
				LM_ERR("could not determine local socket name\n");
			else {
				inet_ntop(
						AF_INET6, &tmp6.sin6_addr, str_addr6, INET6_ADDRSTRLEN);
				LM_DBG("Binding on ipv6 %s:%d\n", str_addr6,
						ntohs(tmp6.sin6_port));
			}

			break;

		default:
			LM_DBG("force_send_ip_str not specified in .cfg file!\n");
			break;
	}

	return 0;
}

static inline int str_eq(const str *p, const char *q)
{
	int l = strlen(q);
	if(p->len != l)
		return 0;
	if(memcmp(p->s, q, l))
		return 0;
	return 1;
}

/**
 * @brief Checks if a string starts with a given prefix and updates the string to
 * 	remove the prefix if it matches.
 *
 * @details Checks if the string pointed to by 'p' starts with the string 'q'.
 * If it does, it updates the string 'out' to be a copy of 'p' with 'q' removed from the beginning.
 *
 * @param p The string to check for the prefix.
 * @param q The prefix to check for.
 * @param out The string that will be updated if 'p' starts with 'q'.
 * @return 1 if 'p' starts with 'q', 0 otherwise.
 */
static inline int str_prefix(const str *p, const char *q, str *out)
{
	int l = strlen(q);
	if(p->len < l)
		return 0;
	if(memcmp(p->s, q, l))
		return 0;
	*out = *p;
	out->s += l;
	out->len -= l;
	return 1;
}

/**
 * @brief Checks if a string starts with a given prefix and updates the string
 * to remove the prefix if it matches.
 *
 * @details If the string is exactly equal to the prefix, it sets the output to a provided value.
 *
 * handle either "foo-bar" or "foo=bar" from flags
 *
 * @param p The string to check for the prefix.
 * @param q The prefix to check for.
 * @param v The value to set the output to if p is exactly equal to q.
 * @param out The string that will be updated if p starts with q.
 * @return 1 if p starts with q or is exactly equal to q (and valid conditions are met), 0 otherwise.
 */
static inline int str_key_val_prefix(
		const str *p, const char *q, const str *v, str *out)
{
	if(str_eq(p, q)) {
		if(!v->s || !v->len)
			return 0;

		*out = *v;
		return 1;
	}
	if(!str_prefix(p, q, out))
		return 0;
	if(out->len < 2)
		return 0;
	if(*out->s != '-')
		return 0;
	out->s++;
	out->len--;
	return 1;
}

/**
 * @brief Stores a new RTP engine set URL from cfg modparam.
 *
 * @details Dynamically allocates memory to store a new RTP engine set URL.
 * It checks if the input URL is valid and if there is enough memory available.
 * If successful, it increments the counter of stored RTP engine sets.
 *
 * @param type The type of the module parameter.
 * @param val The value of the module parameter, expected to be a string.
 * @return 0 on success, -1 on failure.
 */
static int rtpengine_set_store(modparam_t type, void *val)
{

	char *p;
	int len;

	p = (char *)val;

	if(p == 0 || *p == '\0') {
		return 0;
	}

	if(rtpp_sets == 0) {
		rtpp_strings = (char **)pkg_malloc(sizeof(char *));
		if(!rtpp_strings) {
			LM_ERR("no pkg memory left\n");
			return -1;
		}
	} else { /*realloc to make room for the current set*/
		rtpp_strings = (char **)pkg_reallocxf(
				rtpp_strings, (rtpp_sets + 1) * sizeof(char *));
		if(!rtpp_strings) {
			LM_ERR("no pkg memory left\n");
			return -1;
		}
	}

	/*allocate for the current set of urls*/
	len = strlen(p);
	rtpp_strings[rtpp_sets] = (char *)pkg_malloc((len + 1) * sizeof(char));

	if(!rtpp_strings[rtpp_sets]) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	memcpy(rtpp_strings[rtpp_sets], p, len);
	rtpp_strings[rtpp_sets][len] = '\0';
	rtpp_sets++;

	return 0;
}

/**
 * @brief Searches for a specific RTP engine node within a given RTP engine set based on its URL.
 *
 * @details This function iterates through the linked list of RTP engine nodes within the specified
 * RTP engine set, comparing each node's URL with the provided URL. If a match is found, the function
 * returns the matched node.
 *
 * @param rtpp_list The RTP engine set to search within.
 * @param url The URL to match against the RTP engine nodes.
 * @return The RTP engine node with the matching URL, or NULL if no match is found.
 */
struct rtpp_node *get_rtpp_node(struct rtpp_set *rtpp_list, str *url)
{
	struct rtpp_node *rtpp_node;

	if(rtpp_list == NULL) {
		return NULL;
	}

	lock_get(rtpp_list->rset_lock);
	rtpp_node = rtpp_list->rn_first;
	while(rtpp_node) {
		if(str_strcmp(&rtpp_node->rn_url, url) == 0) {
			lock_release(rtpp_list->rset_lock);
			return rtpp_node;
		}
		rtpp_node = rtpp_node->rn_next;
	}
	lock_release(rtpp_list->rset_lock);

	return NULL;
}

/**
 * @brief Retrieves or creates an RTP engine set based on the provided set ID.
 *
 * @details Search for an RTP engine set with the specified ID within the list of RTP engine sets.
 * If the set is found, it is returned. If not, a new RTP engine set is created with the specified
 * ID and added to the list.
 *
 * @param set_id The ID of the RTP engine set to retrieve or create.
 * @return The RTP engine set with the specified ID, or NULL if creation fails due to memory issues.
 */
struct rtpp_set *get_rtpp_set(unsigned int set_id, unsigned int create_new)
{
	struct rtpp_set *rtpp_list;
	unsigned int my_current_id = 0;
	int new_list;

	my_current_id = set_id;
	/*search for the current_id*/
	lock_get(rtpp_set_list->rset_head_lock);
	rtpp_list = rtpp_set_list ? rtpp_set_list->rset_first : 0;
	while(rtpp_list != 0 && rtpp_list->id_set != my_current_id)
		rtpp_list = rtpp_list->rset_next;

	if(rtpp_list == NULL
			&& create_new) { /*if a new id_set : add a new set of rtpp*/
		rtpp_list = shm_malloc(sizeof(struct rtpp_set));
		if(!rtpp_list) {
			lock_release(rtpp_set_list->rset_head_lock);
			LM_ERR("no shm memory left to create new rtpengine set %u\n",
					my_current_id);
			return NULL;
		}
		memset(rtpp_list, 0, sizeof(struct rtpp_set));
		rtpp_list->id_set = my_current_id;
		rtpp_list->rset_lock = lock_alloc();
		if(!rtpp_list->rset_lock) {
			lock_release(rtpp_set_list->rset_head_lock);
			LM_ERR("no shm memory left to create rtpengine set lock\n");
			shm_free(rtpp_list);
			rtpp_list = NULL;
			return NULL;
		}
		if(lock_init(rtpp_list->rset_lock) == 0) {
			lock_release(rtpp_set_list->rset_head_lock);
			LM_ERR("could not init rtpengine set lock\n");
			lock_dealloc((void *)rtpp_list->rset_lock);
			rtpp_list->rset_lock = NULL;
			shm_free(rtpp_list);
			rtpp_list = NULL;
			return NULL;
		}
		new_list = 1;
	} else {
		new_list = 0;
	}

	if(new_list) {
		/*update the list of set info*/
		if(!rtpp_set_list->rset_first) {
			rtpp_set_list->rset_first = rtpp_list;
		} else {
			rtpp_set_list->rset_last->rset_next = rtpp_list;
		}

		rtpp_set_list->rset_last = rtpp_list;
		rtpp_set_count++;

		if(my_current_id == setid_default) {
			default_rtpp_set = rtpp_list;
		}
	}
	lock_release(rtpp_set_list->rset_head_lock);

	return rtpp_list;
}

int get_rtpp_set_id_by_node(struct rtpp_node *node)
{
	struct rtpp_set *rtpp_list;
	struct rtpp_node *crt_rtpp;

	lock_get(rtpp_set_list->rset_head_lock);
	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != NULL;
			rtpp_list = rtpp_list->rset_next) {

		lock_get(rtpp_list->rset_lock);
		for(crt_rtpp = rtpp_list->rn_first; crt_rtpp != NULL;
				crt_rtpp = crt_rtpp->rn_next) {

			if(crt_rtpp->idx == node->idx) {
				break;
			}
		}
		lock_release(rtpp_list->rset_lock);
		if(crt_rtpp != NULL)
			break;
	}
	lock_release(rtpp_set_list->rset_head_lock);

	if(crt_rtpp != NULL)
		return rtpp_list->id_set;
	else
		return 0;
}

/**
 * @brief Adds a new RTP engine instance to the given RTP engine set.
 *
 * @details Parse the given RTP engine string, which can include a weight and/or a disabled flag.
 * It then adds a new RTP engine instance to the specified RTP engine set with the parsed properties.
 *
 * @param rtpp_list The RTP engine set to which the new instance will be added.
 * @param rtpengine The string representation of the RTP engine instance, including its URL and optional weight and disabled flag.
 * @param weight The weight of the RTP engine instance, used for load balancing.
 * @param disabled A flag indicating if the RTP engine instance is disabled.
 * @param ticks The number of ticks until the RTP engine instance is rechecked.
 * @param isDB A flag indicating if the RTP engine instance is being added from the database.
 *
 * @return Returns 0 on success, -1 on failure.
 */
int add_rtpengine_socks(struct rtpp_set *rtpp_list, char *rtpengine,
		unsigned int weight, int disabled, unsigned int ticks, int isDB)
{
	/* Make rtpengine instances list. */
	char *p, *p1, *p2, *plim;
	struct rtpp_node *pnode;
	struct rtpp_node *rtpp_node;
	unsigned int local_weight, port;
	str s1;

	p = rtpengine;
	plim = p + strlen(p);

	for(;;) {
		local_weight = weight;
		while(*p && isspace((int)*p))
			++p;
		if(p >= plim)
			break;
		p1 = p;
		while(*p && !isspace((int)*p))
			++p;
		if(p <= p1)
			break; /* may happen??? */
		p2 = p;

		/* if called for database, consider simple, single char *URL */
		/* if called for config, consider weight URL */
		if(!isDB) {
			/* Have weight specified? If yes, scan it */
			p2 = memchr(p1, '=', p - p1);
			if(p2 != NULL) {
				local_weight = strtoul(p2 + 1, NULL, 10);
			} else {
				p2 = p;
			}
		}

		pnode = shm_malloc(sizeof(struct rtpp_node));
		if(pnode == NULL) {
			LM_ERR("no shm memory left\n");
			return -1;
		}
		memset(pnode, 0, sizeof(*pnode));

		lock_get(rtpp_no_lock);
		pnode->idx = *rtpp_no;

		if(ticks == RTPENGINE_MAX_RECHECK_TICKS) {
			pnode->rn_recheck_ticks = ticks;
		} else {
			pnode->rn_recheck_ticks = ticks + get_ticks();
		}
		pnode->rn_weight = local_weight;
		pnode->rn_umode = RNU_UNKNOWN;
		pnode->rn_disabled = disabled;
		pnode->rn_displayed = 1;
		pnode->rn_url.s = shm_malloc(p2 - p1 + 1);
		if(pnode->rn_url.s == NULL) {
			lock_release(rtpp_no_lock);
			shm_free(pnode);
			LM_ERR("no shm memory left\n");
			return -1;
		}
		memmove(pnode->rn_url.s, p1, p2 - p1);
		pnode->rn_url.s[p2 - p1] = 0;
		pnode->rn_url.len = p2 - p1;

		/* Leave only address in rn_address */
		pnode->rn_address = pnode->rn_url.s;
		if(strncasecmp(pnode->rn_address, "udp:", 4) == 0) {
			pnode->rn_umode = RNU_UDP;
			pnode->rn_address += 4;
		} else if(strncasecmp(pnode->rn_address, "udp6:", 5) == 0) {
			pnode->rn_umode = RNU_UDP6;
			pnode->rn_address += 5;
		} else if(strncasecmp(pnode->rn_address, "unix:", 5) == 0) {
			pnode->rn_umode = RNU_LOCAL;
			pnode->rn_address += 5;
		} else if(strncasecmp(pnode->rn_address, "ws://", 5) == 0) {
			pnode->rn_umode = RNU_WS;
		} else if(strncasecmp(pnode->rn_address, "wss://", 6) == 0) {
			pnode->rn_umode = RNU_WSS;
		} else {
			lock_release(rtpp_no_lock);
			LM_WARN("Node address must start with 'udp:' or 'udp6:' or 'unix:' "
					"or 'ws://' or 'wss://'. Ignore '%s'.\n",
					pnode->rn_address);
			shm_free(pnode->rn_url.s);
			shm_free(pnode);

			if(!isDB) {
				continue;
			} else {
				return 0;
			}
		}

		if(pnode->rn_umode != RNU_WS && pnode->rn_umode != RNU_WSS) {
			/* Check the rn_address is 'hostname:port' */
			/* Check the rn_address port is valid */
			if(pnode->rn_umode == RNU_UDP6) {
				p1 = strstr(pnode->rn_address, "]:");
				if(p1 != NULL) {
					p1++;
				}
			} else {
				p1 = strchr(pnode->rn_address, ':');
			}
			if(p1 != NULL) {
				p1++;
			}

			if(p1 != NULL && p1[0] != '\0') {
				s1.s = p1;
				s1.len = strlen(p1);
				if(str2int(&s1, &port) < 0 || port > 0xFFFF) {
					lock_release(rtpp_no_lock);
					LM_WARN("Node address must end with a valid port number. "
							"Ignore '%s'.\n",
							pnode->rn_address);
					shm_free(pnode->rn_url.s);
					shm_free(pnode);

					if(!isDB) {
						continue;
					} else {
						return 0;
					}
				}
			}
		} else {
			/* websocket */
			if(_rtpe_lwscb.loaded == 0) {
				lock_release(rtpp_no_lock);
				LM_WARN("Websocket protocol requested, but no websocket API "
						"loaded. Ignore '%s'.\n",
						pnode->rn_address);
				shm_free(pnode->rn_url.s);
				shm_free(pnode);

				if(!isDB) {
					continue;
				} else {
					return 0;
				}
			}
		}

		/* If node found in set, update it */
		rtpp_node = get_rtpp_node(rtpp_list, &pnode->rn_url);

		lock_get(rtpp_list->rset_lock);
		if(rtpp_node) {
			rtpp_node->rn_disabled = pnode->rn_disabled;
			rtpp_node->rn_displayed = pnode->rn_displayed;
			rtpp_node->rn_recheck_ticks = pnode->rn_recheck_ticks;
			rtpp_node->rn_weight = pnode->rn_weight;
			lock_release(rtpp_list->rset_lock);
			lock_release(rtpp_no_lock);

			shm_free(pnode->rn_url.s);
			shm_free(pnode);

			if(!isDB) {
				continue;
			} else {
				return 0;
			}
		}

		if(rtpp_list->rn_first == NULL) {
			rtpp_list->rn_first = pnode;
		} else {
			rtpp_list->rn_last->rn_next = pnode;
		}

		rtpp_list->rn_last = pnode;
		rtpp_list->rtpp_node_count++;
		lock_release(rtpp_list->rset_lock);

		*rtpp_no = *rtpp_no + 1;
		lock_release(rtpp_no_lock);

		if(!isDB) {
			continue;
		} else {
			return 0;
		}
	}
	return 0;
}


/* 0 - success
 * -1 - error
 * */
static int rtpengine_add_rtpengine_set(char *rtp_proxies, unsigned int weight,
		int disabled, unsigned int ticks)
{
	char *p, *p2;
	struct rtpp_set *rtpp_list;
	unsigned int my_current_id;
	str id_set;

	/* empty definition? */
	p = rtp_proxies;
	if(!p || *p == '\0') {
		return 0;
	}

	for(; *p && isspace(*p); p++)
		;
	if(*p == '\0') {
		return 0;
	}

	rtp_proxies = strstr(p, "==");
	if(rtp_proxies) {
		if(*(rtp_proxies + 2) == '\0') {
			LM_ERR("script error -invalid rtpengine list!\n");
			return -1;
		}

		*rtp_proxies = '\0';
		p2 = rtp_proxies - 1;
		for(; isspace(*p2); *p2 = '\0', p2--)
			;
		id_set.s = p;
		id_set.len = p2 - p + 1;

		if(id_set.len <= 0 || str2int(&id_set, &my_current_id) < 0) {
			LM_ERR("script error -invalid set_id value!\n");
			return -1;
		}

		rtp_proxies += 2;
	} else {
		rtp_proxies = p;
		my_current_id = setid_default;
	}

	for(; *rtp_proxies && isspace(*rtp_proxies); rtp_proxies++)
		;

	if(!(*rtp_proxies)) {
		LM_ERR("script error -empty rtp_proxy list\n");
		return -1;
		;
	}

	/*search for the current_id*/
	rtpp_list = get_rtpp_set(my_current_id, 1);

	if(rtpp_list != NULL) {

		if(add_rtpengine_socks(
				   rtpp_list, rtp_proxies, weight, disabled, ticks, 0)
				!= 0)
			goto error;
		else
			return 0;
	}

error:
	return -1;
}

static int rtpengine_set_dtmf_events_sock(modparam_t type, void *val)
{
	char *p;
	p = (char *)val;

	if(p == 0 || *p == '\0') {
		return 0;
	}

	rtpengine_dtmf_event_sock.s = p;
	rtpengine_dtmf_event_sock.len = strlen(rtpengine_dtmf_event_sock.s);

	return 0;
}

/**
 * DTMF events loop
 */
static void rtpengine_dtmf_events_loop(void)
{
	int ret;
	char *p;
	str s_port;
	unsigned int port;
	unsigned int socket_len;
	union sockaddr_union udp_addr;
	char buffer[RTPENGINE_DTMF_EVENT_BUFFER];

	p = q_memchr(
			rtpengine_dtmf_event_sock.s, ':', rtpengine_dtmf_event_sock.len);

	if(!p) {
		LM_ERR("failed to initialize dtmf event listener because no port was "
			   "specified %.*s!\n",
				rtpengine_dtmf_event_sock.len, rtpengine_dtmf_event_sock.s);
		return;
	}

	s_port.s = p + 1;
	s_port.len = rtpengine_dtmf_event_sock.s + rtpengine_dtmf_event_sock.len
				 - s_port.s;

	if(s_port.len <= 0 || str2int(&s_port, &port) < 0 || port > 65535) {
		LM_ERR("failed to initialize dtmf event listener because port is "
			   "invalid %.*s\n",
				rtpengine_dtmf_event_sock.len, rtpengine_dtmf_event_sock.s);
		return;
	}
	rtpengine_dtmf_event_sock.len -= s_port.len + 1;
	trim(&rtpengine_dtmf_event_sock);
	rtpengine_dtmf_event_sock.s[rtpengine_dtmf_event_sock.len] = '\0';

	memset(&udp_addr, 0, sizeof(union sockaddr_union));

	if(rtpengine_dtmf_event_sock.s[0] == '[') {
		udp_addr.sin6.sin6_family = AF_INET6;
		udp_addr.sin6.sin6_port = htons(port);
		socket_len = sizeof(struct sockaddr_in6);
		ret = inet_pton(AF_INET6, rtpengine_dtmf_event_sock.s,
				&udp_addr.sin6.sin6_addr);
	} else {
		udp_addr.sin.sin_family = AF_INET;
		udp_addr.sin.sin_port = htons(port);
		socket_len = sizeof(struct sockaddr_in);
		ret = inet_pton(
				AF_INET, rtpengine_dtmf_event_sock.s, &udp_addr.sin.sin_addr);
	}

	if(ret != 1) {
		LM_ERR("failed to initialize dtmf event listener because address could "
			   "not be created for %s\n",
				rtpengine_dtmf_event_sock.s);
		return;
	}

	rtpengine_dtmf_event_fd = socket(udp_addr.s.sa_family, SOCK_DGRAM, 0);

	if(rtpengine_dtmf_event_fd < 0) {
		LM_ERR("can't create socket\n");
		return;
	}

	if(bind(rtpengine_dtmf_event_fd, &udp_addr.s, socket_len) < 0) {
		LM_ERR("could not bind dtmf events socket %s:%u (%s:%d)\n",
				rtpengine_dtmf_event_sock.s, port, strerror(errno), errno);
		goto end;
	}
	LM_INFO("dtmf event listener started on %s:%u\n",
			rtpengine_dtmf_event_sock.s, port);

	for(;;) {
		do
			ret = read(rtpengine_dtmf_event_fd, buffer,
					RTPENGINE_DTMF_EVENT_BUFFER);
		while(ret == -1 && errno == EINTR);

		if(ret < 0) {
			LM_ERR("problem reading on socket %s:%u (%s:%d)\n",
					rtpengine_dtmf_event_sock.s, port, strerror(errno), errno);
			goto end;
		}

		if(rtpe_event_callback.s == NULL || rtpe_event_callback.len <= 0) {
			if(dtmf_event_rt == -1) {
				LM_NOTICE("nothing to do - nobody is listening!\n");
				goto end;
			}
		}

		p = shm_malloc(ret + 1);
		if(!p) {
			LM_ERR("could not allocate %d for buffer %.*s\n", ret, ret, buffer);
			goto end;
		}
		memcpy(p, buffer, ret);
		p[ret] = '\0';

		if(rtpengine_raise_dtmf_event(p, ret) < 0) {
			LM_ERR("Failed to raise dtmf event\n");
		}
		shm_free(p);
	}

end:
	close(rtpengine_dtmf_event_fd);
}

/**
 * Raise DTMF event
 */
static int rtpengine_raise_dtmf_event(char *buffer, int len)
{
	srjson_doc_t jdoc;
	srjson_t *it = NULL;
	struct sip_msg *fmsg = NULL;
	struct run_act_ctx ctx;
	int rtb;

	sr_kemi_eng_t *keng = NULL;

	LM_DBG("executing event_route[rtpengine:dtmf-event] (%d)\n", dtmf_event_rt);
	LM_DBG("dispatching buffer: %s\n", buffer);

	srjson_InitDoc(&jdoc, NULL);

	jdoc.buf.s = buffer;
	jdoc.buf.len = len;

	jdoc.root = srjson_Parse(&jdoc, jdoc.buf.s);
	if(jdoc.root == NULL) {
		LM_ERR("invalid json doc [[%s]]\n", jdoc.buf.s);
		goto error;
	}

	if(faked_msg_init() < 0) {
		LM_ERR("Failed to initialize fake msg\n");
		goto error;
	}

	/* iterate over keys */
	for(it = jdoc.root->child; it; it = it->next) {
		LM_DBG("found field: %s\n", it->string);
		if(strcmp(it->string, "callid") == 0) {
			pv_value_t pv_val;
			pv_val.rs.s = it->valuestring;
			pv_val.rs.len = strlen(it->valuestring);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_callid_pvar->setf(
					   0, &dtmf_event_callid_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_callid_pvar_str.len,
						dtmf_event_callid_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "source_tag") == 0) {
			pv_value_t pv_val;
			pv_val.rs.s = it->valuestring;
			pv_val.rs.len = strlen(it->valuestring);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_source_tag_pvar->setf(
					   0, &dtmf_event_source_tag_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_source_tag_pvar_str.len,
						dtmf_event_source_tag_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "timestamp") == 0) {
			pv_value_t pv_val;
			int_str val = {0};
			char intbuf[32];
			snprintf(intbuf, sizeof(intbuf), "%lld", SRJSON_GET_LLONG(it));
			memset(&val, 0, sizeof(val));

			pv_val.rs.s = intbuf;
			pv_val.rs.len = strlen(intbuf);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_timestamp_pvar->setf(
					   0, &dtmf_event_timestamp_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_timestamp_pvar_str.len,
						dtmf_event_timestamp_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "event") == 0) {
			pv_value_t pv_val;
			int_str val = {0};
			char intbuf[32];
			snprintf(intbuf, sizeof(intbuf), "%lld", SRJSON_GET_LLONG(it));
			memset(&val, 0, sizeof(val));

			pv_val.rs.s = intbuf;
			pv_val.rs.len = strlen(intbuf);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_pvar->setf(
					   0, &dtmf_event_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n", dtmf_event_pvar_str.len,
						dtmf_event_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "source_label") == 0) {
			pv_value_t pv_val;
			pv_val.rs.s = it->valuestring;
			pv_val.rs.len = strlen(it->valuestring);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_source_label_pvar->setf(0,
					   &dtmf_event_source_label_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_source_label_pvar_str.len,
						dtmf_event_source_label_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "tags") == 0) {
			pv_value_t pv_val;
			srjson_t *tag;
			char *tags_str = NULL;
			int tags_len = 0;
			int first = 1;

			/* Calculate total length needed for all tags */
			for(tag = it->child; tag; tag = tag->next) {
				if(!first) {
					tags_len += 1; /* For comma */
				}
				tags_len += strlen(tag->valuestring);
				first = 0;
			}

			if(tags_len > 0) {
				tags_str = pkg_malloc(tags_len + 1);
				if(!tags_str) {
					LM_ERR("no more pkg memory\n");
					goto error;
				}
				tags_str[0] = '\0';
				first = 1;

				/* Concatenate all tags with commas */
				for(tag = it->child; tag; tag = tag->next) {
					if(!first) {
						strcat(tags_str, ",");
					}
					strcat(tags_str, tag->valuestring);
					first = 0;
				}

				pv_val.rs.s = tags_str;
				pv_val.rs.len = tags_len;
				pv_val.flags = PV_VAL_STR;

				if(dtmf_event_tags_pvar->setf(
						   0, &dtmf_event_tags_pvar->pvp, (int)EQ_T, &pv_val)
						< 0) {
					LM_ERR("error setting pvar <%.*s>\n",
							dtmf_event_tags_pvar_str.len,
							dtmf_event_tags_pvar_str.s);
					pkg_free(tags_str);
					goto error;
				}
				pkg_free(tags_str);
			}
		} else if(strcmp(it->string, "type") == 0) {
			pv_value_t pv_val;
			int_str val = {0};
			char intbuf[32];
			snprintf(intbuf, sizeof(intbuf), "%lld", SRJSON_GET_LLONG(it));
			memset(&val, 0, sizeof(val));

			pv_val.rs.s = intbuf;
			pv_val.rs.len = strlen(intbuf);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_type_pvar->setf(
					   0, &dtmf_event_type_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_type_pvar_str.len,
						dtmf_event_type_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "source_ip") == 0) {
			pv_value_t pv_val;
			pv_val.rs.s = it->valuestring;
			pv_val.rs.len = strlen(it->valuestring);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_source_ip_pvar->setf(
					   0, &dtmf_event_source_ip_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_source_ip_pvar_str.len,
						dtmf_event_source_ip_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "duration") == 0) {
			pv_value_t pv_val;
			int_str val = {0};
			char intbuf[32];
			snprintf(intbuf, sizeof(intbuf), "%lld", SRJSON_GET_LLONG(it));
			memset(&val, 0, sizeof(val));

			pv_val.rs.s = intbuf;
			pv_val.rs.len = strlen(intbuf);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_duration_pvar->setf(
					   0, &dtmf_event_duration_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_duration_pvar_str.len,
						dtmf_event_duration_pvar_str.s);
				goto error;
			}
		} else if(strcmp(it->string, "volume") == 0) {
			pv_value_t pv_val;
			int_str val = {0};
			char intbuf[32];
			snprintf(intbuf, sizeof(intbuf), "%lld", SRJSON_GET_LLONG(it));
			memset(&val, 0, sizeof(val));

			pv_val.rs.s = intbuf;
			pv_val.rs.len = strlen(intbuf);
			pv_val.flags = PV_VAL_STR;

			if(dtmf_event_volume_pvar->setf(
					   0, &dtmf_event_volume_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n",
						dtmf_event_volume_pvar_str.len,
						dtmf_event_volume_pvar_str.s);
				goto error;
			}
		}
	}

	fmsg = faked_msg_next();
	rtb = get_route_type();
	str evname = str_init("rtpengine:dtmf-event");
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	if(rtpe_event_callback.s == NULL || rtpe_event_callback.len <= 0) {
		run_top_route(event_rt.rlist[dtmf_event_rt], fmsg, &ctx);
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL) {
			if(sr_kemi_route(
					   keng, fmsg, REQUEST_ROUTE, &rtpe_event_callback, &evname)
					< 0) {
				LM_ERR("error running event route kemi callback\n");
			}
		} else {
			LM_ERR("no event route or kemi callback found for execution\n");
		}
	}
	set_route_type(rtb);
	if(ctx.run_flags & DROP_R_F) {
		LM_ERR("exit due to 'drop' in event route\n");
		goto error;
	}

	srjson_DestroyDoc(&jdoc);

	return 0;

error:
	srjson_DestroyDoc(&jdoc);
	return -1;
}

static int fixup_set_id(void **param, int param_no)
{
	int int_val;
	unsigned int set_id;
	struct rtpp_set *rtpp_list;
	rtpp_set_link_t *rtpl = NULL;
	str s;

	rtpl = (rtpp_set_link_t *)pkg_malloc(sizeof(rtpp_set_link_t));
	if(rtpl == NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	memset(rtpl, 0, sizeof(rtpp_set_link_t));
	s.s = (char *)*param;
	s.len = strlen(s.s);

	if(s.s[0] == PV_MARKER) {
		int_val = pv_locate_name(&s);
		if(int_val < 0 || int_val != s.len) {
			LM_ERR("invalid parameter %s\n", s.s);
			pkg_free(rtpl);
			return -1;
		}
		rtpl->rpv = pv_cache_get(&s);
		if(rtpl->rpv == NULL) {
			LM_ERR("invalid pv parameter %s\n", s.s);
			pkg_free(rtpl);
			return -1;
		}
	} else {
		int_val = str2int(&s, &set_id);
		if(int_val == 0) {
			pkg_free(*param);
			if((rtpp_list = select_rtpp_set(set_id)) == 0) {
				LM_ERR("rtpp_proxy set %u not configured\n", set_id);
				pkg_free(rtpl);
				return E_CFG;
			}
			rtpl->rset = rtpp_list;
		} else {
			LM_ERR("bad number <%s>\n", (char *)(*param));
			pkg_free(rtpl);
			return E_CFG;
		}
	}
	*param = (void *)rtpl;
	return 0;
}

static int fixup_free_set_id(void **param, int param_no)
{
	pkg_free(*param);
	return 0;
}

/**
 * @brief Sends a ping command to an RTP engine node and checks the response.
 *
 * @details Initialize a bencode buffer, construct a ping command dictionary,
 * send the command to the specified RTP engine node, and then decodes the response.
 * It checks if the response is a dictionary and if it contains the expected "pong" result.
 *
 * @param node The RTP engine node to send the ping command to.
 * @return Returns 0 on success (pong received), -1 on failure.
 */
static int rtpp_test_ping(struct rtpp_node *node)
{
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;
	char *cp;
	int ret;

	LM_DBG("Sending ping to node %.*s\n", node->rn_url.len, node->rn_url.s);
	if(bencode_buffer_init(&bencbuf)) {
		return -1;
	}
	dict = bencode_dictionary(&bencbuf);
	bencode_dictionary_add_string(dict, "command", command_strings[OP_PING]);

	if(bencbuf.error) {
		goto error;
	}

	cp = send_rtpp_command(node, dict, &ret);
	if(!cp) {
		goto error;
	}

	dict = bencode_decode_expect(&bencbuf, cp, ret, BENCODE_DICTIONARY);
	if(!dict || bencode_dictionary_get_strcmp(dict, "result", "pong")) {
		goto error;
	}

	bencode_buffer_free(&bencbuf);
	return 0;

error:
	bencode_buffer_free(&bencbuf);
	return -1;
}


static void rtpengine_rpc_reload(rpc_t *rpc, void *ctx)
{
	time_t tnow;
	int rping = 1;
	int n = 0;

	if(rtpp_db_url.s == NULL) {
		// no database
		rpc->fault(ctx, 500, "No Database URL");
		return;
	}

	if(!sr_instance_ready()) {
		rpc->fault(ctx, 500, "Initializing - try later");
		return;
	}

	n = rpc->scan(ctx, "*d", &rping);
	if(n != 1) {
		rping = 1;
	} else if(rping != 0) {
		rping = 1;
	}

	tnow = time(NULL);
	if(tnow - _rtpe_list_version->vertime < RTPE_LIST_VERSION_DELAY) {
		rpc->fault(ctx, 500, "Too short reload interval - try later");
		return;
	}
	_rtpe_list_version->vertime = tnow;

	if(init_rtpengine_db() < 0) {
		// fail reloading from database
		rpc->fault(ctx, 500, "Failed reloading db");
		return;
	}

	if(build_rtpp_socks(1, rping)) {
		rpc->fault(ctx, 500, "Failed to build rtpengine sockets");
		return;
	}

	_rtpe_list_version->vernum += 1;
	_rtpe_list_version->vertime = time(NULL);
	LM_DBG("current rtpengines list version: %d (%" PRIu64 ")\n",
			_rtpe_list_version->vernum, (uint64_t)_rtpe_list_version->vertime);
	rpc->rpl_printf(ctx, "Ok. Reload successful.");
}

/**
 * @brief Iterates over the RTP engine nodes and filter them based on the provided URL.
 *
 * @details Iterate over the RTP engine nodes in the system, filtering them based on the provided URL.
 * If the @p rtpp_url is "all", it iterates over all RTP engine nodes. Otherwise, it iterates
 * over the nodes that match the provided URL. For each matching node, it calls the provided
 * callback function @p cb.
 *
 * @param rpc The RPC instance.
 * @param ctx The context.
 * @param rtpp_url The RTP proxy URL to filter by.
 * @param cb The callback function to call for each matching node.
 * @param data The data to pass to the callback function.
 * @return 0 on success, -1 on error.
 */
static int rtpengine_rpc_iterate(rpc_t *rpc, void *ctx, const str *rtpp_url,
		int (*cb)(struct rtpp_node *, struct rtpp_set *, void *), void *data)
{
	struct rtpp_set *rtpp_list;
	struct rtpp_node *crt_rtpp;
	int found = RPC_FOUND_NONE, err = 0;
	int ret;

	if(!sr_instance_ready()) {
		rpc->fault(ctx, 500, "Initializing - try later");
		return -1;
	}

	if(build_rtpp_socks(1, 1)) {
		rpc->fault(ctx, 500, "Out of memory");
		return -1;
	}

	if(!rtpp_set_list) {
		rpc->fault(ctx, 404, "Instance not found (no sets loaded)");
		return -1;
	}

	/* found a matching all - show all rtpp */
	if(strncmp("all", rtpp_url->s, 3) == 0) {
		found = RPC_FOUND_ALL;
	}

	lock_get(rtpp_set_list->rset_head_lock);
	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != NULL;
			rtpp_list = rtpp_list->rset_next) {

		lock_get(rtpp_list->rset_lock);
		for(crt_rtpp = rtpp_list->rn_first; crt_rtpp != NULL;
				crt_rtpp = crt_rtpp->rn_next) {

			if(!crt_rtpp->rn_displayed) {
				continue;
			}

			/* found a matching rtpp - ping it */
			if(found == RPC_FOUND_ALL
					|| (crt_rtpp->rn_url.len == rtpp_url->len
							&& strncmp(crt_rtpp->rn_url.s, rtpp_url->s,
									   rtpp_url->len)
									   == 0)) {

				ret = cb(crt_rtpp, rtpp_list, data);
				if(ret) {
					err = 1;
					break;
				}

				if(found == RPC_FOUND_NONE) {
					found = RPC_FOUND_ONE;
				}
			}
		}
		lock_release(rtpp_list->rset_lock);

		if(err)
			break;
	}
	lock_release(rtpp_set_list->rset_head_lock);

	if(err)
		return -1;

	if(found == RPC_FOUND_NONE) {
		rpc->fault(ctx, 404, "Instance not found");
		return -1;
	}

	return found;
}

/**
 * @brief Adds information about an RTP engine node to the RPC response.
 *
 * @details Constructs and adds a structured data block to the RPC response,
 * containing details about the specified RTP engine node, such as its URL, set ID,
 * index, weight, disabled status, and recheck ticks.
 *
 * @param ptrsp A pointer to an array of pointers containing the RPC context and other necessary data.
 * @param crt_rtpp The RTP engine node for which to add information.
 * @param rtpp_list The RTP engine set to which the node belongs.
 *
 * @return Returns 0 on success, -1 on failure.
 */
static int add_rtpp_node_info(
		void *ptrsp, struct rtpp_node *crt_rtpp, struct rtpp_set *rtpp_list)
{
	void *vh;
	void **ptrs = ptrsp;
	rpc_t *rpc;
	void *ctx;
	int rtpp_ticks;

	rpc = ptrs[0];
	ctx = ptrs[1];

	if(rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return -1;
	}

	rpc->struct_add(vh, "Sddd", "url", &crt_rtpp->rn_url, "set",
			rtpp_list->id_set, "index", crt_rtpp->idx, "weight",
			crt_rtpp->rn_weight);

	if((1 == crt_rtpp->rn_disabled)
			&& (crt_rtpp->rn_recheck_ticks == RTPENGINE_MAX_RECHECK_TICKS)) {
		rpc->struct_add(vh, "b", "disabled", 1);
	} else {
		rpc->struct_add(vh, "b", "disabled", 0);
	}
	rpc->struct_add(vh, "b", "active", crt_rtpp->rn_disabled == 0);

	if(crt_rtpp->rn_recheck_ticks == RTPENGINE_MAX_RECHECK_TICKS) {
		rpc->struct_add(vh, "d", "recheck_ticks", -1);
	} else {
		rtpp_ticks = crt_rtpp->rn_recheck_ticks - get_ticks();
		rtpp_ticks = rtpp_ticks < 0 ? 0 : rtpp_ticks;
		rpc->struct_add(vh, "d", "recheck_ticks", rtpp_ticks);
	}

	return 0;
}

/**
 * @brief Callback to enable or disable an RTP engine node.
 *
 * @details Called for each RTP engine node during an iteration process.
 * It enables or disables the node based on the @p flagp provided and updates its status accordingly.
 *
 * @param crt_rtpp The RTP engine node to be enabled or disabled.
 * @param rtpp_list The RTP engine set to which the node belongs. (not used in this function)
 * @param flagp A pointer to a flag indicating whether to enable (1) or disable (0) the node.
 *
 * @return Returns 0 on success, -1 on failure.
 */
static int rtpengine_iter_cb_enable(
		struct rtpp_node *crt_rtpp, struct rtpp_set *rtpp_list, void *flagp)
{
	int *flag = flagp;

	/* do ping when try to enable the rtpp */
	if(*flag) {

		/* if ping success, enable the rtpp and reset ticks */
		if(rtpp_test_ping(crt_rtpp) == 0) {
			crt_rtpp->rn_disabled = 0;
			crt_rtpp->rn_recheck_ticks = RTPENGINE_MIN_RECHECK_TICKS;

			/* if ping fail, disable the rtpps but _not_ permanently*/
		} else {
			crt_rtpp->rn_recheck_ticks =
					get_ticks()
					+ cfg_get(rtpengine, rtpengine_cfg, rtpengine_disable_tout);
			crt_rtpp->rn_disabled = 1;
			*flag = 2; /* return value to caller */
		}

		/* do not ping when disable the rtpp; disable it permanenty */
	} else {
		crt_rtpp->rn_disabled = 1;
		crt_rtpp->rn_recheck_ticks = RTPENGINE_MAX_RECHECK_TICKS;
	}

	return 0;
}

static void rtpengine_rpc_enable(rpc_t *rpc, void *ctx)
{
	void *vh;
	str rtpp_url;
	int flag, found;

	if(rpc->scan(ctx, "Sd", &rtpp_url, &flag) < 2) {
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}

	flag = flag ? 1 : 0; /* also used as a return value */

	found = rtpengine_rpc_iterate(
			rpc, ctx, &rtpp_url, rtpengine_iter_cb_enable, &flag);
	if(found == -1)
		return;

	if(rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}

	rpc->struct_add(vh, "S", "url", &rtpp_url);

	if(flag == 0)
		rpc->struct_add(vh, "s", "status", "disable");
	else if(flag == 1)
		rpc->struct_add(vh, "s", "status", "enable");
	else
		rpc->struct_add(vh, "s", "status", "fail");
}

static int rtpengine_iter_cb_show(
		struct rtpp_node *crt_rtpp, struct rtpp_set *rtpp_list, void *ptrsp)
{
	if(add_rtpp_node_info(ptrsp, crt_rtpp, rtpp_list) < 0)
		return -1;
	return 0;
}

static void rtpengine_rpc_show(rpc_t *rpc, void *ctx)
{
	str rtpp_url;
	void *ptrs[2] = {rpc, ctx};

	if(rpc->scan(ctx, "S", &rtpp_url) < 1) {
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}

	rtpengine_rpc_iterate(rpc, ctx, &rtpp_url, rtpengine_iter_cb_show, ptrs);
}

static int rtpengine_iter_cb_ping(
		struct rtpp_node *crt_rtpp, struct rtpp_set *rtpp_list, void *data)
{
	int *found_rtpp_disabled = data;

	/* if ping fail */
	if(rtpp_test_ping(crt_rtpp) < 0) {
		crt_rtpp->rn_recheck_ticks =
				get_ticks()
				+ cfg_get(rtpengine, rtpengine_cfg, rtpengine_disable_tout);
		*found_rtpp_disabled = 1;
		crt_rtpp->rn_disabled = 1;
	}
	/* if ping success, enable the rtpp and reset ticks, ONLY IF was not disabled manually */
	else if(crt_rtpp->rn_recheck_ticks != RTPENGINE_MAX_RECHECK_TICKS) {
		crt_rtpp->rn_recheck_ticks = RTPENGINE_MIN_RECHECK_TICKS;
		crt_rtpp->rn_disabled = 0;
		*found_rtpp_disabled = 0;
	}
	return 0;
}

static void rtpengine_rpc_ping(rpc_t *rpc, void *ctx)
{
	void *vh;
	int found;
	int found_rtpp_disabled = 0;
	str rtpp_url;

	if(rpc->scan(ctx, "S", &rtpp_url) < 1) {
		rpc->fault(ctx, 500, "Not enough parameters");
		return;
	}

	found = rtpengine_rpc_iterate(
			rpc, ctx, &rtpp_url, rtpengine_iter_cb_ping, &found_rtpp_disabled);
	if(found == -1)
		return;

	if(rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}

	rpc->struct_add(vh, "Ss", "url", &rtpp_url, "status",
			(found_rtpp_disabled ? "fail" : "success"));
}

static void rtpengine_rpc_get_hash_total(rpc_t *rpc, void *ctx)
{
	rpc->add(ctx, "u", rtpengine_hash_table_total());
}


static const char *rtpengine_rpc_reload_doc[2] = {
		"Reload rtpengine proxies from database", 0};
static const char *rtpengine_rpc_ping_doc[2] = {
		"Ping an rtpengine instance", 0};
static const char *rtpengine_rpc_show_doc[2] = {
		"Get details about an rtpengine instance", 0};
static const char *rtpengine_rpc_enable_doc[2] = {
		"Enable or disable an rtpengine instance", 0};
static const char *rtpengine_rpc_get_hash_total_doc[2] = {
		"Get total number of entries in hash table", 0};

rpc_export_t rtpengine_rpc[] = {
		{"rtpengine.reload", rtpengine_rpc_reload, rtpengine_rpc_reload_doc, 0},
		{"rtpengine.ping", rtpengine_rpc_ping, rtpengine_rpc_ping_doc, 0},
		{"rtpengine.show", rtpengine_rpc_show, rtpengine_rpc_show_doc,
				RET_ARRAY},
		{"rtpengine.enable", rtpengine_rpc_enable, rtpengine_rpc_enable_doc, 0},
		{"rtpengine.get_hash_total", rtpengine_rpc_get_hash_total,
				rtpengine_rpc_get_hash_total_doc, 0},
		{0, 0, 0, 0}};

static int rtpengine_rpc_init(void)
{
	if(rpc_register_array(rtpengine_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

static int mod_init(void)
{
	int i;
	pv_spec_t *avp_spec;
	avp_flags_t avp_flags;
	str s;

	_rtpe_list_version =
			(rtpe_list_version_t *)shm_mallocxz(sizeof(rtpe_list_version_t));
	if(_rtpe_list_version == NULL) {
		LM_ERR("no more shm memory for rtpe list version\n");
		return -1;
	}
	_rtpe_list_version->vernum = 1;
	_rtpe_list_version->vertime = time(NULL);

	if(rtpengine_rpc_init() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	rtpp_no = (unsigned int *)shm_malloc(sizeof(unsigned int));
	if(!rtpp_no) {
		LM_ERR("no more shm memory for rtpp_no\n");
		return -1;
	}
	*rtpp_no = 0;

	rtpp_no_lock = lock_alloc();
	if(!rtpp_no_lock) {
		LM_ERR("no more shm memory for rtpp_no_lock\n");
		return -1;
	}

	if(lock_init(rtpp_no_lock) == 0) {
		LM_ERR("could not init rtpp_no_lock\n");
		return -1;
	}

	if(_rtpe_wsapi.s != NULL && _rtpe_wsapi.len == 4
			&& strncasecmp(_rtpe_wsapi.s, "lwsc", 4) == 0) {
		if(lwsc_load_api(&_rtpe_lwscb)) {
			LM_ERR("failed to load WS API: %s\n", _rtpe_wsapi.s);
			return -1;
		}
	} else {
		if(_rtpe_wsapi.s != NULL && _rtpe_wsapi.len > 0) {
			LM_ERR("unsupported WS API: %s\n", _rtpe_wsapi.s);
			return -1;
		}
	}

	/* initialize the list of set; mod_destroy does shm_free() if fail */
	if(!rtpp_set_list) {
		rtpp_set_list = shm_malloc(sizeof(struct rtpp_set_head));
		if(!rtpp_set_list) {
			LM_ERR("no shm memory left to create list of proxysets\n");
			return -1;
		}
		memset(rtpp_set_list, 0, sizeof(struct rtpp_set_head));

		rtpp_set_list->rset_head_lock = lock_alloc();
		if(!rtpp_set_list->rset_head_lock) {
			LM_ERR("no shm memory left to create list of proxysets lock\n");
			return -1;
		}

		if(lock_init(rtpp_set_list->rset_head_lock) == 0) {
			LM_ERR("could not init lock sets for rtpengine list\n");
			return -1;
		}
	}

	if(rtpp_db_url.s == NULL) {
		/* storing the list of rtpengine sets in shared memory*/
		for(i = 0; i < rtpp_sets; i++) {
			if(rtpengine_add_rtpengine_set(rtpp_strings[i], 1, 0, 0) != 0) {
				for(; i < rtpp_sets; i++)
					if(rtpp_strings[i])
						pkg_free(rtpp_strings[i]);
				pkg_free(rtpp_strings);
				return -1;
			}
			if(rtpp_strings[i])
				pkg_free(rtpp_strings[i]);
		}
	} else {
		LM_INFO("Loading rtpengine definitions from DB\n");
		if(init_rtpengine_db() < 0) {
			LM_ERR("error while loading rtpengine instances from database\n");
			return -1;
		}
	}

	if(rtp_inst_pv_param.s) {
		rtp_inst_pv_param.len = strlen(rtp_inst_pv_param.s);
		rtp_inst_pvar = pv_cache_get(&rtp_inst_pv_param);
		if((rtp_inst_pvar == NULL)
				|| ((rtp_inst_pvar->type != PVT_AVP)
						&& (rtp_inst_pvar->type != PVT_XAVP)
						&& (rtp_inst_pvar->type != PVT_SCRIPTVAR))) {
			LM_ERR("Invalid pvar name <%.*s>\n", rtp_inst_pv_param.len,
					rtp_inst_pv_param.s);
			return -1;
		}
	}

	if(pv_parse_var(&extra_id_pv_param, &extra_id_pv, NULL))
		return -1;

	if(mos_label_stats_parse(&global_mos_stats))
		return -1;
	if(mos_label_stats_parse(&side_A_mos_stats))
		return -1;
	if(mos_label_stats_parse(&side_B_mos_stats))
		return -1;

	if(setid_avp_param) {
		s.s = setid_avp_param;
		s.len = strlen(s.s);
		avp_spec = pv_cache_get(&s);
		if(avp_spec == NULL || (avp_spec->type != PVT_AVP)) {
			LM_ERR("malformed or non AVP definition <%s>\n", setid_avp_param);
			return -1;
		}
		if(pv_get_avp_name(0, &(avp_spec->pvp), &setid_avp, &avp_flags) != 0) {
			LM_ERR("invalid AVP definition <%s>\n", setid_avp_param);
			return -1;
		}
		setid_avp_type = avp_flags;
	}

	if(write_sdp_pvar_str.len > 0) {
		write_sdp_pvar = pv_cache_get(&write_sdp_pvar_str);
		if(write_sdp_pvar == NULL
				|| (write_sdp_pvar->type != PVT_AVP
						&& write_sdp_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("write_sdp_pv: not a valid AVP or VAR definition <%.*s>\n",
					write_sdp_pvar_str.len, write_sdp_pvar_str.s);
			return -1;
		}
	}

	if(read_sdp_pvar_str.len > 0) {
		read_sdp_pvar = pv_cache_get(&read_sdp_pvar_str);
		if(read_sdp_pvar == NULL
				|| (read_sdp_pvar->type != PVT_AVP
						&& read_sdp_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("read_sdp_pv: not a valid AVP or VAR definition <%.*s>\n",
					read_sdp_pvar_str.len, read_sdp_pvar_str.s);
			return -1;
		}
	}

	if(media_duration_pvar_str.len > 0) {
		media_duration_pvar = pv_cache_get(&media_duration_pvar_str);
		if(media_duration_pvar == NULL
				|| (media_duration_pvar->type != PVT_AVP
						&& media_duration_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("media_duration_pv: not a valid AVP or VAR definition "
				   "<%.*s>\n",
					media_duration_pvar_str.len, media_duration_pvar_str.s);
			return -1;
		}
	}

	if(rtpp_strings)
		pkg_free(rtpp_strings);

	if(load_tm_api(&tmb) < 0) {
		LM_DBG("could not load the TM-functions - answer-offer model"
			   " auto-detection is disabled\n");
		memset(&tmb, 0, sizeof(struct tm_binds));
	}

	/* Determine IP addr type (IPv4 or IPv6 allowed) */
	force_send_ip_af = get_ip_type(force_send_ip_str);
	if(force_send_ip_af != AF_INET && force_send_ip_af != AF_INET6
			&& strlen(force_send_ip_str) > 0) {
		LM_ERR("%s is an unknown address\n", force_send_ip_str);
		return -1;
	}

	/* init the hastable which keeps the call-id <-> selected_node relation */
	if(!rtpengine_hash_table_init(hash_table_size)) {
		LM_ERR("rtpengine_hash_table_init(%d) failed!\n", hash_table_size);
		return -1;
	} else {
		LM_DBG("rtpengine_hash_table_init(%d) success!\n", hash_table_size);
	}

	/* select the default set */
	default_rtpp_set = select_rtpp_set(setid_default);
	if(!default_rtpp_set) {
		LM_NOTICE("Default rtpp set %u NOT found\n", setid_default);
	} else {
		LM_DBG("Default rtpp set %u found\n", setid_default);
	}

	if(cfg_declare("rtpengine", rtpengine_cfg_def, &default_rtpengine_cfg,
			   cfg_sizeof(rtpengine), &rtpengine_cfg)) {
		LM_ERR("Failed to declare the configuration\n");
		return -1;
	}

	if(hash_algo == RTP_HASH_SHA1_CALLID) {
		if(load_crypto_api(&rtpengine_cb) != 0) {
			LM_ERR("Crypto module required in order to have SHA1 hashing!\n");
			return -1;
		}
	}

	/* Enable ping timer if interval is positive */
	if(rtpengine_ping_interval > 0) {
		register_timer(rtpengine_ping_check_timer, 0, rtpengine_ping_interval);
	}

	dtmf_event_rt = route_lookup(&event_rt, "rtpengine:dtmf-event");

	if(rtpe_event_callback.s == NULL || rtpe_event_callback.len <= 0) {
		if(dtmf_event_rt >= 0 && event_rt.rlist[dtmf_event_rt] == 0) {
			dtmf_event_rt = -1; /* disable */
			return 0;
		}
	}

	if(dtmf_event_callid_pvar_str.len > 0) {
		dtmf_event_callid_pvar = pv_cache_get(&dtmf_event_callid_pvar_str);
		if(dtmf_event_callid_pvar == NULL
				|| (dtmf_event_callid_pvar->type != PVT_AVP
						&& dtmf_event_callid_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_callid_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_callid_pvar_str.len,
					dtmf_event_callid_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_source_tag_pvar_str.len > 0) {
		dtmf_event_source_tag_pvar =
				pv_cache_get(&dtmf_event_source_tag_pvar_str);
		if(dtmf_event_source_tag_pvar == NULL
				|| (dtmf_event_source_tag_pvar->type != PVT_AVP
						&& dtmf_event_source_tag_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_source_tag_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_source_tag_pvar_str.len,
					dtmf_event_source_tag_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_timestamp_pvar_str.len > 0) {
		dtmf_event_timestamp_pvar =
				pv_cache_get(&dtmf_event_timestamp_pvar_str);
		if(dtmf_event_timestamp_pvar == NULL
				|| (dtmf_event_timestamp_pvar->type != PVT_AVP
						&& dtmf_event_timestamp_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_timestamp_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_timestamp_pvar_str.len,
					dtmf_event_timestamp_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_pvar_str.len > 0) {
		dtmf_event_pvar = pv_cache_get(&dtmf_event_pvar_str);
		if(dtmf_event_pvar == NULL
				|| (dtmf_event_pvar->type != PVT_AVP
						&& dtmf_event_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("event_pv: not a valid AVP or VAR definition <%.*s>\n",
					dtmf_event_pvar_str.len, dtmf_event_pvar_str.s);
			return -1;
		}
	}
	if(dtmf_event_source_label_pvar_str.len > 0) {
		dtmf_event_source_label_pvar =
				pv_cache_get(&dtmf_event_source_label_pvar_str);
		if(dtmf_event_source_label_pvar == NULL
				|| (dtmf_event_source_label_pvar->type != PVT_AVP
						&& dtmf_event_source_label_pvar->type
								   != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_source_label_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_source_label_pvar_str.len,
					dtmf_event_source_label_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_tags_pvar_str.len > 0) {
		dtmf_event_tags_pvar = pv_cache_get(&dtmf_event_tags_pvar_str);
		if(dtmf_event_tags_pvar == NULL
				|| (dtmf_event_tags_pvar->type != PVT_AVP
						&& dtmf_event_tags_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_tags_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_tags_pvar_str.len, dtmf_event_tags_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_type_pvar_str.len > 0) {
		dtmf_event_type_pvar = pv_cache_get(&dtmf_event_type_pvar_str);
		if(dtmf_event_type_pvar == NULL
				|| (dtmf_event_type_pvar->type != PVT_AVP
						&& dtmf_event_type_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_type_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_type_pvar_str.len, dtmf_event_type_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_source_ip_pvar_str.len > 0) {
		dtmf_event_source_ip_pvar =
				pv_cache_get(&dtmf_event_source_ip_pvar_str);
		if(dtmf_event_source_ip_pvar == NULL
				|| (dtmf_event_source_ip_pvar->type != PVT_AVP
						&& dtmf_event_source_ip_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_source_ip_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_source_ip_pvar_str.len,
					dtmf_event_source_ip_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_duration_pvar_str.len > 0) {
		dtmf_event_duration_pvar = pv_cache_get(&dtmf_event_duration_pvar_str);
		if(dtmf_event_duration_pvar == NULL
				|| (dtmf_event_duration_pvar->type != PVT_AVP
						&& dtmf_event_duration_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_duration_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_duration_pvar_str.len,
					dtmf_event_duration_pvar_str.s);
			return -1;
		}
	}

	if(dtmf_event_volume_pvar_str.len > 0) {
		dtmf_event_volume_pvar = pv_cache_get(&dtmf_event_volume_pvar_str);
		if(dtmf_event_volume_pvar == NULL
				|| (dtmf_event_volume_pvar->type != PVT_AVP
						&& dtmf_event_volume_pvar->type != PVT_SCRIPTVAR)) {
			LM_ERR("dtmf_event_volume_pv: not a valid AVP or VAR "
				   "definition <%.*s>\n",
					dtmf_event_volume_pvar_str.len,
					dtmf_event_volume_pvar_str.s);
			return -1;
		}
	}

	if(rtpengine_dtmf_event_sock.len > 0) {
		register_procs(1);
		cfg_register_child(1);
	}

	if(rtpengine_enable_dmq > 0 && rtpengine_dmq_init() != 0) {
		LM_ERR("rtpengine_dmq_init() failed!\n");
		return -1;
	}

	return 0;
}

#define rtpe_reload_lock_get(plock)             \
	do {                                        \
		if(rtpp_db_url.s != NULL && lmode != 0) \
			lock_get(plock);                    \
	} while(0)

#define rtpe_reload_lock_release(plock)         \
	do {                                        \
		if(rtpp_db_url.s != NULL && lmode != 0) \
			lock_release(plock);                \
	} while(0)

/**
 * @brief Builds RTP engine sockets based on the current RTP engine list.
 *
 * @details Iterate through the list of RTP engines, closes any existing sockets,
 * and attempts to create new sockets for each RTP engine instance. It handles both
 * IPv4 and IPv6 connections, sets up socket options for MTU discovery and TOS,
 * and binds the socket to a specific address if necessary. If a socket cannot be
 * created or connected, it retries later.
 *
 * @param lmode Locking mode (1 - lock if needed; 0 - no locking)
 * @param rtest RTP engine testing (1 - test if active; 0 - no test done)
 * @return 0 on success, -1 on failure
 */
static int build_rtpp_socks(int lmode, int rtest)
{
	int n, i;
	char *cp;
	struct addrinfo hints, *res;
	struct rtpp_set *rtpp_list;
	struct rtpp_node *pnode;
	unsigned int current_rtpp_no;
#ifdef IP_MTU_DISCOVER
	int ip_mtu_discover = IP_PMTUDISC_DONT;
#endif

	if(_rtpe_list_vernum_local == _rtpe_list_version->vernum) {
		/* same version for the list of rtpengines */
		LM_DBG("same rtpengines list version: %d (%" PRIu64 ")\n",
				_rtpe_list_version->vernum,
				(uint64_t)_rtpe_list_version->vertime);
		return 0;
	}

	rtpe_reload_lock_get(rtpp_no_lock);
	current_rtpp_no = *rtpp_no;
	rtpe_reload_lock_release(rtpp_no_lock);

	// close current sockets
	for(i = 0; i < rtpp_socks_size; i++) {
		if(rtpp_socks[i] >= 0) {
			close(rtpp_socks[i]);
			rtpp_socks[i] = -1;
		}
	}

	rtpp_socks_size = current_rtpp_no;
	/* allocate one more to have a safety end place holder */
	rtpp_socks = (int *)pkg_reallocxf(
			rtpp_socks, sizeof(int) * (rtpp_socks_size + 1));
	if(!rtpp_socks) {
		LM_ERR("no more pkg memory for rtpp_socks\n");
		return -1;
	}
	memset(rtpp_socks, -1, sizeof(int) * (rtpp_socks_size + 1));

	rtpe_reload_lock_get(rtpp_set_list->rset_head_lock);
	_rtpe_list_vernum_local = _rtpe_list_version->vernum;
	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != 0;
			rtpp_list = rtpp_list->rset_next) {

		rtpe_reload_lock_get(rtpp_list->rset_lock);
		for(pnode = rtpp_list->rn_first; pnode != 0; pnode = pnode->rn_next) {
			char *hostname;
			char *hp;

			if(pnode->rn_umode == RNU_LOCAL || pnode->rn_umode == RNU_WS
					|| pnode->rn_umode == RNU_WSS) {
				rtpp_socks[pnode->idx] = -1;
				goto rptest;
			}

			/*
			 * This is UDP or UDP6. Detect host and port; lookup host;
			 * do connect() in order to specify peer address
			 */
			hostname = (char *)pkg_malloc(
					sizeof(char) * (strlen(pnode->rn_address) + 1));
			if(hostname == NULL) {
				LM_ERR("no more pkg memory\n");
				rtpp_socks[pnode->idx] = -1;

				/* retry later */
				_rtpe_list_version->vernum += 1;
				_rtpe_list_version->vertime = time(NULL);

				continue;
			}
			strcpy(hostname, pnode->rn_address);

			cp = strrchr(hostname, ':');
			if(cp != NULL) {
				*cp = '\0';
				cp++;
			}
			if(cp == NULL || *cp == '\0')
				cp = CPORT;

			if(pnode->rn_umode == RNU_UDP6) {
				hp = strrchr(hostname, ']');
				if(hp != NULL)
					*hp = '\0';

				hp = hostname;
				if(*hp == '[')
					hp++;
			} else {
				hp = hostname;
			}

			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = 0;
			hints.ai_family =
					(pnode->rn_umode == RNU_UDP6) ? AF_INET6 : AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			if((n = getaddrinfo(hp, cp, &hints, &res)) != 0) {
				LM_ERR("%s\n", gai_strerror(n));
				pkg_free(hostname);
				rtpp_socks[pnode->idx] = -1;

				/* retry later */
				_rtpe_list_version->vernum += 1;
				_rtpe_list_version->vertime = time(NULL);

				continue;
			}
			pkg_free(hostname);

			rtpp_socks[pnode->idx] =
					socket((pnode->rn_umode == RNU_UDP6) ? AF_INET6 : AF_INET,
							SOCK_DGRAM, 0);
			if(rtpp_socks[pnode->idx] == -1) {
				LM_ERR("can't create socket\n");
				freeaddrinfo(res);

				/* retry later */
				_rtpe_list_version->vernum += 1;
				_rtpe_list_version->vertime = time(NULL);

				continue;
			}

#ifdef IP_MTU_DISCOVER
			if(setsockopt(rtpp_socks[pnode->idx], IPPROTO_IP, IP_MTU_DISCOVER,
					   &ip_mtu_discover, sizeof(ip_mtu_discover)))
				LM_WARN("Failed enable set MTU discovery socket option\n");
#endif

			if((0 <= control_cmd_tos) && (control_cmd_tos < 256)) {
				unsigned char tos = control_cmd_tos;
				if(pnode->rn_umode == RNU_UDP6) {
					if(setsockopt(rtpp_socks[pnode->idx], IPPROTO_IPV6,
							   IPV6_TCLASS, &control_cmd_tos,
							   sizeof(control_cmd_tos)))
						LM_WARN("Failed to set IPv6 TOS socket option\n");

				} else {
					if(setsockopt(rtpp_socks[pnode->idx], IPPROTO_IP, IP_TOS,
							   &tos, sizeof(tos)))
						LM_WARN("Failed to set IPv4 TOS socket option\n");
				}
			}

			if(bind_force_send_ip(pnode->idx) == -1) {
				LM_ERR("can't bind socket\n");
				close(rtpp_socks[pnode->idx]);
				rtpp_socks[pnode->idx] = -1;
				freeaddrinfo(res);

				/* retry later */
				_rtpe_list_version->vernum += 1;
				_rtpe_list_version->vertime = time(NULL);

				continue;
			}

			if(connect(rtpp_socks[pnode->idx], res->ai_addr, res->ai_addrlen)
					== -1) {
				LM_ERR("can't connect to a RTPEngine instance\n");
				close(rtpp_socks[pnode->idx]);
				rtpp_socks[pnode->idx] = -1;
				freeaddrinfo(res);

				/* retry later */
				_rtpe_list_version->vernum += 1;
				_rtpe_list_version->vertime = time(NULL);

				continue;
			}

			freeaddrinfo(res);
		rptest:
			if(rtest)
				pnode->rn_disabled = rtpp_test(pnode, 0, 1);
		}
		rtpe_reload_lock_release(rtpp_list->rset_lock);
	}
	rtpe_reload_lock_release(rtpp_set_list->rset_head_lock);

	return 0;
}

static int pv_parse_var(str *inp, pv_elem_t **outp, int *got_any)
{
	if(inp->s && *inp->s) {
		inp->len = strlen(inp->s);
		if(pv_parse_format(inp, outp) < 0) {
			LM_ERR("malformed PV string: %s\n", inp->s);
			return -1;
		}
		if(got_any)
			*got_any = 1;
	} else {
		*outp = NULL;
	}
	return 0;
}

static int minmax_pv_parse(struct minmax_mos_stats *s, int *got_any)
{
	if(pv_parse_var(&s->mos_param, &s->mos_pv, got_any))
		return -1;
	if(pv_parse_var(&s->at_param, &s->at_pv, got_any))
		return -1;
	if(pv_parse_var(&s->packetloss_param, &s->packetloss_pv, got_any))
		return -1;
	if(pv_parse_var(&s->jitter_param, &s->jitter_pv, got_any))
		return -1;
	if(pv_parse_var(&s->roundtrip_param, &s->roundtrip_pv, got_any))
		return -1;
	if(pv_parse_var(&s->roundtrip_leg_param, &s->roundtrip_leg_pv, got_any))
		return -1;
	if(pv_parse_var(&s->samples_param, &s->samples_pv, got_any))
		return -1;
	return 0;
}

static int mos_label_stats_parse(struct minmax_mos_label_stats *mmls)
{
	if(pv_parse_var(&mmls->label_param, &mmls->label_pv, &mmls->got_any_pvs))
		return -1;

	if(minmax_pv_parse(&mmls->min, &mmls->got_any_pvs))
		return -1;
	if(minmax_pv_parse(&mmls->max, &mmls->got_any_pvs))
		return -1;
	if(minmax_pv_parse(&mmls->average, &mmls->got_any_pvs))
		return -1;

	if(mmls->got_any_pvs)
		got_any_mos_pvs = 1;

	return 0;
}


static int child_init(int rank)
{
	pid_t mypid = 0;

	if(!rtpp_set_list)
		return 0;

	/* do not init sockets for PROC_INIT */
	if(rank == PROC_INIT) {
		return 0;
	}

	if(rank == PROC_MAIN) {
		if(rtpengine_dtmf_event_sock.len > 0) {
			LM_DBG("Register RTPENGINE DTMF WORKER %d\n", getpid());
			/* fork worker process */
			mypid = fork_process(PROC_RPC, "RTPENGINE DTMF WORKER", 1);
			if(mypid < 0) {
				LM_ERR("failed to fork RTPENGINE DTMF WORKER process %d\n",
						mypid);
				return -1;
			} else if(mypid == 0) {
				if(cfg_child_init())
					return -1;
				/* this will loop forever */
				rtpengine_dtmf_events_loop();
			}

			return 0;
		}

		/* do not init sockets for main process when fork=yes */
		if(dont_fork == 0)
			return 0;
	}

	/* random start value for for cookie sequence number */
	myseqn = fastrand();

	// vector of pointers to queried nodes
	queried_nodes_ptr = (struct rtpp_node **)pkg_malloc(
			MAX_RTPP_TRIED_NODES * sizeof(struct rtpp_node *));
	if(!queried_nodes_ptr) {
		LM_ERR("no more pkg memory for queried_nodes_ptr\n");
		return -1;
	}
	memset(queried_nodes_ptr, 0,
			MAX_RTPP_TRIED_NODES * sizeof(struct rtpp_node *));

	/* Iterate known RTPEngine instances - create sockets */
	if(rank == PROC_SIPINIT) {
		/* probe rtpengines only in first worker */
		if(build_rtpp_socks(0, rtpengine_ping_mode))
			return -1;
		else {
			if(build_rtpp_socks(0, 0))
				return -1;
		}
	}

	return 0;
}


static void mod_destroy(void)
{
	struct rtpp_set *crt_list, *last_list;
	struct rtpp_node *crt_rtpp, *last_rtpp;

	/*free the shared memory*/
	if(rtpp_no) {
		shm_free(rtpp_no);
		rtpp_no = NULL;
	}

	if(rtpp_no_lock) {
		lock_destroy(rtpp_no_lock);
		lock_dealloc(rtpp_no_lock);
		rtpp_no_lock = NULL;
	}

	if(!rtpp_set_list) {
		return;
	}

	if(!rtpp_set_list->rset_head_lock) {
		shm_free(rtpp_set_list);
		rtpp_set_list = NULL;
		return;
	}

	lock_get(rtpp_set_list->rset_head_lock);
	for(crt_list = rtpp_set_list->rset_first; crt_list != NULL;) {
		last_list = crt_list;

		if(!crt_list->rset_lock) {
			crt_list = last_list->rset_next;
			shm_free(last_list);
			last_list = NULL;
			continue;
		}

		lock_get(last_list->rset_lock);
		for(crt_rtpp = crt_list->rn_first; crt_rtpp != NULL;) {

			if(crt_rtpp->rn_url.s)
				shm_free(crt_rtpp->rn_url.s);

			last_rtpp = crt_rtpp;
			crt_rtpp = last_rtpp->rn_next;
			shm_free(last_rtpp);
		}
		crt_list = last_list->rset_next;
		lock_release(last_list->rset_lock);

		lock_destroy(last_list->rset_lock);
		lock_dealloc((void *)last_list->rset_lock);
		last_list->rset_lock = NULL;

		shm_free(last_list);
		last_list = NULL;
	}
	lock_release(rtpp_set_list->rset_head_lock);

	lock_destroy(rtpp_set_list->rset_head_lock);
	lock_dealloc((void *)rtpp_set_list->rset_head_lock);
	rtpp_set_list->rset_head_lock = NULL;

	shm_free(rtpp_set_list);
	rtpp_set_list = NULL;

	/* destroy the hastable which keeps the call-id <-> selected_node relation */
	if(!rtpengine_hash_table_destroy()) {
		LM_ERR("rtpengine_hash_table_destroy() failed!\n");
	} else {
		LM_DBG("rtpengine_hash_table_destroy() success!\n");
	}
	if(_rtpe_list_version != NULL) {
		shm_free(_rtpe_list_version);
		_rtpe_list_version = NULL;
	}
}


static char *gencookie(void)
{
	static char cook[34];

	snprintf(cook, 34, "%d_%u_%u ", server_id, fastrand(), myseqn);
	myseqn++;
	return cook;
}


static const char *transports[] = {
		[0x00] = "RTP/AVP",
		[0x01] = "RTP/SAVP",
		[0x02] = "RTP/AVPF",
		[0x03] = "RTP/SAVPF",
		[0x04] = "UDP/TLS/RTP/SAVP",
		[0x06] = "UDP/TLS/RTP/SAVPF",
};

static int parse_codec_flag(struct ng_flags_parse *ng_flags, const str *key,
		const str *val, const char *cmp1, const char *cmp2, const char *dictstr,
		bencode_item_t **dictp)
{
	str s;

	if(!str_key_val_prefix(key, cmp1, val, &s)) {
		if(!cmp2)
			return 0;
		if(!str_key_val_prefix(key, cmp2, val, &s))
			return 0;
	}

	if(!*dictp) {
		*dictp = bencode_list(ng_flags->dict->buffer);
		bencode_dictionary_add(ng_flags->codec, dictstr, *dictp);
	}
	bencode_list_add_str(*dictp, &s);

	return 1;
}

/**
 * parse viabranch using rtpp flags
 */
static int parse_viabranch(struct ng_flags_parse *ng_flags, struct sip_msg *msg,
		str *viabranch, char *branch_buf)
{
	char md5[MD5_LEN];
	unsigned int branch_idx;
	tm_cell_t *t;
	int ret = -1;

	/* pre-process */
	switch(ng_flags->via) {
		case 3:
			ng_flags->via = (msg->first_line.type == SIP_REPLY) ? 2 : 1;
			break;
		case -3:
			ng_flags->via = (msg->first_line.type == SIP_REPLY) ? 1 : -2;
			break;
		case -4:
			ng_flags->via = (msg->first_line.type == SIP_REPLY) ? 1 : -1;
			break;
	}

	ret = -1;
	switch(ng_flags->via) {
		case 1:
		case 2:
			ret = get_via_branch(msg, ng_flags->via, viabranch);
			break;
		case -1:
			if(extra_id_pv)
				ret = get_extra_id(msg, viabranch);
			break;
		case -2:
			if(!char_msg_val(msg, md5))
				break;
			branch_idx = 0;
			if(tmb.t_gett) {
				t = tmb.t_gett();
				if(t && t != T_UNDEFINED)
					branch_idx = t->nr_of_outgoings;
			}
			msg->hash_index = hash(msg->callid->body, get_cseq(msg)->number);

			viabranch->s = branch_buf;
			if(branch_builder(msg->hash_index, 0, md5, branch_idx, branch_buf,
					   &viabranch->len))
				ret = 0;
			break;
	}

	return ret;
}

/**
 * parse viabranch using function parameter
 */
static int parse_viabranch_with_param(struct ng_flags_parse *ng_flags,
		struct sip_msg *msg, char *branch_buf, str *p_viabranch,
		str *dst_viabranch)
{
	if(!p_viabranch)
		return -1;

	if(*p_viabranch->s == '1' || *p_viabranch->s == '2')
		ng_flags->via = *p_viabranch->s - '0';
	else if(str_eq(p_viabranch, "auto"))
		ng_flags->via = 3;
	else if(str_eq(p_viabranch, "extra"))
		ng_flags->via = -1;
	else if(str_eq(p_viabranch, "next"))
		ng_flags->via = -2;
	else if(str_eq(p_viabranch, "auto-next")
			|| str_eq(p_viabranch, "next-auto"))
		ng_flags->via = -3;
	else if(str_eq(p_viabranch, "auto-extra")
			|| str_eq(p_viabranch, "extra-auto"))
		ng_flags->via = -4;
	else
		return -1;

	return parse_viabranch(ng_flags, msg, dst_viabranch, branch_buf);
}

/**
 * parse to and from tag
 */
static int parse_from_to_tags(struct ng_flags_parse *ng_flags,
		enum rtpe_operation op, struct sip_msg *msg)
{
	if(op == OP_BLOCK_DTMF || op == OP_BLOCK_MEDIA || op == OP_UNBLOCK_DTMF
			|| op == OP_UNBLOCK_MEDIA || op == OP_START_FORWARDING
			|| op == OP_STOP_FORWARDING || op == OP_SILENCE_MEDIA
			|| op == OP_UNSILENCE_MEDIA) {
		if(ng_flags->directional) {
			bencode_dictionary_add_str(
					ng_flags->dict, "from-tag", &ng_flags->from_tag);
			if(ng_flags->to && ng_flags->to_tag.s && ng_flags->to_tag.len)
				bencode_dictionary_add_str(
						ng_flags->dict, "to-tag", &ng_flags->to_tag);
		}
	} else if(op == OP_SUBSCRIBE_REQUEST) {
		/* SUBSCRIBE can either specify a list of from tags or have the keyword
		 * all
		 * */
		if(ng_flags->from_tags)
			bencode_dictionary_add(
					ng_flags->dict, "from-tags", ng_flags->from_tags);
		/* SUBSCRIBE can specify a to tag, if none specified a to-tag gets auto
		 * generated
		 * */
		if(ng_flags->to && ng_flags->to_tag.s && ng_flags->to_tag.len)
			bencode_dictionary_add_str(
					ng_flags->dict, "to-tag", &ng_flags->to_tag);
	} else if(op == OP_SUBSCRIBE_ANSWER || op == OP_UNSUBSCRIBE) {
		/* SUBSCRIBE answer and UNSUBSCRIBE should specify the tag matching the
		 * original subscribe request.  Internal api passes it via extra_dict
		 * so this can be missing from flags in that case
		 */
		if(ng_flags->to && ng_flags->to_tag.s && ng_flags->to_tag.len)
			bencode_dictionary_add_str(
					ng_flags->dict, "to-tag", &ng_flags->to_tag);
	} else if((msg->first_line.type == SIP_REQUEST && op != OP_ANSWER)
			  || (msg->first_line.type == SIP_REPLY && op == OP_DELETE)
			  || (msg->first_line.type == SIP_REPLY && op == OP_ANSWER)
			  || ng_flags->directional) /* set if from-tag was set manually */
	{
		bencode_dictionary_add_str(
				ng_flags->dict, "from-tag", &ng_flags->from_tag);
		if(ng_flags->to && ng_flags->to_tag.s && ng_flags->to_tag.len)
			bencode_dictionary_add_str(
					ng_flags->dict, "to-tag", &ng_flags->to_tag);
	} else {
		if(!ng_flags->to_tag.s || !ng_flags->to_tag.len) {
			LM_ERR("No to-tag present\n");
			return -1;
		}
		bencode_dictionary_add_str(
				ng_flags->dict, "from-tag", &ng_flags->to_tag);
		bencode_dictionary_add_str(
				ng_flags->dict, "to-tag", &ng_flags->from_tag);
	}
	return 0;
}

/**
 * Parse the flags string
 */
static int parse_flags(struct ng_flags_parse *ng_flags, struct sip_msg *msg,
		enum rtpe_operation *op, const char *flags_str)
{
	char *e;
	const char *err;
	str key, val, s, s1;
	int ip_af = AF_UNSPEC;
	if(!flags_str)
		return 0;

	while(1) {
		while(*flags_str == ' ')
			flags_str++;

		key.s = (void *)flags_str;
		val.len = key.len = -1;
		val.s = NULL;

		e = strpbrk(key.s, " =");
		if(!e)
			e = key.s + strlen(key.s);
		else if(*e == '=') {
			key.len = e - key.s;
			val.s = e + 1;
			e = strchr(val.s, ' ');
			if(!e)
				e = val.s + strlen(val.s);
			val.len = e - val.s;
		}

		if(key.len == -1)
			key.len = e - key.s;
		if(!key.len)
			break;

		/* check for items which have their own sub-list */
		if(str_key_val_prefix(&key, "replace", &val, &s)) {
			bencode_list_add_str(ng_flags->replace, &s);
			goto next;
		}
		if(str_key_val_prefix(&key, "received-from", &val, &s)) {
			ip_af = get_ip_type(s.s);
			if(ip_af == AF_INET) {
				s1.s = "IP4";
				s1.len = 3;
				bencode_list_add_str(ng_flags->received_from, &s1);
				bencode_list_add_str(ng_flags->received_from, &s);

			} else if(ip_af == AF_INET6) {
				s1.s = "IP6";
				s1.len = 3;
				bencode_list_add_str(ng_flags->received_from, &s1);
				bencode_list_add_str(ng_flags->received_from, &s);
			}


			goto next;
		}
		if(str_key_val_prefix(&key, "SDES", &val, &s)) {
			bencode_list_add_str(ng_flags->sdes, &s);
			goto next;
		}
		if(str_key_val_prefix(&key, "T38", &val, &s)
				|| str_key_val_prefix(&key, "T.38", &val, &s)) {
			bencode_list_add_str(ng_flags->t38, &s);
			goto next;
		}
		if(str_key_val_prefix(&key, "rtcp-mux", &val, &s)) {
			bencode_list_add_str(ng_flags->rtcp_mux, &s);
			goto next;
		}

		if(parse_codec_flag(ng_flags, &key, &val, "transcode",
				   "codec-transcode", "transcode", &ng_flags->codec_transcode))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-strip", NULL, "strip",
				   &ng_flags->codec_strip))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-offer", NULL, "offer",
				   &ng_flags->codec_offer))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-mask", NULL, "mask",
				   &ng_flags->codec_mask))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-set", NULL, "set",
				   &ng_flags->codec_set))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-except", NULL,
				   "except", &ng_flags->codec_except))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-accept", NULL,
				   "accept", &ng_flags->codec_accept))
			goto next;
		if(parse_codec_flag(ng_flags, &key, &val, "codec-consume", NULL,
				   "consume", &ng_flags->codec_consume))
			goto next;

		/* check for specially handled items */
		switch(key.len) {
			case 3:
				if(str_eq(&key, "RTP") && !val.s) {
					ng_flags->transport |= 0x100;
					ng_flags->transport &= ~0x001;
				} else if(str_eq(&key, "AVP") && !val.s) {
					ng_flags->transport |= 0x100;
					ng_flags->transport &= ~0x002;
				} else if(str_eq(&key, "TOS") && val.s) {
					bencode_dictionary_add_integer(
							ng_flags->dict, "TOS", atoi(val.s));
				} else
					goto generic;
				goto next;
				break;

			case 4:
				if(str_eq(&key, "SRTP") && !val.s)
					ng_flags->transport |= 0x101;
				else if(str_eq(&key, "AVPF") && !val.s)
					ng_flags->transport |= 0x102;
				else if(str_eq(&key, "DTLS") && !val.s)
					ng_flags->transport |= 0x104;
				else
					goto generic;
				goto next;
				break;

			case 6:
				if(str_eq(&key, "to-tag")) {
					if(val.s)
						ng_flags->to_tag = val;
					ng_flags->to = 1;
					goto next;
				}
				break;

			case 7:
				if(str_eq(&key, "RTP/AVP") && !val.s)
					ng_flags->transport = 0x100;
				else if(str_eq(&key, "call-id")) {
					err = "missing value";
					if(!val.s)
						goto error;
					ng_flags->call_id = val;
				} else
					goto generic;
				goto next;
				break;

			case 8:
				if(str_eq(&key, "internal") || str_eq(&key, "external"))
					bencode_list_add_str(ng_flags->direction, &key);
				else if(str_eq(&key, "RTP/AVPF") && !val.s)
					ng_flags->transport = 0x102;
				else if(str_eq(&key, "RTP/SAVP") && !val.s)
					ng_flags->transport = 0x101;
				else if(str_eq(&key, "from-tag")) {
					err = "missing value";
					if(!val.s)
						goto error;
					ng_flags->from_tag = val;
					ng_flags->directional = 1;
				} else
					goto generic;
				goto next;
				break;

			case 9:
				if(str_eq(&key, "RTP/SAVPF") && !val.s)
					ng_flags->transport = 0x103;
				else if(str_eq(&key, "direction"))
					bencode_list_add_str(ng_flags->direction, &val);
				else if(str_eq(&key, "from-tags")) {
					err = "missing value";
					if(!val.s)
						goto error;

					if(!ng_flags->from_tags) {
						ng_flags->from_tags =
								bencode_list(ng_flags->dict->buffer);
					}
					bencode_list_add_str(ng_flags->from_tags, &val);
				} else
					goto generic;
				goto next;
				break;

			case 10:
				if(str_eq(&key, "via-branch")) {
					err = "missing value";
					if(!val.s)
						goto error;
					err = "invalid value";
					if(*val.s == '1' || *val.s == '2')
						ng_flags->via = *val.s - '0';
					else if(str_eq(&val, "auto"))
						ng_flags->via = 3;
					else if(str_eq(&val, "extra"))
						ng_flags->via = -1;
					else if(str_eq(&val, "next"))
						ng_flags->via = -2;
					else if(str_eq(&val, "auto-next")
							|| str_eq(&val, "next-auto"))
						ng_flags->via = -3;
					else if(str_eq(&val, "auto-extra")
							|| str_eq(&val, "extra-auto"))
						ng_flags->via = -4;
					else
						goto error;
					goto next;
				}
				break;

			case 11:
				if(str_eq(&key, "repacketize")) {
					err = "missing value";
					if(!val.s)
						goto error;
					ng_flags->packetize = 0;
					while(isdigit(*val.s)) {
						ng_flags->packetize *= 10;
						ng_flags->packetize += *val.s - '0';
						val.s++;
					}
					err = "invalid value";
					if(!ng_flags->packetize)
						goto error;
					bencode_dictionary_add_integer(
							ng_flags->dict, "repacketize", ng_flags->packetize);
				} else if(str_eq(&key, "directional"))
					ng_flags->directional = 1;
				else
					goto generic;
				goto next;
				break;

			case 12:
				if(str_eq(&key, "force-answer")) {
					err = "cannot force answer in non-offer command";
					if(*op != OP_OFFER)
						goto error;
					*op = OP_ANSWER;
					goto next;
				} else if(str_eq(&key, "delete-delay") && val.s)
					bencode_dictionary_add_integer(
							ng_flags->dict, "delete delay", atoi(val.s));
				break;


			case 16:
				if(str_eq(&key, "UDP/TLS/RTP/SAVP") && !val.s)
					ng_flags->transport = 0x104;
				else
					goto generic;
				goto next;
				break;

			case 17:
				if(str_eq(&key, "UDP/TLS/RTP/SAVPF") && !val.s)
					ng_flags->transport = 0x106;
				else
					goto generic;
				goto next;
				break;
		}

	generic:
		if(!val.s) {
			LM_DBG("Setting flag %.*s\n", key.len, key.s);
			bencode_list_add_str(ng_flags->flags, &key);
		} else
			bencode_dictionary_str_add_str(ng_flags->dict, &key, &val);
		goto next;

	next:
		flags_str = e;
	}

	return 0;

error:
	if(val.s)
		LM_ERR("error processing flag `%.*s' (value '%.*s'): %s\n", key.len,
				key.s, val.len, val.s, err);
	else
		LM_ERR("error processing flag `%.*s': %s\n", key.len, key.s, err);
	return -1;
}

/**
 * flags - rtpp flags in a raw format (plain text)
 * p_viabranch - can be NULL. If not NULL flags are parsed on the daemon side,
 *             if it's NULL, flags are parsed by the module.
 *             Allowed values similarly to the flag option `via-branch`.
 */
static bencode_item_t *rtpp_function_call(bencode_buffer_t *bencbuf,
		struct sip_msg *msg, enum rtpe_operation op, str *flags,
		str *p_viabranch, str *body_out, str *cl_field,
		bencode_item_t *extra_dict)
{
	struct ng_flags_parse ng_flags;
	bencode_item_t *item, *resp;
	bencode_item_t *result;
	pv_value_t pv_val;
	str viabranch = STR_NULL;
	str body = STR_NULL, error = STR_NULL, tmp_callid = STR_NULL;
	int ret, queried_nodes = 0, cont_type = 0;
	unsigned int parse_by_module = (p_viabranch) ? 0 : 1;
	struct rtpp_node *node;
	char *cp;
	char branch_buf[MAX_BRANCH_PARAM_LEN];

	body.s = NULL;

	memset(&ng_flags, 0, sizeof(ng_flags));

	/* get call-id, to-tag, from-tag from the SIP message */
	if(IS_SIP(msg) || IS_SIP_REPLY(msg)) {
		if(get_callid(msg, &ng_flags.call_id) == -1
				|| ng_flags.call_id.len == 0) {
			LM_ERR("can't get Call-Id field\n");
			return NULL;
		}
		if(get_to_tag(msg, &ng_flags.to_tag) == -1) {
			LM_ERR("can't get To tag\n");
			return NULL;
		}
		if(get_from_tag(msg, &ng_flags.from_tag) == -1
				|| ng_flags.from_tag.len == 0) {
			LM_ERR("can't get From tag\n");
			return NULL;
		}
	}

	/* initialize bencode buffer */
	if(bencode_buffer_init(bencbuf)) {
		LM_ERR("could not initialize bencode_buffer_t\n");
		return NULL;
	}

	/* initialize some basic bencode items */
	if(!extra_dict) {
		ng_flags.dict = bencode_dictionary(bencbuf);
		if(parse_by_module) {
			ng_flags.flags = bencode_list(bencbuf);
		}
	} else {
		ng_flags.dict = extra_dict;
		ng_flags.flags = bencode_dictionary_get(ng_flags.dict, "flags");
		bencode_dictionary_get_str(ng_flags.dict, "call-id", &tmp_callid);
		if(tmp_callid.len > 0) {
			ng_flags.call_id = tmp_callid;
		}
	}

	if(parse_by_module) {
		ng_flags.received_from = bencode_list(bencbuf);
	}

	item = bencode_dictionary_add_list(ng_flags.dict, "supports");
	bencode_list_add_string(item, "load limit");

	/* offer/asnwer specific things */
	if(op == OP_OFFER || op == OP_ANSWER) {
		/* create these bencode items only if parsing is local */
		if(parse_by_module && flags) {
			ng_flags.direction = bencode_list(bencbuf);
			ng_flags.replace = bencode_list(bencbuf);
			ng_flags.rtcp_mux = bencode_list(bencbuf);
			ng_flags.sdes = bencode_list(bencbuf);
			ng_flags.t38 = bencode_list(bencbuf);
			ng_flags.codec = bencode_dictionary(bencbuf);
		}
	}
	if(op == OP_OFFER || op == OP_ANSWER || op == OP_SUBSCRIBE_ANSWER) {
		/* get SDP body */
		if(read_sdp_pvar != NULL) {
			if(read_sdp_pvar->getf(msg, &read_sdp_pvar->pvp, &pv_val) < 0) {
				LM_ERR("error getting pvar value <%.*s>\n",
						read_sdp_pvar_str.len, read_sdp_pvar_str.s);
				goto error;
			} else {
				body = pv_val.rs;
			}

		} else if((cont_type = extract_body(msg, &body, cl_field)) == -1) {
			LM_ERR("can't extract body from the message\n");
			goto error;
		}
		if(body_intermediate.s)
			bencode_dictionary_add_str(
					ng_flags.dict, "sdp", &body_intermediate);
		else
			bencode_dictionary_add_str(ng_flags.dict, "sdp", &body);
	}

	/**
	 * flags prasing
	 */

	/* affects to-tag parsing */
	ng_flags.to = (!parse_by_module
						  || (op != OP_DELETE && op != OP_SUBSCRIBE_REQUEST))
						  ? 1
						  : 0;

	/* module specific parsing */
	if(parse_by_module && flags && parse_flags(&ng_flags, msg, &op, flags->s))
		goto error;

	/* if it's not SIP, check additionally if the call-id and from tag have been set at all */
	if(!IS_SIP(msg) && !IS_SIP_REPLY(msg)) {
		/* check required values */
		if(ng_flags.call_id.len == 0) {
			LM_ERR("can't get Call-Id field\n");
			return NULL;
		}
		if(ng_flags.from_tag.len == 0) {
			LM_ERR("can't get From tag\n");
			return NULL;
		}
	}

	/* module specific parsing,
	 * but only add those if any flags were given at all */
	if(parse_by_module && flags) {
		/* direction */
		if(ng_flags.direction && ng_flags.direction->child)
			bencode_dictionary_add(
					ng_flags.dict, "direction", ng_flags.direction);
		/* flags */
		if(ng_flags.flags && ng_flags.flags->child)
			bencode_dictionary_add(ng_flags.dict, "flags", ng_flags.flags);
		/* replace */
		if(ng_flags.replace && ng_flags.replace->child)
			bencode_dictionary_add(ng_flags.dict, "replace", ng_flags.replace);
		/* codec */
		if(ng_flags.codec && ng_flags.codec->child)
			bencode_dictionary_add(ng_flags.dict, "codec", ng_flags.codec);
		/* transport-protocol */
		if((ng_flags.transport & 0x100))
			bencode_dictionary_add_string(ng_flags.dict, "transport-protocol",
					transports[ng_flags.transport & 0x007]);
		/* rtcp-mux */
		if(ng_flags.rtcp_mux && ng_flags.rtcp_mux->child)
			bencode_dictionary_add(
					ng_flags.dict, "rtcp-mux", ng_flags.rtcp_mux);
		/* SDES */
		if(ng_flags.sdes && ng_flags.sdes->child)
			bencode_dictionary_add(ng_flags.dict, "SDES", ng_flags.sdes);
		/* T.38 */
		if(ng_flags.t38 && ng_flags.t38->child)
			bencode_dictionary_add(ng_flags.dict, "T.38", ng_flags.t38);
		/* received-from */
		if(ng_flags.received_from && ng_flags.received_from->child) {
			bencode_dictionary_add(
					ng_flags.dict, "received-from", ng_flags.received_from);
		} else {
			bencode_dictionary_add(
					ng_flags.dict, "received-from", ng_flags.received_from);
			bencode_list_add_string(ng_flags.received_from,
					(msg->rcv.src_ip.af == AF_INET)
							? "IP4"
							: ((msg->rcv.src_ip.af == AF_INET6) ? "IP6" : "?"));
			bencode_list_add_string(
					ng_flags.received_from, ip_addr2a(&msg->rcv.src_ip));
		}
	}

	/* bencode items which are to be added always */
	{
		/* trickle ice sdp fragment */
		if(cont_type == 3)
			bencode_list_add_string(ng_flags.flags, "fragment");

		/* call-id */
		bencode_dictionary_add_str(ng_flags.dict, "call-id", &ng_flags.call_id);

		/* viabranch */
		if(parse_by_module && ng_flags.via) {
			LM_DBG("parsing viabranch using rtpp flags\n");
			ret = parse_viabranch(&ng_flags, msg, &viabranch, branch_buf);
			if(ret == -1 || viabranch.len == 0) {
				LM_ERR("can't get Via branch/extra ID\n");
				goto error;
			}
		} else if(p_viabranch && !str_eq(p_viabranch, "none")) {
			LM_DBG("parsing viabranch using function parameter\n");
			ret = parse_viabranch_with_param(
					&ng_flags, msg, branch_buf, p_viabranch, &viabranch);
			if(ret == -1 || viabranch.len == 0) {
				LM_ERR("can't get Via branch/extra ID\n");
				goto error;
			}
		}
		if(viabranch.s && viabranch.len) {
			bencode_dictionary_add_str(ng_flags.dict, "via-branch", &viabranch);
		}

		/* from/to tags */
		if(parse_from_to_tags(&ng_flags, op, msg))
			goto error;

		/* rtpengine command */
		bencode_dictionary_add_string(
				ng_flags.dict, "command", command_strings[op]);

		/* sip message type */
		bencode_dictionary_add_string(ng_flags.dict, "sip-message-type",
				sip_type_strings[msg->first_line.type]);
	}

	/* add rtpp flags, if parsed by daemon */
	if(!parse_by_module && flags)
		bencode_dictionary_add_str(ng_flags.dict, "rtpp-flags", flags);

	/**
	 * send it out
	 */

	if(bencbuf->error) {
		LM_ERR("out of memory - bencode failed\n");
		goto error;
	}

	if(msg->id != current_msg_id)
		active_rtpp_set = default_rtpp_set;

select_node:
	do {
		if(queried_nodes
				>= cfg_get(rtpengine, rtpengine_cfg, queried_nodes_limit)) {
			LM_ERR("queried nodes limit reached\n");
			goto error;
		}

		node = select_rtpp_node(ng_flags.call_id, viabranch, 1,
				queried_nodes_ptr, queried_nodes, op);
		if(!node) {
			LM_ERR("no available proxies\n");
			goto error;
		}

		cp = send_rtpp_command(node, ng_flags.dict, &ret);
		/* if node is disabled permanent, don't recheck it later */
		if(cp == NULL
				&& node->rn_recheck_ticks != RTPENGINE_MAX_RECHECK_TICKS) {
			node->rn_disabled = 1;
			node->rn_recheck_ticks =
					get_ticks()
					+ cfg_get(rtpengine, rtpengine_cfg, rtpengine_disable_tout);
		}

		queried_nodes_ptr[queried_nodes++] = node;
	} while(cp == NULL);

	LM_DBG("proxy reply: %.*s\n", ret, cp);

	set_rtp_inst_pvar(msg, &node->rn_url);
	/*** process reply ***/

	resp = bencode_decode_expect(bencbuf, cp, ret, BENCODE_DICTIONARY);
	if(!resp) {
		LM_ERR("failed to decode bencoded reply from proxy: %.*s\n", ret, cp);
		goto error;
	}

	result = bencode_dictionary_get_expect(resp, "result", BENCODE_STRING);
	if(!result) {
		LM_ERR("No 'result' dictionary entry in response from proxy %.*s",
				node->rn_url.len, node->rn_url.s);
		goto error;
	}

	if(!bencode_strcmp(result, "load limit")) {
		item = bencode_dictionary_get_expect(resp, "message", BENCODE_STRING);
		if(!item)
			LM_INFO("proxy %.*s has reached its load limit - trying next one",
					node->rn_url.len, node->rn_url.s);
		else
			LM_INFO("proxy %.*s has reached its load limit (%.*s) - trying "
					"next one",
					node->rn_url.len, node->rn_url.s, (int)item->iov[1].iov_len,
					(char *)item->iov[1].iov_base);
		goto select_node;
	}

	if(!bencode_strcmp(result, "error")) {
		if(!bencode_dictionary_get_str(resp, "error-reason", &error)) {
			LM_ERR("proxy return error but didn't give an error reason: %.*s\n",
					ret, cp);
		} else {
			if((RTPENGINE_SESS_LIMIT_MSG_LEN == error.len)
					&& (strncmp(error.s, RTPENGINE_SESS_LIMIT_MSG,
								RTPENGINE_SESS_LIMIT_MSG_LEN)
							== 0)) {
				LM_WARN("proxy %.*s: %.*s", node->rn_url.len, node->rn_url.s,
						error.len, error.s);
				goto select_node;
			}
			if((RTPENGINE_SESS_OUT_OF_PORTS_MSG_LEN == error.len)
					&& (strncmp(error.s, RTPENGINE_SESS_OUT_OF_PORTS_MSG,
								RTPENGINE_SESS_OUT_OF_PORTS_MSG_LEN)
							== 0)) {
				LM_WARN("proxy %.*s: %.*s", node->rn_url.len, node->rn_url.s,
						error.len, error.s);
				goto select_node;
			}

			LM_ERR("proxy replied with error: %.*s\n", error.len, error.s);
		}
		goto error;
	}

	/* add hastable entry with the node => */
	if(!rtpengine_hash_table_lookup(ng_flags.call_id, viabranch, op)) {
		// build the entry
		struct rtpengine_hash_entry *entry =
				shm_malloc(sizeof(struct rtpengine_hash_entry));
		if(!entry) {
			LM_ERR("rtpengine hash table fail to create entry for calllen=%d "
				   "callid=%.*s viabranch=%.*s\n",
					ng_flags.call_id.len, ng_flags.call_id.len,
					ng_flags.call_id.s, viabranch.len, viabranch.s);
			goto skip_hash_table_insert;
		}
		memset(entry, 0, sizeof(struct rtpengine_hash_entry));

		// fill the entry
		if(ng_flags.call_id.s && ng_flags.call_id.len > 0) {
			if(shm_str_dup(&entry->callid, &ng_flags.call_id) < 0) {
				LM_ERR("rtpengine hash table fail to duplicate calllen=%d "
					   "callid=%.*s\n",
						ng_flags.call_id.len, ng_flags.call_id.len,
						ng_flags.call_id.s);
				rtpengine_hash_table_free_entry(entry);
				goto skip_hash_table_insert;
			}
		}
		if(viabranch.s && viabranch.len > 0) {
			if(shm_str_dup(&entry->viabranch, &viabranch) < 0) {
				LM_ERR("rtpengine hash table fail to duplicate calllen=%d "
					   "viabranch=%.*s\n",
						ng_flags.call_id.len, viabranch.len, viabranch.s);
				rtpengine_hash_table_free_entry(entry);
				goto skip_hash_table_insert;
			}
		}
		entry->node = node;
		entry->next = NULL;
		entry->tout = get_ticks() + hash_table_tout;

		// insert the key<->entry from the hashtable
		if(!rtpengine_hash_table_insert(ng_flags.call_id, viabranch, entry)) {
			LM_ERR("rtpengine hash table fail to insert node=%.*s for "
				   "calllen=%d callid=%.*s viabranch=%.*s\n",
					node->rn_url.len, node->rn_url.s, ng_flags.call_id.len,
					ng_flags.call_id.len, ng_flags.call_id.s, viabranch.len,
					viabranch.s);
			rtpengine_hash_table_free_entry(entry);
			goto skip_hash_table_insert;
		} else {
			LM_DBG("rtpengine hash table insert node=%.*s for calllen=%d "
				   "callid=%.*s viabranch=%.*s\n",
					node->rn_url.len, node->rn_url.s, ng_flags.call_id.len,
					ng_flags.call_id.len, ng_flags.call_id.s, viabranch.len,
					viabranch.s);
			if(rtpengine_enable_dmq > 0)
				rtpengine_dmq_replicate_insert(
						ng_flags.call_id, viabranch, entry);
		}
	}

skip_hash_table_insert:
	if(body_out)
		*body_out = body;

	if(op == OP_DELETE) {
		/* Delete the key<->value from the hashtable */
		if(!rtpengine_hash_table_remove(ng_flags.call_id, viabranch, op)) {
			LM_ERR("rtpengine hash table failed to remove entry for callen=%d "
				   "callid=%.*s viabranch=%.*s\n",
					ng_flags.call_id.len, ng_flags.call_id.len,
					ng_flags.call_id.s, viabranch.len, viabranch.s);
		} else {
			LM_DBG("rtpengine hash table remove entry for callen=%d "
				   "callid=%.*s viabranch=%.*s\n",
					ng_flags.call_id.len, ng_flags.call_id.len,
					ng_flags.call_id.s, viabranch.len, viabranch.s);
			if(rtpengine_enable_dmq > 0)
				rtpengine_dmq_replicate_remove(ng_flags.call_id, viabranch);
		}
	}

	return resp;

error:
	bencode_buffer_free(bencbuf);
	return NULL;
}

static int rtpp_function_call_simple(
		struct sip_msg *msg, enum rtpe_operation op, void *d)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;
	bencode_item_t *ret;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	ret = rtpp_function_call(
			&bencbuf, msg, op, flags, viabranch, NULL, NULL, NULL);
	if(!ret)
		return -1;

	if(bencode_dictionary_get_strcmp(ret, "result", "ok")) {
		LM_ERR("proxy didn't return \"ok\" result\n");
		bencode_buffer_free(&bencbuf);
		return -1;
	}

	bencode_buffer_free(&bencbuf);
	return 1;
}

static int rtpengine_simple_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	return rtpp_function_call_simple(msg, op, d);
}


static bencode_item_t *rtpp_function_call_ok(bencode_buffer_t *bencbuf,
		struct sip_msg *msg, enum rtpe_operation op, str *flags, str *viabranch,
		str *body, str *cl_field, bencode_item_t *dict)
{
	bencode_item_t *ret;

	ret = rtpp_function_call(
			bencbuf, msg, op, flags, viabranch, body, cl_field, dict);
	if(!ret)
		return NULL;

	if(bencode_dictionary_get_strcmp(ret, "result", "ok")) {
		LM_ERR("proxy didn't return \"ok\" result\n");
		bencode_buffer_free(bencbuf);
		return NULL;
	}

	return ret;
}

/**
* @brief Timer function to check the status of rtpengine nodes.
* Check every node in the set and if it is not responding,
* mark it as disabled.
 */
static void rtpengine_ping_check_timer(unsigned int ticks, void *param)
{
	struct rtpp_set *rtpp_list;
	struct rtpp_node *crt_rtpp;
	int err = 0;
	int ret;
	int rtpp_disabled = 0;

	/* No need to test them while building */
	if(build_rtpp_socks(1, 0)) {
		return;
	}
	/* Most of this is from rtpengine_rpc_iterate functions maybe split? */
	LM_DBG("Pinging all enabled rtpengines...\n");
	lock_get(rtpp_set_list->rset_head_lock);
	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != NULL;
			rtpp_list = rtpp_list->rset_next) {

		lock_get(rtpp_list->rset_lock);
		for(crt_rtpp = rtpp_list->rn_first; crt_rtpp != NULL;
				crt_rtpp = crt_rtpp->rn_next) {

			if(!crt_rtpp->rn_displayed || crt_rtpp->rn_disabled) {
				continue;
			}

			/* Ping all available nodes */
			ret = rtpengine_iter_cb_ping(crt_rtpp, rtpp_list, &rtpp_disabled);
			if(ret) {
				err = 1;
				break;
			}
		}
		lock_release(rtpp_list->rset_lock);

		if(err)
			break;
	}
	lock_release(rtpp_set_list->rset_head_lock);
}

/**
 * @brief Tests the RTP engine node by sending a ping command with some additional logic
 * to skip it.
 *
 * @details Similar to rtpp_test_ping but provides additional logic for handling disabled
 * nodes, ping intervals, and recheck ticks.
 *
 * @param node The RTP engine node to test.
 * @param isdisabled Flag indicating if the node is currently disabled.
 * @param force Flag to force the test even if the node is disabled or ping_interval is set.
 * @return Returns 0 if the node is NOT disabled, 1 otherwise.
 */
static int rtpp_test(struct rtpp_node *node, int isdisabled, int force)
{
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;
	char *cp;
	int ret;

	if(node->rn_recheck_ticks == RTPENGINE_MAX_RECHECK_TICKS) {
		LM_DBG("rtpp %s disabled for ever\n", node->rn_url.s);
		return 1;
	}

	if(force == 0) {
		if(isdisabled == 0)
			return 0;
		/* If ping_interval is set, the timer will ping and test
		the rtps. No need to do something during routing.
		Return the current status.
		*/
		if(rtpengine_ping_interval > 0)
			return isdisabled;
		if(node->rn_recheck_ticks > get_ticks())
			return 1;
	}

	if(bencode_buffer_init(&bencbuf)) {
		LM_ERR("could not initialized bencode_buffer_t\n");
		return 1;
	}
	dict = bencode_dictionary(&bencbuf);
	bencode_dictionary_add_string(dict, "command", "ping");
	if(bencbuf.error)
		goto benc_error;

	cp = send_rtpp_command(node, dict, &ret);
	if(!cp) {
		node->rn_disabled = 1;
		node->rn_recheck_ticks =
				get_ticks()
				+ cfg_get(rtpengine, rtpengine_cfg, rtpengine_disable_tout);
		LM_ERR("proxy did not respond to ping\n");
		goto error;
	}

	dict = bencode_decode_expect(&bencbuf, cp, ret, BENCODE_DICTIONARY);
	if(!dict || bencode_dictionary_get_strcmp(dict, "result", "pong")) {
		LM_ERR("proxy responded with invalid response\n");
		goto error;
	}

	LM_INFO("rtpengine instance <%s> found, support for it %senabled\n",
			node->rn_url.s, force == 0 ? "re-" : "");

	bencode_buffer_free(&bencbuf);
	return 0;

benc_error:
	LM_ERR("out of memory - bencode failed\n");
error:
	bencode_buffer_free(&bencbuf);
	return 1;
}

static char *send_rtpp_command(
		struct rtpp_node *node, bencode_item_t *dict, int *outlen)
{
	struct sockaddr_un addr;
	int fd = -1, len, i, vcnt;
	int rtpengine_retr, rtpengine_tout_ms = 1000;
	char *cp;
	static char buf[0x40000];
	struct pollfd fds[1];
	struct iovec *v;
	str cmd = STR_NULL;
	const static str rtpe_proto = {"ng.rtpengine.com", 16};
	str request, response;

	v = bencode_iovec(dict, &vcnt, 1, 0);
	if(!v) {
		LM_ERR("error converting bencode to iovec\n");
		return NULL;
	}

	len = 0;
	cp = buf;
	rtpengine_tout_ms = cfg_get(rtpengine, rtpengine_cfg, rtpengine_tout_ms);

	if(node->rn_umode == RNU_LOCAL) {
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_LOCAL;
		strncpy(addr.sun_path, node->rn_address, sizeof(addr.sun_path) - 1);
#ifdef HAVE_SOCKADDR_SA_LEN
		addr.sun_len = strlen(addr.sun_path);
#endif

		fd = socket(AF_LOCAL, SOCK_STREAM, 0);
		if(fd < 0) {
			LM_ERR("can't create socket\n");
			goto badproxy;
		}
		if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LM_ERR("can't connect to RTPEngine <%s> (%s:%d)\n", node->rn_url.s,
					strerror(errno), errno);
			goto badproxy;
		}

		do {
			len = writev(fd, v + 1, vcnt);
		} while(len == -1 && errno == EINTR);
		if(len <= 0) {
			LM_ERR("can't send command to RTPEngine <%s> (%s:%d)\n",
					node->rn_url.s, strerror(errno), errno);
			goto badproxy;
		}
		do {
			len = read(fd, buf, sizeof(buf) - 1);
		} while(len == -1 && errno == EINTR);
		if(len <= 0) {
			LM_ERR("can't read reply from RTPEngine <%s> (%s:%d)\n",
					node->rn_url.s, strerror(errno), errno);
			goto badproxy;
		}
		close(fd);
	} else if(node->rn_umode == RNU_WS || node->rn_umode == RNU_WSS) {
		/* assemble full request string, flatten iovec */
		v[0].iov_base = gencookie();
		v[0].iov_len = strlen(v[0].iov_base);
		len = 0;
		for(i = 0; i <= vcnt; i++)
			len += v[i].iov_len;
		request.s = pkg_malloc(len + 1);
		if(!request.s) {
			LM_ERR("out of memory\n");
			goto badproxy;
		}
		len = 0;
		for(i = 0; i <= vcnt; i++) {
			memcpy(request.s + len, v[i].iov_base, v[i].iov_len);
			len += v[i].iov_len;
		}
		request.s[len] = '\0';
		request.len = len;

		len = _rtpe_lwscb.request(&node->rn_url, (str *)&rtpe_proto, &request,
				&response, rtpengine_tout_ms * 1000);
		pkg_free(request.s);

		if(len < 0) {
			LM_ERR("failed to do websocket request\n");
			goto badproxy;
		}

		/* process/copy response; verify cookie */
		if(response.len < v[0].iov_len) {
			LM_ERR("empty or short websocket response\n");
			pkg_free(response.s);
			goto badproxy;
		}
		if(memcmp(response.s, v[0].iov_base, v[0].iov_len)) {
			LM_ERR("mismatched cookie in websocket response\n");
			pkg_free(response.s);
			goto badproxy;
		}
		len = response.len - v[0].iov_len;
		if(len >= sizeof(buf)) {
			LM_ERR("websocket response too large\n");
			pkg_free(response.s);
			goto badproxy;
		}
		memcpy(buf, response.s + v[0].iov_len, len);
		pkg_free(response.s);
	} else {
		/* UDP or UDP6 */
		fds[0].fd = rtpp_socks[node->idx];
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		/* Drain input buffer */
		while((poll(fds, 1, 0) == 1) && ((fds[0].revents & POLLIN) != 0)) {
			/* coverity[check_return : FALSE] */
			recv(rtpp_socks[node->idx], buf, sizeof(buf) - 1, 0);
			fds[0].revents = 0;
		}
		v[0].iov_base = gencookie();
		v[0].iov_len = strlen(v[0].iov_base);
		rtpengine_retr = cfg_get(rtpengine, rtpengine_cfg, rtpengine_retr);
		for(i = 0; i < rtpengine_retr; i++) {
			do {
				len = writev(rtpp_socks[node->idx], v, vcnt + 1);
			} while(len == -1 && (errno == EINTR || errno == ENOBUFS));
			if(len <= 0) {
				bencode_get_str(bencode_dictionary_get(dict, "command"), &cmd);
				LM_ERR("can't send command \"%.*s\" to RTPEngine <%s> "
					   "(%s:%d)\n",
						cmd.len, cmd.s, node->rn_url.s, strerror(errno), errno);
				goto badproxy;
			}
			while((poll(fds, 1, rtpengine_tout_ms) == 1)
					&& (fds[0].revents & POLLIN) != 0) {
				do {
					len = recv(rtpp_socks[node->idx], buf, sizeof(buf) - 1, 0);
				} while(len == -1 && errno == EINTR);
				if(len <= 0) {
					bencode_get_str(
							bencode_dictionary_get(dict, "command"), &cmd);
					LM_ERR("can't read reply for command \"%.*s\" from "
						   "RTPEngine <%s> (%s:%d)\n",
							cmd.len, cmd.s, node->rn_url.s, strerror(errno),
							errno);
					goto badproxy;
				}
				if(len >= (v[0].iov_len - 1)
						&& memcmp(buf, v[0].iov_base, (v[0].iov_len - 1))
								   == 0) {
					len -= (v[0].iov_len - 1);
					cp += (v[0].iov_len - 1);
					if(len != 0) {
						len--;
						cp++;
					}
					goto out;
				}
				fds[0].revents = 0;
			}
		}
		if(i == rtpengine_retr) {
			bencode_get_str(bencode_dictionary_get(dict, "command"), &cmd);
			LM_ERR("timeout waiting reply for command \"%.*s\" from RTPEngine "
				   "<%s>\n",
					cmd.len, cmd.s, node->rn_url.s);
			goto badproxy;
		}
	}

out:
	cp[len] = '\0';
	*outlen = len;
	return cp;

badproxy:
	if(fd >= 0)
		close(fd);
	return NULL;
}

/*
 * select the set with the id_set id
 */

static struct rtpp_set *select_rtpp_set(unsigned int id_set)
{

	struct rtpp_set *rtpp_list;
	/*is it a valid set_id?*/

	if(!rtpp_set_list) {
		LM_ERR("no rtpp_set_list\n");
		return 0;
	}

	lock_get(rtpp_set_list->rset_head_lock);
	if(!rtpp_set_list->rset_first) {
		LM_ERR("no rtpp_set_list->rset_first\n");
		lock_release(rtpp_set_list->rset_head_lock);
		return 0;
	}

	for(rtpp_list = rtpp_set_list->rset_first;
			rtpp_list != 0 && rtpp_list->id_set != id_set;
			rtpp_list = rtpp_list->rset_next)
		;
	if(!rtpp_list) {
		LM_ERR(" script error-invalid id_set to be selected\n");
	}
	lock_release(rtpp_set_list->rset_head_lock);

	return rtpp_list;
}

/*
 * run the selection algorithm and return the new selected node
 */
static struct rtpp_node *select_rtpp_node_new(str callid, str viabranch,
		int do_test, struct rtpp_node **queried_nodes_list, int queried_nodes)
{
	struct rtpp_node *node;
	unsigned i, sum, sumcut, weight_sum;
	int was_forced = 0;

	str hash_data;

	switch(hash_algo) {
		case RTP_HASH_CALLID:
			hash_data = callid;

			break;
		case RTP_HASH_SHA1_CALLID:
			if(rtpengine_cb.SHA1 == NULL) {
				/* don't throw warning here; there is already a warni*/
				LM_BUG("SHA1 algo set but crypto not loaded! Program shouldn't "
					   "have started!");
				return NULL;
			}

			if(rtpengine_cb.SHA1(&callid, &hash_data) < 0) {
				LM_ERR("SHA1 hash in crypto module failed!\n");
				return NULL;
			}

			break;
		case RTP_HASH_CRC32_CALLID:
			crc32_uint(&callid, &sum);
			goto retry;
		default:
			LM_ERR("unknown hashing algo %d\n", hash_algo);
			return NULL;
	}

	/* XXX Use quick-and-dirty hashing algo */
	sum = 0;
	for(i = 0; i < hash_data.len; i++)
		sum += hash_data.s[i];

	/* FIXME this seems to affect the algorithm in a negative way
	 * legacy code uses it; disable it for other algos */
	if(hash_algo == RTP_HASH_CALLID) {
		sum &= 0xff;
	}

retry:
	LM_DBG("sum is = %u\n", sum);
	weight_sum = 0;

	lock_get(active_rtpp_set->rset_lock);
	for(node = active_rtpp_set->rn_first; node != NULL; node = node->rn_next) {
		/* Select only between displayed machines */
		if(!node->rn_displayed) {
			continue;
		}

		/* Try to enable if it's time to try. */
		if(node->rn_disabled && node->rn_recheck_ticks <= get_ticks()) {
			node->rn_disabled = rtpp_test(node, 1, 0);
		}

		/* Select only between enabled machines */
		if(!node->rn_disabled
				&& !is_queried_node(node, queried_nodes_list, queried_nodes)) {
			weight_sum += node->rn_weight;
		}
	}
	lock_release(active_rtpp_set->rset_lock);

	/* No proxies? Force all to be redetected, if not yet */
	if(weight_sum == 0) {
		if(!cfg_get(rtpengine, rtpengine_cfg, aggressive_redetection)) {
			return NULL;
		}

		if(was_forced) {
			return NULL;
		}

		was_forced = 1;

		lock_get(active_rtpp_set->rset_lock);
		for(node = active_rtpp_set->rn_first; node != NULL;
				node = node->rn_next) {
			/* Select only between displayed machines */
			if(!node->rn_displayed) {
				continue;
			}

			node->rn_disabled = rtpp_test(node, 1, 1);
		}
		lock_release(active_rtpp_set->rset_lock);

		goto retry;
	}

	/* sumcut here lays from 0 to weight_sum-1 */
	sumcut = sum % weight_sum;

	/*
	 * Scan proxy list and decrease until appropriate proxy is found.
	 */
	lock_get(active_rtpp_set->rset_lock);
	for(node = active_rtpp_set->rn_first; node != NULL; node = node->rn_next) {
		/* Select only between displayed machines */
		if(!node->rn_displayed) {
			continue;
		}

		/* Select only between enabled machines */
		if(node->rn_disabled)
			continue;

		/* Select only between not already queried machines */
		if(is_queried_node(node, queried_nodes_list, queried_nodes))
			continue;

		/* Found machine */
		if(sumcut < node->rn_weight) {
			lock_release(active_rtpp_set->rset_lock);
			goto found;
		}

		/* Update sumcut if enabled machine */
		sumcut -= node->rn_weight;
	}
	lock_release(active_rtpp_set->rset_lock);

	/* No node list */
	return NULL;

found:
	if(do_test) {
		lock_get(active_rtpp_set->rset_lock);
		node->rn_disabled = rtpp_test(node, node->rn_disabled, 0);
		if(node->rn_disabled) {
			lock_release(active_rtpp_set->rset_lock);
			goto retry;
		}
		lock_release(active_rtpp_set->rset_lock);
	}

	/* return selected node */
	return node;
}

/*
 * lookup the hastable (key=callid value=node) and get the old node (e.g. for answer/delete)
 */
static struct rtpp_node *select_rtpp_node_old(
		str callid, str viabranch, int do_test, enum rtpe_operation op)
{
	struct rtpp_node *node = NULL;

	node = rtpengine_hash_table_lookup(callid, viabranch, op);

	if(!node) {
		LM_DBG("rtpengine hash table lookup failed to find node for calllen=%d "
			   "callid=%.*s viabranch=%.*s\n",
				callid.len, callid.len, callid.s, viabranch.len, viabranch.s);
		return NULL;
	} else {
		LM_DBG("rtpengine hash table lookup find node=%.*s for calllen=%d "
			   "callid=%.*s viabranch=%.*s\n",
				node->rn_url.len, node->rn_url.s, callid.len, callid.len,
				callid.s, viabranch.len, viabranch.s);
	}

	return node;
}

unsigned int node_in_set(struct rtpp_node *node, struct rtpp_set *set)
{
	struct rtpp_node *current = set->rn_first;
	while(current) {
		if(current->idx == node->idx)
			return 1;
		current = current->rn_next;
	}
	return 0;
}

/*
 * Main balancing routine. This DO try to keep the same proxy for
 * the call if some proxies were disabled or enabled (e.g. kamctl command)
 */
static struct rtpp_node *select_rtpp_node(str callid, str viabranch,
		int do_test, struct rtpp_node **queried_nodes_list, int queried_nodes,
		enum rtpe_operation op)
{
	struct rtpp_node *node = NULL;

	if(build_rtpp_socks(1, 0)) {
		LM_ERR("out of memory\n");
		return NULL;
	}

	if(!active_rtpp_set) {
		default_rtpp_set = select_rtpp_set(setid_default);
		active_rtpp_set = default_rtpp_set;
	}

	if(!active_rtpp_set) {
		LM_ERR("script error - no valid set selected\n");
		return NULL;
	}

	// lookup node
	node = select_rtpp_node_old(callid, viabranch, do_test, op);

	if(node && is_queried_node(node, queried_nodes_list, queried_nodes)) {
		LM_ERR("rtpengine node for callid=%.*s is known (%.*s) but it has "
			   "already been queried, therefore not returning it\n",
				callid.len, callid.s, node->rn_url.len, node->rn_url.s);
		return NULL;
	}

	// check node
	if(!node || (node_in_set(node, active_rtpp_set) == 0)) {
		// run the selection algorithm
		node = select_rtpp_node_new(
				callid, viabranch, do_test, queried_nodes_list, queried_nodes);

		// check node
		if(!node) {
			LM_ERR("rtpengine failed to select new for calllen=%d "
				   "callid=%.*s\n",
					callid.len, callid.len, callid.s);
			return NULL;
		}
	}

	// if node enabled, return it
	if(!node->rn_disabled) {
		return node;
	}

	// if proper configuration and node manually or timeout disabled, return it
	if(rtpengine_allow_op) {
		if(node->rn_recheck_ticks == RTPENGINE_MAX_RECHECK_TICKS) {
			LM_DBG("node=%.*s for calllen=%d callid=%.*s is "
				   "disabled(permanent) (probably still UP)! Return it\n",
					node->rn_url.len, node->rn_url.s, callid.len, callid.len,
					callid.s);
			return node;
		} else {
			LM_DBG("node=%.*s for calllen=%d callid=%.*s is disabled, either "
				   "broke or timeout disabled!\n",
					node->rn_url.len, node->rn_url.s, callid.len, callid.len,
					callid.s);
			if(rtpengine_allow_op == 1) {
				LM_DBG("Return it\n");
				return node;
			}
		}
	}

	return NULL;
}

static int get_extra_id(struct sip_msg *msg, str *id_str)
{
	if(msg == NULL || extra_id_pv == NULL || id_str == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(pv_printf_s(msg, extra_id_pv, id_str) < 0) {
		LM_ERR("cannot print the additional id\n");
		return -1;
	}

	return 1;
}

static int set_rtpengine_set_from_avp(struct sip_msg *msg, int direction)
{
	struct usr_avp *avp;
	avp_value_t setid_val;

	if((setid_avp_param == NULL)
			|| (avp = search_first_avp(
						setid_avp_type, setid_avp, &setid_val, 0))
					   == NULL) {
		if(direction == 1 || !selected_rtpp_set_2)
			active_rtpp_set = selected_rtpp_set_1;
		else
			active_rtpp_set = selected_rtpp_set_2;
		return 1;
	}

	if(avp->flags & AVP_VAL_STR) {
		LM_ERR("setid_avp must hold an integer value\n");
		return -1;
	}

	active_rtpp_set = select_rtpp_set(setid_val.n);
	if(active_rtpp_set == NULL) {
		LM_ERR("could not locate engine set %ld\n", setid_val.n);
		return -1;
	}

	LM_DBG("using rtpengine set %ld\n", setid_val.n);

	current_msg_id = msg->id;

	return 1;
}

static void avp_print_s(pv_elem_t *pv, char *str, int len, struct sip_msg *msg)
{
	pv_value_t val;

	if(!pv)
		return;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs.s = str;
	val.rs.len = len;
	pv->spec->setf(msg, &pv->spec->pvp, EQ_T, &val);
}

static void avp_print_decimal(pv_elem_t *pv, int num, struct sip_msg *msg)
{
	int len;
	char buf[8];

	len = snprintf(buf, sizeof(buf), "%i.%i", num / 10, abs(num % 10));
	avp_print_s(pv, buf, len, msg);
}
static void avp_print_llint(
		pv_elem_t *pv, long long int num, struct sip_msg *msg)
{
	int len;
	char buf[20];

	len = snprintf(buf, sizeof(buf), "%lld", num);
	avp_print_s(pv, buf, len, msg);
}
static void avp_print_time(pv_elem_t *pv, int num, struct sip_msg *msg)
{
	int len;
	char buf[8];

	len = snprintf(buf, sizeof(buf), "%i:%02i", num / 60, abs(num % 60));
	avp_print_s(pv, buf, len, msg);
}

static void avp_print_mos(struct minmax_mos_stats *s,
		struct minmax_stats_vals *vals, long long created, struct sip_msg *msg)
{
	if(!vals->avg_samples)
		return;

	avp_print_decimal(s->mos_pv, vals->mos / vals->avg_samples, msg);
	avp_print_time(s->at_pv, vals->at - created, msg);
	avp_print_llint(
			s->packetloss_pv, vals->packetloss / vals->avg_samples, msg);
	avp_print_llint(s->jitter_pv, vals->jitter / vals->avg_samples, msg);
	avp_print_llint(s->roundtrip_pv, vals->roundtrip / vals->avg_samples, msg);
	avp_print_llint(
			s->roundtrip_leg_pv, vals->roundtrip_leg / vals->avg_samples, msg);
	avp_print_llint(s->samples_pv, vals->samples / vals->avg_samples, msg);
}

static int decode_mos_vals_dict(
		struct minmax_stats_vals *vals, bencode_item_t *dict, const char *key)
{
	bencode_item_t *mos_ent;

	mos_ent = bencode_dictionary_get_expect(dict, key, BENCODE_DICTIONARY);
	if(!mos_ent)
		return 0;

	vals->mos = bencode_dictionary_get_integer(mos_ent, "MOS", -1);
	vals->at = bencode_dictionary_get_integer(mos_ent, "reported at", -1);
	vals->packetloss =
			bencode_dictionary_get_integer(mos_ent, "packet loss", -1);
	vals->jitter = bencode_dictionary_get_integer(mos_ent, "jitter", -1);
	vals->roundtrip =
			bencode_dictionary_get_integer(mos_ent, "round-trip time", -1);
	vals->roundtrip_leg =
			bencode_dictionary_get_integer(mos_ent, "round-trip time leg", -1);
	vals->samples = bencode_dictionary_get_integer(mos_ent, "samples", -1);
	vals->avg_samples = 1;

	return 1;
}

static void parse_call_stats_1(struct minmax_mos_label_stats *mmls,
		bencode_item_t *dict, struct sip_msg *msg)
{
	long long created;
	str label, check;
	long long ssrcs[4];
	unsigned int num_ssrcs = 0, i;
	long long ssrc;
	char *endp;
	bencode_item_t *ssrc_list, *ssrc_key, *ssrc_dict, *tags, *tag_key,
			*tag_dict, *medias, *media, *streams, *stream, *ingress_ssrcs,
			*ingress_ssrc;
	struct minmax_stats_vals min_vals = {.mos = 100}, max_vals = {.mos = -1},
							 average_vals = {.avg_samples = 0}, vals_decoded;

	if(!mmls->got_any_pvs)
		return;

	/* check if only a subset of info is requested */
	if(!mmls->label_pv)
		goto ssrcs_done;

	if(pv_printf_s(msg, mmls->label_pv, &label)) {
		LM_ERR("error printing label PV\n");
		return;
	}
	LM_DBG("rtpengine: looking for label '%.*s'\n", label.len, label.s);

	/* walk through tags to find the label we're looking for */
	tags = bencode_dictionary_get_expect(dict, "tags", BENCODE_DICTIONARY);
	if(!tags)
		return; /* label wanted but no tags found - return nothing */
	LM_DBG("rtpengine: XXX got tags\n");

	for(tag_key = tags->child; tag_key; tag_key = tag_key->sibling->sibling) {
		LM_DBG("rtpengine: XXX got tag\n");
		tag_dict = tag_key->sibling;
		/* compare label */
		if(!bencode_dictionary_get_str(tag_dict, "label", &check))
			continue;
		LM_DBG("rtpengine: XXX got label %.*s\n", check.len, check.s);
		if(str_strcmp(&check, &label))
			continue;
		LM_DBG("rtpengine: XXX label match\n");
		medias =
				bencode_dictionary_get_expect(tag_dict, "medias", BENCODE_LIST);
		if(!medias)
			continue;
		LM_DBG("rtpengine: XXX got medias\n");
		for(media = medias->child; media; media = media->sibling) {
			LM_DBG("rtpengine: XXX got media\n");
			streams = bencode_dictionary_get_expect(
					media, "streams", BENCODE_LIST);
			if(!streams)
				continue;
			LM_DBG("rtpengine: XXX got streams\n");
			/* only check the first stream (RTP) */
			stream = streams->child;
			if(!stream)
				continue;
			LM_DBG("rtpengine: XXX got stream type %i\n", stream->type);
			LM_DBG("rtpengine: XXX stream child '%.*s'\n",
					(int)stream->child->iov[1].iov_len,
					(char *)stream->child->iov[1].iov_base);
			LM_DBG("rtpengine: XXX stream child val type %i\n",
					stream->child->sibling->type);
			ssrc = bencode_dictionary_get_integer(stream, "SSRC", -1);
			if(ssrc == -1) {
				ingress_ssrcs = bencode_dictionary_get_expect(
						stream, "ingress SSRCs", BENCODE_LIST);
				if(!ingress_ssrcs || !ingress_ssrcs->child)
					continue;
				LM_DBG("rtpengine: XXX got ingress SSRCs\n");
				ingress_ssrc = ingress_ssrcs->child;
				if((ssrc = bencode_dictionary_get_integer(
							ingress_ssrc, "SSRC", -1))
						== -1) {
					continue;
				}
			}

			/* got a valid SSRC to watch for */
			ssrcs[num_ssrcs] = ssrc;
			LM_DBG("rtpengine: found SSRC '%lli' for label '%.*s'\n", ssrc,
					label.len, label.s);
			num_ssrcs++;
			/* see if we can do more */
			if(num_ssrcs >= (sizeof(ssrcs) / sizeof(*ssrcs)))
				goto ssrcs_done;
		}
	}
	/* if we get here, we were looking for label. see if we found one. if not, return nothing */
	if(num_ssrcs == 0)
		return;

ssrcs_done:
	/* now look for the stats values */
	created = bencode_dictionary_get_integer(dict, "created", 0);
	ssrc_list = bencode_dictionary_get_expect(dict, "SSRC", BENCODE_DICTIONARY);
	if(!ssrc_list)
		return;

	for(ssrc_key = ssrc_list->child; ssrc_key;
			ssrc_key = ssrc_key->sibling->sibling) {
		/* see if this is a SSRC we're interested in */
		if(num_ssrcs == 0)
			goto ssrc_ok;
		if(!bencode_get_str(ssrc_key, &check))
			continue;
		ssrc = strtoll(check.s, &endp, 10);
		for(i = 0; i < num_ssrcs; i++) {
			if(ssrcs[i] != ssrc)
				continue;
			/* it's a match */
			LM_DBG("rtpengine: considering SSRC '%.*s'\n", check.len, check.s);
			goto ssrc_ok;
		}
		/* no match */
		continue;

	ssrc_ok:
		ssrc_dict = ssrc_key->sibling;
		if(!ssrc_dict)
			continue;

		if(decode_mos_vals_dict(&vals_decoded, ssrc_dict, "average MOS")) {
			if(vals_decoded.mos > 0) {
				average_vals.avg_samples++;
				average_vals.mos += vals_decoded.mos;
				average_vals.packetloss += vals_decoded.packetloss;
				average_vals.jitter += vals_decoded.jitter;
				average_vals.roundtrip += vals_decoded.roundtrip;
				average_vals.roundtrip_leg += vals_decoded.roundtrip_leg;
				average_vals.samples += vals_decoded.samples;
			}
		}

		if(decode_mos_vals_dict(&vals_decoded, ssrc_dict, "highest MOS")) {
			if(vals_decoded.mos > max_vals.mos)
				max_vals = vals_decoded;
		}
		if(decode_mos_vals_dict(&vals_decoded, ssrc_dict, "lowest MOS")) {
			if(vals_decoded.mos > 0 && vals_decoded.mos < min_vals.mos)
				min_vals = vals_decoded;
		}
	}

	avp_print_mos(&mmls->max, &max_vals, created, msg);
	avp_print_mos(&mmls->min, &min_vals, created, msg);
	avp_print_mos(&mmls->average, &average_vals, created, msg);
}

static void parse_call_stats(bencode_item_t *dict, struct sip_msg *msg)
{
	if(!got_any_mos_pvs)
		return;

	parse_call_stats_1(&global_mos_stats, dict, msg);
	parse_call_stats_1(&side_A_mos_stats, dict, msg);
	parse_call_stats_1(&side_B_mos_stats, dict, msg);
}

static int rtpengine_delete(struct sip_msg *msg, void *d)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	bencode_item_t *ret = rtpp_function_call_ok(
			&bencbuf, msg, OP_DELETE, flags, viabranch, NULL, NULL, NULL);
	if(!ret)
		return -1;
	parse_call_stats(ret, msg);
	bencode_buffer_free(&bencbuf);
	return 1;
}

static int rtpengine_query(struct sip_msg *msg, void *d)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	bencode_item_t *ret = rtpp_function_call_ok(
			&bencbuf, msg, OP_QUERY, flags, viabranch, NULL, NULL, NULL);
	if(!ret)
		return -1;
	parse_call_stats(ret, msg);
	bencode_buffer_free(&bencbuf);
	return 1;
}

static int rtpengine_rtpp_set_wrap(struct sip_msg *msg,
		int (*func)(struct sip_msg *msg, void *, int, enum rtpe_operation),
		void *data, int direction, enum rtpe_operation op)
{
	int ret, more;

	body_intermediate.s = NULL;

	if(set_rtpengine_set_from_avp(msg, direction) == -1)
		return -1;

	more = 1;
	if(!selected_rtpp_set_2 || selected_rtpp_set_2 == selected_rtpp_set_1)
		more = 0;

	ret = func(msg, data, more, op);
	if(ret < 0)
		return ret;

	if(!more)
		return ret;

	direction = (direction == 1) ? 2 : 1;
	if(set_rtpengine_set_from_avp(msg, direction) == -1)
		return -1;

	ret = func(msg, data, 0, op);
	body_intermediate.s = NULL;
	return ret;
}

static int rtpengine_delete_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	return rtpengine_delete(msg, d);
}

static int rtpengine_rtpp_set_wrap_fparam(struct sip_msg *msg,
		int (*func)(struct sip_msg *msg, void *, int, enum rtpe_operation),
		char *str1, char *str2, int direction, enum rtpe_operation op)
{
	str flags;
	str viabranch;

	void *parms[2];

	parms[0] = NULL;
	parms[1] = NULL;

	flags.s = NULL;
	if(str1) {
		if(get_str_fparam(&flags, msg, (fparam_t *)str1)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[0] = &flags;
	}

	viabranch.s = NULL;
	if(str2) {
		if(get_str_fparam(&viabranch, msg, (fparam_t *)str2)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[1] = &viabranch;
	}

	return rtpengine_rtpp_set_wrap(msg, func, parms, direction, op);
}

static int rtpengine_delete1_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_delete_wrap, str1, str2, 1, OP_DELETE);
}

static int rtpengine_query_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	return rtpengine_query(msg, d);
}

static int rtpengine_query1_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_query_wrap, str1, str2, 1, OP_QUERY);
}


/* This function assumes p points to a line of requested type. */

static int set_rtpengine_set_n(
		struct sip_msg *msg, rtpp_set_link_t *rtpl, struct rtpp_set **out)
{
	pv_value_t val;
	struct rtpp_node *node;
	int nb_active_nodes = 0;

	if(rtpl->rset != NULL) {
		current_msg_id = msg->id;
		*out = rtpl->rset;
		return 1;
	}

	if(pv_get_spec_value(msg, rtpl->rpv, &val) < 0) {
		LM_ERR("cannot evaluate pv param\n");
		return -1;
	}
	if(!(val.flags & PV_VAL_INT)) {
		LM_ERR("pv param must hold an integer value\n");
		return -1;
	}
	*out = select_rtpp_set(val.ri);
	if(*out == NULL) {
		LM_ERR("could not locate rtpengine set %ld\n", val.ri);
		return -1;
	}
	current_msg_id = msg->id;

	lock_get((*out)->rset_lock);
	node = (*out)->rn_first;
	while(node != NULL) {
		if(node->rn_disabled == 0)
			nb_active_nodes++;
		node = node->rn_next;
	}
	lock_release((*out)->rset_lock);

	if(nb_active_nodes > 0) {
		LM_DBG("rtpp: selected proxy set ID %d with %d active nodes.\n",
				(*out)->id_set, nb_active_nodes);
		return nb_active_nodes;
	} else {
		LM_WARN("rtpp: selected proxy set ID %d but it has no active node.\n",
				(*out)->id_set);
		return -2;
	}
}

static int set_rtpengine_set_f(struct sip_msg *msg, char *str1, char *str2)
{
	rtpp_set_link_t *rtpl1, *rtpl2;
	int ret;

	rtpl1 = (rtpp_set_link_t *)str1;
	rtpl2 = (rtpp_set_link_t *)str2;

	current_msg_id = 0;
	active_rtpp_set = 0;
	selected_rtpp_set_1 = 0;
	selected_rtpp_set_2 = 0;

	ret = set_rtpengine_set_n(msg, rtpl1, &selected_rtpp_set_1);
	if(ret < 0)
		return ret;

	if(rtpl2) {
		ret = set_rtpengine_set_n(msg, rtpl2, &selected_rtpp_set_2);
		if(ret < 0)
			return ret;
	}

	return 1;
}

static int rtpengine_manage(struct sip_msg *msg, void *d)
{
	int method;
	int nosdp;
	tm_cell_t *t = NULL;

	if(route_type == BRANCH_FAILURE_ROUTE) {
		/* do nothing in branch failure event route
		 * - delete done on transaction failure route */
		return 1;
	}

	if(msg->cseq == NULL
			&& ((parse_headers(msg, HDR_CSEQ_F, 0) == -1)
					|| (msg->cseq == NULL))) {
		LM_ERR("no CSEQ header\n");
		return -1;
	}

	method = get_cseq(msg)->method_id;

	if(!(method
			   & (METHOD_INVITE | METHOD_ACK | METHOD_CANCEL | METHOD_BYE
					   | METHOD_UPDATE | METHOD_PRACK)))
		return -1;

	if(method & (METHOD_CANCEL | METHOD_BYE))
		return rtpengine_delete(msg, d);

	if(msg->msg_flags & FL_SDP_BODY)
		nosdp = 0;
	else
		nosdp = parse_sdp(msg);

	if(msg->first_line.type == SIP_REQUEST) {
		if((method & (METHOD_ACK | METHOD_PRACK)) && nosdp == 0)
			return rtpengine_offer_answer(msg, d, OP_ANSWER, 0);
		if(method == METHOD_UPDATE && nosdp == 0)
			return rtpengine_offer_answer(msg, d, OP_OFFER, 0);
		if(method == METHOD_INVITE && nosdp == 0) {
			msg->msg_flags |= FL_SDP_BODY;
			if(tmb.t_gett != NULL) {
				t = tmb.t_gett();
				if(t != NULL && t != T_UNDEFINED && t->uas.request != NULL) {
					t->uas.request->msg_flags |= FL_SDP_BODY;
				}
			}
			if(route_type == FAILURE_ROUTE)
				return rtpengine_delete(msg, d);
			return rtpengine_offer_answer(msg, d, OP_OFFER, 0);
		}
	} else if(msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode >= 300)
			return rtpengine_delete(msg, d);
		if(nosdp == 0) {
			if(method == METHOD_UPDATE)
				return rtpengine_offer_answer(msg, d, OP_ANSWER, 0);
			if(tmb.t_gett == NULL || tmb.t_gett() == NULL
					|| tmb.t_gett() == T_UNDEFINED)
				return rtpengine_offer_answer(msg, d, OP_ANSWER, 0);
			if(tmb.t_gett()->uas.request->msg_flags & FL_SDP_BODY)
				return rtpengine_offer_answer(msg, d, OP_ANSWER, 0);
			return rtpengine_offer_answer(msg, d, OP_OFFER, 0);
		}
	}
	return -1;
}

static int rtpengine_manage_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	return rtpengine_manage(msg, d);
}

static int rtpengine_manage1_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_manage_wrap, str1, str2, 1, OP_ANY);
}

static int rtpengine_info1_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_simple_wrap, str1, str2, 1, OP_OFFER);
}

static int rtpengine_offer_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	return rtpengine_offer_answer(msg, d, OP_OFFER, more);
}

static int rtpengine_offer1_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_offer_wrap, str1, str2, 1, OP_OFFER);
}

static int rtpengine_answer_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	return rtpengine_offer_answer(msg, d, OP_ANSWER, more);
}

static int rtpengine_answer1_f(struct sip_msg *msg, char *str1, char *str2)
{
	if(msg->first_line.type == SIP_REQUEST)
		if(!(msg->first_line.u.request.method_value
				   & (METHOD_ACK | METHOD_PRACK)))
			return -1;

	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_answer_wrap, str1, str2, 2, OP_ANSWER);
}

static int rtpengine_subscribe_request_wrap_f(struct sip_msg *msg, char *str1,
		char *str2, char *str3, char *str4, char *str5)
{
	str flags;
	str viabranch;

	void *parms[5];

	parms[0] = NULL;
	parms[1] = NULL;
	parms[2] = NULL;
	parms[3] = NULL;
	parms[4] = NULL;

	flags.s = NULL;
	if(str1) {
		if(get_str_fparam(&flags, msg, (fparam_t *)str1)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[0] = &flags;
	}

	if(str2) {
		// get the SDP AVP
		parms[1] = str2;
	}
	if(str3) {
		// get the to-tag AVP
		parms[2] = str3;
	}
	if(str4) {
		// get the stream XAVP
		parms[3] = str4;
	}
	if(str5) {
		if(get_str_fparam(&viabranch, msg, (fparam_t *)str5)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[4] = &viabranch;
	}

	return rtpengine_rtpp_set_wrap(msg, rtpengine_subscribe_request_wrap, parms,
			1, OP_SUBSCRIBE_REQUEST);
}

static int ki_subscribe_request(sip_msg_t *msg, str *flags, str *sdp_spv,
		str *to_tag_spv, str *stream_xavp)
{
	void *parms[5] = {flags, sdp_spv, to_tag_spv, stream_xavp, NULL};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_subscribe_request_wrap, parms,
			1, OP_SUBSCRIBE_REQUEST);
}

static int rtpengine_subscribe_request_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	void **parms;
	str *flags = NULL;
	str *sdp_spv = NULL;
	str *to_tag_spv = NULL;
	str *stream_xavp = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;
	str ret_body, to_tag = {"", 0};

	pv_spec_t *pvs = NULL;
	pv_value_t dst_val;
	memset(&dst_val, 0, sizeof(pv_value_t));
	dst_val.flags |= PV_VAL_STR;

	parms = d;
	flags = parms[0];
	sdp_spv = parms[1];
	to_tag_spv = parms[2];
	stream_xavp = parms[3];
	viabranch = parms[4];

	dict = rtpp_function_call_ok(
			&bencbuf, msg, op, flags, viabranch, NULL, NULL, NULL);
	if(!dict)
		return -1;

	// Extract SDP body from the RTPEngine response
	if(!bencode_dictionary_get_str_dup(dict, "sdp", &ret_body)) {
		LM_ERR("failed to extract sdp body from proxy reply\n");
		return -1;
	}

	if(sdp_spv != NULL && sdp_spv->s != NULL && sdp_spv->len > 0) {
		pvs = pv_cache_get(sdp_spv);
		dst_val.rs.s = ret_body.s;
		dst_val.rs.len = ret_body.len;
		if(pv_set_spec_value(msg, pvs, 0, &dst_val) != 0) {
			LM_ERR("failed to set sdp AVP\n");
			return -1;
		}
		LM_DBG("Set sdp %.*s AVP to %.*s\n", sdp_spv->len, sdp_spv->s,
				ret_body.len, ret_body.s);
	}

	// Extract and allocate a new to-tag from the RTPEngine response
	if(!bencode_dictionary_get_str(dict, "to-tag", &to_tag)) {
		LM_ERR("failed to extract to-tag from proxy reply\n");
		return -1;
	}
	if(to_tag_spv != NULL && to_tag_spv->s != NULL && to_tag_spv->len > 0) {
		pvs = pv_cache_get(to_tag_spv);
		dst_val.rs.s = to_tag.s;
		dst_val.rs.len = to_tag.len;
		if(pv_set_spec_value(msg, pvs, 0, &dst_val) != 0) {
			LM_ERR("failed to set to_tag AVP\n");
			return -1;
		}
		LM_DBG("Set to_tag %.*s AVP to %.*s\n", to_tag_spv->len, to_tag_spv->s,
				to_tag.len, to_tag.s);
	}

	if(stream_xavp != NULL) {
		rtpengine_copy_streams_to_xavp(
				stream_xavp, bencode_dictionary_get(dict, "tag-medias"));
	}

	// Free the bencode buffer
	bencode_buffer_free(bencode_item_buffer(dict));
	return 0;
}


static int rtpengine_subscribe_answer_wrap_f(
		struct sip_msg *msg, char *str1, char *str2)
{
	str flags;
	str viabranch;

	void *parms[2];

	parms[0] = NULL;
	parms[1] = NULL;

	flags.s = NULL;
	if(str1) {
		if(get_str_fparam(&flags, msg, (fparam_t *)str1)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[0] = &flags;
	}
	if(str2) {
		if(get_str_fparam(&viabranch, msg, (fparam_t *)str2)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[1] = &viabranch;
	}

	return rtpengine_rtpp_set_wrap(msg, rtpengine_subscribe_answer_wrap, parms,
			1, OP_SUBSCRIBE_ANSWER);
}

static int ki_subscribe_answer(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_subscribe_answer_wrap, parms,
			1, OP_SUBSCRIBE_ANSWER);
}

static int rtpengine_subscribe_answer_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	dict = rtpp_function_call_ok(
			&bencbuf, msg, op, flags, viabranch, NULL, NULL, NULL);
	if(!dict)
		return -1;
	// Free the bencode buffer
	bencode_buffer_free(bencode_item_buffer(dict));
	return 0;
}

static int rtpengine_unsubscribe_wrap_f(
		struct sip_msg *msg, char *str1, char *str2)
{
	str flags;
	str viabranch;

	void *parms[2];

	parms[0] = NULL;
	parms[1] = NULL;

	flags.s = NULL;
	if(str1) {
		if(get_str_fparam(&flags, msg, (fparam_t *)str1)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[0] = &flags;
	}
	if(str2) {
		if(get_str_fparam(&viabranch, msg, (fparam_t *)str2)) {
			LM_ERR("Error getting string parameter\n");
			return -1;
		}
		parms[1] = &viabranch;
	}

	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_unsubscribe_wrap, parms, 1, OP_UNSUBSCRIBE);
}

static int ki_unsubscribe(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_unsubscribe_wrap, parms, 1, OP_UNSUBSCRIBE);
}


static int rtpengine_unsubscribe_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	dict = rtpp_function_call_ok(
			&bencbuf, msg, op, flags, viabranch, NULL, NULL, NULL);
	if(!dict)
		return -1;
	// Free the bencode buffer
	bencode_buffer_free(bencode_item_buffer(dict));
	return 0;
}

static int rtpengine_offer_answer(
		struct sip_msg *msg, void *d, enum rtpe_operation op, int more)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;
	str body, newbody;
	struct lump *anchor;
	pv_value_t pv_val;
	str cur_body = STR_NULL;
	str cl_field = STR_NULL;
	str cl_repl = STR_NULL;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	dict = rtpp_function_call_ok(
			&bencbuf, msg, op, flags, viabranch, &body, &cl_field, NULL);
	if(!dict)
		return -1;

	if(!bencode_dictionary_get_str_dup(dict, "sdp", &newbody)) {
		LM_ERR("failed to extract sdp body from proxy reply\n");
		goto error;
	}

	if(body_intermediate.s)
		pkg_free(body_intermediate.s);

	if(more)
		body_intermediate = newbody;
	else {
		if(write_sdp_pvar != NULL) {
			pv_val.rs = newbody;
			pv_val.flags = PV_VAL_STR;

			if(write_sdp_pvar->setf(
					   msg, &write_sdp_pvar->pvp, (int)EQ_T, &pv_val)
					< 0) {
				LM_ERR("error setting pvar <%.*s>\n", write_sdp_pvar_str.len,
						write_sdp_pvar_str.s);
				goto error_free;
			}

			if(write_sdp_pvar_mode == 0) {
				pkg_free(newbody.s);
			}
		}
		if(write_sdp_pvar == NULL || write_sdp_pvar_mode != 0) {
			if(read_sdp_pvar_str.len > 0) {
				/* get the body from the message as body ptr may have changed
				 * when using read_sdp_pv */
				if(extract_body(msg, &cur_body, &cl_field) == -1) {
					LM_ERR("failed to extract body from message");
					goto error_free;
				}
				anchor = del_lump(msg, cur_body.s - msg->buf, cur_body.len, 0);
			} else {
				anchor = del_lump(msg, body.s - msg->buf, body.len, 0);
			}
			if(!anchor) {
				LM_ERR("del_lump failed\n");
				goto error_free;
			}
			if(!insert_new_lump_after(anchor, newbody.s, newbody.len, 0)) {
				LM_ERR("insert_new_lump_after failed\n");
				goto error_free;
			}

			if(cl_field.len) {
				anchor = del_lump(msg, cl_field.s - msg->buf, cl_field.len, 0);
				cl_repl.s = pkg_malloc(10);
				if(!cl_repl.s) {
					LM_ERR("pkg_malloc for Content-Length failed\n");
					goto error_free;
				}
				cl_repl.len = snprintf(cl_repl.s, 10, "%i", (int)newbody.len);
				if(!insert_new_lump_after(anchor, cl_repl.s, cl_repl.len, 0)) {
					LM_ERR("insert_new_lump_after failed\n");
					goto error_free;
				}
				cl_repl.s = NULL;
			}
		}
	}

	bencode_buffer_free(&bencbuf);
	return 1;

error_free:
	pkg_free(newbody.s);
	if(cl_repl.s)
		pkg_free(cl_repl.s);
error:
	bencode_buffer_free(&bencbuf);
	return -1;
}


static int rtpengine_generic_f(
		struct sip_msg *msg, char *str1, enum rtpe_operation op)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_simple_wrap, str1, NULL, 1, op);
}

static int start_recording_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_START_RECORDING);
}

static int stop_recording_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_STOP_RECORDING);
}

static int block_dtmf_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_BLOCK_DTMF);
}

static int unblock_dtmf_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_UNBLOCK_DTMF);
}

static int block_media_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_BLOCK_MEDIA);
}

static int unblock_media_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_UNBLOCK_MEDIA);
}

static int silence_media_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_SILENCE_MEDIA);
}

static int unsilence_media_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_UNSILENCE_MEDIA);
}

static int rtpengine_play_media(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	void **parms;
	str *flags = NULL;
	str *viabranch = NULL;
	bencode_buffer_t bencbuf;
	long long duration;
	bencode_item_t *ret;
	char intbuf[32];
	pv_value_t val;
	int retval = 1;

	parms = d;
	flags = parms[0];
	viabranch = parms[1];

	ret = rtpp_function_call_ok(
			&bencbuf, msg, OP_PLAY_MEDIA, flags, viabranch, NULL, NULL, NULL);
	if(!ret)
		return -1;
	if(media_duration_pvar) {
		duration = bencode_dictionary_get_integer(ret, "duration", -1);
		snprintf(intbuf, sizeof(intbuf), "%lli", duration);
		memset(&val, 0, sizeof(val));
		val.flags = PV_VAL_STR;
		val.rs.s = intbuf;
		val.rs.len = strlen(intbuf);
		if(media_duration_pvar->setf(
				   msg, &media_duration_pvar->pvp, (int)EQ_T, &val)
				< 0) {
			LM_ERR("error setting pvar <%.*s>\n", media_duration_pvar_str.len,
					media_duration_pvar_str.s);
			retval = -1;
		}
	}

	bencode_buffer_free(&bencbuf);
	return retval;
}

static int play_media_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_rtpp_set_wrap_fparam(
			msg, rtpengine_play_media, str1, str2, 1, OP_PLAY_MEDIA);
}

static int stop_media_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_STOP_MEDIA);
}

static int play_dtmf_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_PLAY_DTMF);
}

static int start_forwarding_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_START_FORWARDING);
}

static int stop_forwarding_f(struct sip_msg *msg, char *str1, char *str2)
{
	return rtpengine_generic_f(msg, str1, OP_STOP_FORWARDING);
}

static int rtpengine_rtpstat_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	void **parms;
	pv_param_t *param;
	pv_value_t *res;
	bencode_buffer_t bencbuf;
	bencode_item_t *dict, *tot, *rtp, *rtcp;
	static char buf[256];
	str ret;

	parms = d;
	param = parms[0];
	res = parms[1];

	dict = rtpp_function_call_ok(
			&bencbuf, msg, OP_QUERY, NULL, NULL, NULL, NULL, NULL);
	if(!dict)
		return -1;

	tot = bencode_dictionary_get_expect(dict, "totals", BENCODE_DICTIONARY);
	rtp = bencode_dictionary_get_expect(tot, "RTP", BENCODE_DICTIONARY);
	rtcp = bencode_dictionary_get_expect(tot, "RTCP", BENCODE_DICTIONARY);

	if(!rtp || !rtcp)
		goto error;

	ret.s = buf;
	ret.len = snprintf(buf, sizeof(buf),
			"RTP: %lli bytes, %lli packets, %lli errors; "
			"RTCP: %lli bytes, %lli packets, %lli errors",
			bencode_dictionary_get_integer(rtp, "bytes", -1),
			bencode_dictionary_get_integer(rtp, "packets", -1),
			bencode_dictionary_get_integer(rtp, "errors", -1),
			bencode_dictionary_get_integer(rtcp, "bytes", -1),
			bencode_dictionary_get_integer(rtcp, "packets", -1),
			bencode_dictionary_get_integer(rtcp, "errors", -1));

	bencode_buffer_free(&bencbuf);
	return pv_get_strval(msg, param, res, &ret);

error:
	bencode_buffer_free(&bencbuf);
	return -1;
}

/*
 * Returns the current RTP-Statistics from the RTP-Proxy
 */
static int pv_get_rtpestat_f(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	void *parms[2];

	parms[0] = param;
	parms[1] = res;

	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_rtpstat_wrap, parms, 1, OP_ANY);
}

/**
 *
 */
static srjson_t *rtpengine_query_v_build_json(
		srjson_doc_t *jdoc, bencode_item_t *dict)
{
	srjson_t *vnode;
	srjson_t *tnode;
	bencode_item_t *it;
	str sval;

	if(jdoc == NULL || dict == NULL) {
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	switch(dict->type) {
		case BENCODE_STRING:
			return srjson_CreateStr(
					jdoc, dict->iov[1].iov_base, dict->iov[1].iov_len);

		case BENCODE_INTEGER:
			return srjson_CreateNumber(jdoc, dict->value);

		case BENCODE_LIST:
			vnode = srjson_CreateArray(jdoc);
			if(vnode == NULL) {
				LM_ERR("failed to create the array node\n");
				return NULL;
			}
			for(it = dict->child; it; it = it->sibling) {
				tnode = rtpengine_query_v_build_json(jdoc, it);
				if(!tnode) {
					srjson_Delete(jdoc, vnode);
					return NULL;
				}
				srjson_AddItemToArray(jdoc, vnode, tnode);
			}
			return vnode;

		case BENCODE_DICTIONARY:
			vnode = srjson_CreateObject(jdoc);
			if(vnode == NULL) {
				LM_ERR("failed to create the object node\n");
				return NULL;
			}
			for(it = dict->child; it; it = it->sibling) {
				/* name of the item */
				sval.s = it->iov[1].iov_base;
				sval.len = it->iov[1].iov_len;
				/* value of the item */
				it = it->sibling;
				tnode = rtpengine_query_v_build_json(jdoc, it);
				if(!tnode) {
					srjson_Delete(jdoc, vnode);
					return NULL;
				}
				srjson_AddStrItemToObject(jdoc, vnode, sval.s, sval.len, tnode);
			}
			return vnode;

		default:
			LM_ERR("unsupported bencode item type %d\n", dict->type);
			return NULL;
	}
}

/**
 *
 */
static int rtpengine_query_v_print(bencode_item_t *dict, str *fmt, str *bout)
{
	srjson_doc_t jdoc;

	if(fmt == NULL || fmt->s == NULL || fmt->len <= 0) {
		LM_ERR("invalid format parameter\n");
		return -1;
	}
	if(fmt->s[0] != 'j' && fmt->s[0] != 'J') {
		LM_ERR("invalid format parameter value: %.*s\n", fmt->len, fmt->s);
		return -1;
	}

	srjson_InitDoc(&jdoc, NULL);
	jdoc.root = rtpengine_query_v_build_json(&jdoc, dict);

	if(jdoc.root == NULL) {
		LM_ERR("failed to build json document\n");
		return -1;
	}

	if(fmt->len > 1 && (fmt->s[1] == 'p' || fmt->s[1] == 'P')) {
		bout->s = srjson_Print(&jdoc, jdoc.root);
	} else {
		bout->s = srjson_PrintUnformatted(&jdoc, jdoc.root);
	}
	if(bout->s == NULL) {
		LM_ERR("unable to serialize json document\n");
		srjson_DestroyDoc(&jdoc);
		return -1;
	}
	bout->len = strlen(bout->s);
	srjson_DestroyDoc(&jdoc);

	return 0;
}

/**
 *
 */
static int rtpengine_query_v_wrap(
		struct sip_msg *msg, void *d, int more, enum rtpe_operation op)
{
	void **parms;
	str *fmt = NULL;
	pv_spec_t *dst = NULL;
	pv_value_t val = {0};
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;

	parms = d;
	fmt = parms[0];
	dst = parms[1];

	dict = rtpp_function_call_ok(
			&bencbuf, msg, OP_QUERY, NULL, NULL, NULL, NULL, NULL);
	if(!dict) {
		return -1;
	}
	if(rtpengine_query_v_print(dict, fmt, &val.rs) < 0) {
		goto error;
	}

	val.flags = PV_VAL_STR;
	if(dst->setf) {
		dst->setf(msg, &dst->pvp, (int)EQ_T, &val);
	} else {
		LM_WARN("target pv is not writable\n");
	}

	/* val.rs.s is allocated by srjson print */
	free(val.rs.s);

	bencode_buffer_free(&bencbuf);
	return 1;

error:
	bencode_buffer_free(&bencbuf);
	return -1;
}

/**
 *
 */
static int ki_rtpengine_query_v(sip_msg_t *msg, str *fmt, str *dpv)
{
	void *parms[2];
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	parms[0] = fmt;
	parms[1] = dst;
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_query_v_wrap, parms, 1, OP_ANY);
}

/*
 * Store the cmd QUERY result to variable
 */
static int w_rtpengine_query_v(sip_msg_t *msg, char *pfmt, char *pvar)
{
	void *parms[2];
	str fmt = {NULL, 0};
	pv_spec_t *dst;

	if(fixup_get_svalue(msg, (gparam_t *)pfmt, &fmt) < 0 || fmt.len <= 0) {
		LM_ERR("fmt has no value\n");
		return -1;
	}
	dst = (pv_spec_t *)pvar;

	parms[0] = &fmt;
	parms[1] = dst;

	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_query_v_wrap, parms, 1, OP_ANY);
}

/**
 *
 */
static int fixup_rtpengine_query_v(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if(param_no == 2) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/**
 *
 */
static int fixup_free_rtpengine_query_v(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_rtpengine_subscribe_request_v(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if(param_no >= 2 && param_no <= 4) {
		return fixup_str_null(param, 1);
	}

	if(param_no == 5) {
		return fixup_spve_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/**
 *
 */
static int fixup_free_rtpengine_subscribe_request_v(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no >= 2 && param_no <= 4) {
		return fixup_free_str_null(param, 1);
	}

	if(param_no == 5) {
		return fixup_free_spve_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int set_rtp_inst_pvar(struct sip_msg *msg, const str *const uri)
{
	pv_value_t val;

	if(rtp_inst_pvar == NULL)
		return 0;

	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_STR;
	val.rs = *uri;

	if(rtp_inst_pvar->setf(msg, &rtp_inst_pvar->pvp, (int)EQ_T, &val) < 0) {
		LM_ERR("Failed to add RTP Engine URI to pvar\n");
		return -1;
	}
	return 0;
}

/**
 * KI stuff.
 */

/* KI - rtpengine manage */
static int ki_rtpengine_manage0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_manage_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_manage(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_manage_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_manage2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_manage_wrap, parms, 1, OP_ANY);
}

/* KI - rtpengine offer */
static int ki_rtpengine_offer0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_offer_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_offer(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_offer_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_offer2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_offer_wrap, parms, 1, OP_ANY);
}

/* KI - rtpengine answer */
static int ki_rtpengine_answer0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_answer_wrap, parms, 2, OP_ANY);
}
static int ki_rtpengine_answer(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_answer_wrap, parms, 2, OP_ANY);
}
static int ki_rtpengine_answer2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_answer_wrap, parms, 2, OP_ANY);
}

/* KI - rtpengine delete */
static int ki_rtpengine_delete0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_delete_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_delete(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_delete_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_delete2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_delete_wrap, parms, 1, OP_ANY);
}

/* KI - rtpengine query */
static int ki_rtpengine_query0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_query_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_query(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_query_wrap, parms, 1, OP_ANY);
}
static int ki_rtpengine_query2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(msg, rtpengine_query_wrap, parms, 1, OP_ANY);
}

/* KI - start recording */
static int ki_start_recording(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_START_RECORDING);
}

/* KI - stop recording */
static int ki_stop_recording(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_STOP_RECORDING);
}

/* KI - block media */
static int ki_block_media0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_BLOCK_MEDIA);
}
static int ki_block_media(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_BLOCK_MEDIA);
}
static int ki_block_media2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_BLOCK_MEDIA);
}

/* KI - unblock media */
static int ki_unblock_media0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNBLOCK_MEDIA);
}
static int ki_unblock_media(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNBLOCK_MEDIA);
}
static int ki_unblock_media2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNBLOCK_MEDIA);
}

/* KI - silence media */
static int ki_silence_media0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_SILENCE_MEDIA);
}
static int ki_silence_media(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_SILENCE_MEDIA);
}
static int ki_silence_media2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_SILENCE_MEDIA);
}

/* KI - unsilence media */
static int ki_unsilence_media0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNSILENCE_MEDIA);
}
static int ki_unsilence_media(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNSILENCE_MEDIA);
}
static int ki_unsilence_media2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNSILENCE_MEDIA);
}

/* KI - block dtmf */
static int ki_block_dtmf0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_BLOCK_DTMF);
}
static int ki_block_dtmf(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_BLOCK_DTMF);
}
static int ki_block_dtmf2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_BLOCK_DTMF);
}

/* KI - unblock dtmf */
static int ki_unblock_dtmf0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNBLOCK_DTMF);
}
static int ki_unblock_dtmf(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNBLOCK_DTMF);
}
static int ki_unblock_dtmf2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_UNBLOCK_DTMF);
}

static int ki_play_dtmf(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_PLAY_DTMF);
}

/* KI - play media */
static int ki_play_media(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_PLAY_MEDIA);
}
static int ki_play_media2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_PLAY_MEDIA);
}

/* KI - stop media */
static int ki_stop_media0(sip_msg_t *msg)
{
	void *parms[2] = {NULL, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_STOP_MEDIA);
}
static int ki_stop_media(sip_msg_t *msg, str *flags)
{
	void *parms[2] = {flags, NULL};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_STOP_MEDIA);
}
static int ki_stop_media2(sip_msg_t *msg, str *flags, str *viabranch)
{
	void *parms[2] = {flags, viabranch};
	return rtpengine_rtpp_set_wrap(
			msg, rtpengine_simple_wrap, parms, 1, OP_STOP_MEDIA);
}

static int ki_set_rtpengine_set(sip_msg_t *msg, int r1)
{
	rtpp_set_link_t rtpl1;
	rtpp_set_link_t rtpl2;
	int ret;

	memset(&rtpl1, 0, sizeof(rtpp_set_link_t));
	memset(&rtpl2, 0, sizeof(rtpp_set_link_t));

	if((rtpl1.rset = select_rtpp_set((unsigned int)r1)) == 0) {
		LM_ERR("rtpp_proxy set %d not configured\n", r1);
		return -1;
	}

	current_msg_id = 0;
	active_rtpp_set = 0;
	selected_rtpp_set_1 = 0;
	selected_rtpp_set_2 = 0;

	ret = set_rtpengine_set_n(msg, &rtpl1, &selected_rtpp_set_1);
	if(ret < 0)
		return ret;

	return 1;
}

static int ki_set_rtpengine_set2(sip_msg_t *msg, int r1, int r2)
{
	rtpp_set_link_t rtpl1;
	rtpp_set_link_t rtpl2;
	int ret;

	memset(&rtpl1, 0, sizeof(rtpp_set_link_t));
	memset(&rtpl2, 0, sizeof(rtpp_set_link_t));

	if((rtpl1.rset = select_rtpp_set((unsigned int)r1)) == 0) {
		LM_ERR("rtpp_proxy set %d not configured\n", r1);
		return -1;
	}
	if((rtpl2.rset = select_rtpp_set((unsigned int)r2)) == 0) {
		LM_ERR("rtpp_proxy set %d not configured\n", r2);
		return -1;
	}

	current_msg_id = 0;
	active_rtpp_set = 0;
	selected_rtpp_set_1 = 0;
	selected_rtpp_set_2 = 0;

	ret = set_rtpengine_set_n(msg, &rtpl1, &selected_rtpp_set_1);
	if(ret < 0)
		return ret;

	ret = set_rtpengine_set_n(msg, &rtpl2, &selected_rtpp_set_2);
	if(ret < 0)
		return ret;

	return 1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_rtpengine_exports[] = {
    { str_init("rtpengine"), str_init("rtpengine_manage0"),
        SR_KEMIP_INT, ki_rtpengine_manage0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_manage"),
        SR_KEMIP_INT, ki_rtpengine_manage,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_manage2"),
        SR_KEMIP_INT, ki_rtpengine_manage2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("rtpengine_offer0"),
        SR_KEMIP_INT, ki_rtpengine_offer0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_offer"),
        SR_KEMIP_INT, ki_rtpengine_offer,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_offer2"),
        SR_KEMIP_INT, ki_rtpengine_offer2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("rtpengine_answer0"),
        SR_KEMIP_INT, ki_rtpengine_answer0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_answer"),
        SR_KEMIP_INT, ki_rtpengine_answer,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_answer2"),
        SR_KEMIP_INT, ki_rtpengine_answer2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("rtpengine_delete0"),
        SR_KEMIP_INT, ki_rtpengine_delete0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_delete"),
        SR_KEMIP_INT, ki_rtpengine_delete,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_delete2"),
        SR_KEMIP_INT, ki_rtpengine_delete2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("start_recording"),
        SR_KEMIP_INT, ki_start_recording,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("stop_recording"),
        SR_KEMIP_INT, ki_stop_recording,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("block_media0"),
        SR_KEMIP_INT, ki_block_media0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("block_media"),
        SR_KEMIP_INT, ki_block_media,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("block_media2"),
        SR_KEMIP_INT, ki_block_media2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

	{ str_init("rtpengine"), str_init("unblock_media0"),
        SR_KEMIP_INT, ki_unblock_media0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("unblock_media"),
        SR_KEMIP_INT, ki_unblock_media,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("unblock_media2"),
        SR_KEMIP_INT, ki_unblock_media2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("silence_media0"),
        SR_KEMIP_INT, ki_silence_media0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("silence_media"),
        SR_KEMIP_INT, ki_silence_media,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("silence_media2"),
        SR_KEMIP_INT, ki_silence_media2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("unsilence_media0"),
        SR_KEMIP_INT, ki_unsilence_media0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("unsilence_media"),
        SR_KEMIP_INT, ki_unsilence_media,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("unsilence_media2"),
        SR_KEMIP_INT, ki_unsilence_media2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("block_dtmf0"),
        SR_KEMIP_INT, ki_block_dtmf0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("block_dtmf"),
        SR_KEMIP_INT, ki_block_dtmf,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("block_dtmf2"),
        SR_KEMIP_INT, ki_block_dtmf2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
	{ str_init("rtpengine"), str_init("play_dtmf"),
		SR_KEMIP_INT, ki_play_dtmf,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

    { str_init("rtpengine"), str_init("unblock_dtmf0"),
        SR_KEMIP_INT, ki_unblock_dtmf0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("unblock_dtmf"),
        SR_KEMIP_INT, ki_unblock_dtmf,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("unblock_dtmf2"),
        SR_KEMIP_INT, ki_unblock_dtmf2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("play_media"),
        SR_KEMIP_INT, ki_play_media,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("play_media2"),
        SR_KEMIP_INT, ki_play_media2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("stop_media0"),
        SR_KEMIP_INT, ki_stop_media0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("stop_media"),
        SR_KEMIP_INT, ki_stop_media,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("stop_media2"),
        SR_KEMIP_INT, ki_stop_media2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("set_rtpengine_set"),
        SR_KEMIP_INT, ki_set_rtpengine_set,
        { SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("set_rtpengine_set2"),
        SR_KEMIP_INT, ki_set_rtpengine_set2,
        { SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("rtpengine_query0"),
        SR_KEMIP_INT, ki_rtpengine_query0,
        { SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("rtpengine"), str_init("rtpengine_query"),
        SR_KEMIP_INT, ki_rtpengine_query,
        { SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("rtpengine_query2"),
        SR_KEMIP_INT, ki_rtpengine_query2,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { str_init("rtpengine"), str_init("rtpengine_query_v"),
        SR_KEMIP_INT, ki_rtpengine_query_v,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
	{ str_init("rtpengine"), str_init("rtpengine_subscribe_request"),
		SR_KEMIP_INT, ki_subscribe_request,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("rtpengine"), str_init("rtpengine_subscribe_answer"),
		SR_KEMIP_INT, ki_subscribe_answer,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("rtpengine"), str_init("rtpengine_unsubscribe"),
		SR_KEMIP_INT, ki_unsubscribe,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},


    { {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_rtpengine_exports);
	return 0;
}

/**
 * load rtpengine module API
 */
static int bind_rtpengine(rtpengine_api_t *api)
{
	if(!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	api->rtpengine_start_recording = ki_start_recording;
	api->rtpengine_answer = ki_rtpengine_answer2;
	api->rtpengine_offer = ki_rtpengine_offer2;
	api->rtpengine_delete = ki_rtpengine_delete2;
	api->rtpengine_subscribe_request = rtpengine_subscribe_request;
	api->rtpengine_subscribe_answer = rtpengine_subscribe_answer;
	api->rtpengine_unsubscribe = rtpengine_unsubscribe;

	return 0;
}

/**
 * @brief Wraps the subscription request for RTPEngine.
 *
 * @details Constructs a bencode dictionary containing the subscription details.
 * Handles different subscription modes.
 *
 * @param sess The RTPEngine session containing session-related data.
 * @param op The RTPEngine operation (e.g., subscribe , unsubscribe).
 * @param to_tag Optional tag identifying the participant (nullable).
 * @param flags A space-separated string of feature flags used in rtpengine functions.
 * @param subscribe_flags Flags controlling the subscription behavior (e.g., SIPREC mode, inactivity).
 * @param body Optional SDP body for the subscribe answer command (nullable).
 * @return A pointer to a bencode item representing the RTPEngine response, or NULL on failure.
 */
static bencode_item_t *w_rtpengine_subscribe_wrap(
		struct rtpengine_session *sess, enum rtpe_operation op, str *to_tag,
		str *flags, unsigned int subscribe_flags, str *body)
{
	static bencode_buffer_t bencbuf;
	bencode_item_t *dict, *list = NULL;
	struct sip_msg *msg;
	str *viabranch = NULL;

	// Initialize the bencode buffer
	if(bencode_buffer_init(&bencbuf)) {
		LM_ERR("could not initialize bencode_buffer_t\n");
		return NULL;
	}

	// Create and fill a dictionary with subscription data
	dict = bencode_dictionary(&bencbuf);
	if(sess->callid) {
		bencode_dictionary_add_str(dict, "call-id", sess->callid);
	} else if(sess->msg) {
		bencode_dictionary_add_str(dict, "call-id", &sess->msg->callid->body);
	}
	if(sess->branch != RTPENGINE_ALL_BRANCHES) {
		bencode_dictionary_add_str(dict, "via-branch", viabranch);
	}
	if(to_tag && to_tag->len) {
		bencode_dictionary_add_str(dict, "to-tag", to_tag);
	}
	if(subscribe_flags & RTP_SUBSCRIBE_MODE_SIPREC) {
		list = bencode_list(&bencbuf);
		bencode_list_add_string(list, "all");
		bencode_list_add_string(list, "siprec");
	} else if((subscribe_flags & RTP_SUBSCRIBE_LEG_BOTH)
			  == RTP_SUBSCRIBE_LEG_BOTH) {
		list = bencode_list(&bencbuf);
		bencode_list_add_string(list, "all");
	} else if(subscribe_flags & RTP_SUBSCRIBE_LEG_CALLER && sess->from_tag) {
		bencode_dictionary_add_str(dict, "from-tag", sess->from_tag);
	} else if(sess->to_tag) {
		bencode_dictionary_add_str(dict, "from-tag", sess->to_tag);
	}
	if(subscribe_flags & RTP_SUBSCRIBE_MODE_DISABLE) {
		if(!list) {
			list = bencode_list(&bencbuf);
		}
		bencode_list_add_string(list, "inactive");
	}
	if(list) {
		bencode_dictionary_add(dict, "flags", list);
	}
	if(op == OP_SUBSCRIBE_ANSWER) {
		bencode_dictionary_add_str(dict, "sdp", body);
	}

	// Use the session's message or a fake message
	msg = (sess->msg ? sess->msg : faked_msg_get_next());

	return rtpp_function_call_ok(
			&bencbuf, msg, op, flags, NULL, NULL, NULL, dict);
}

/**
 * @brief Allocates and initializes a new participant tag.
 *
 * @details Note that to_tag must be freed after use.
 *
 * @param tag The tag to copy.
 * @return A new allocated tag, or NULL on failure.
 */
static str *rtpengine_new_subs(str *tag)
{
	str *to_tag = shm_malloc(sizeof *to_tag + tag->len);
	if(to_tag) {
		to_tag->s = (char *)(to_tag + 1);
		to_tag->len = tag->len;
		memcpy(to_tag->s, tag->s, tag->len);
	}

	return to_tag;
}

/**
 * @brief Copies RTPEngine stream data into a structured format.
 *
 * @details Extracts stream information from a bencode item and populates the result structure.
 * Validates stream data.
 *
 * @param streams The bencode item containing stream data.
 * @param ret The structure to populate with stream data.
 * @param sess The rtpengine session containing session-related data.
 * @return void.
 */
static void rtpengine_copy_streams(bencode_item_t *streams,
		struct rtpengine_streams *ret_streams, struct rtpengine_session *sess)
{
	bencode_item_t *item, *medias;
	str tag = STR_NULL, label_str = STR_NULL;
	int leg = RTPENGINE_CALLEE, medianum, label, streams_count;

	if(!ret_streams || !streams) {
		return;
	}

	ret_streams->count = 0;
	streams_count = 0;

	// Iterate through each item (each item is related to a participant)
	for(item = streams->child; item; item = item->sibling) {
		// Set leg based on participant tag
		tag.s = bencode_dictionary_get_string(item, "tag", &tag.len);
		if(!tag.s) {
			LM_WARN("could not retrieve tag - placing to %s\n",
					(leg == RTPENGINE_CALLER ? "caller" : "callee"));
		} else {
			if(sess->from_tag && sess->from_tag->len > 0
					&& str_strcmp(&tag, sess->from_tag) == 0) {
				leg = RTPENGINE_CALLER;
			} else if(sess->to_tag && sess->to_tag->len > 0
					  && str_strcmp(&tag, sess->to_tag) != 0) {
				leg = RTPENGINE_CALLER;
			} else {
				leg = RTPENGINE_CALLEE;
			}
		}
		// Retrieve media streams
		medias = bencode_dictionary_get_expect(item, "medias", BENCODE_LIST);
		if(!medias) {
			continue;
		}
		// Iterate through each media stream
		for(medias = medias->child; medias; medias = medias->sibling) {
			streams_count = ret_streams->count;
			if(streams_count == RTP_SUBSCRIBE_MAX_STREAMS) {
				LM_WARN("maximum amount of streams %d reached!\n",
						RTP_SUBSCRIBE_MAX_STREAMS);
				return;
			}
			medianum = bencode_dictionary_get_integer(item, "index", 0);
			label_str.s = bencode_dictionary_get_string(
					medias, "label", &label_str.len);
			if(str2sint(&label_str, &label) < 0) {
				LM_WARN("invalid label %.*s - not integer - skipping\n",
						label_str.len, label_str.s);
				continue;
			}
			ret_streams->streams[streams_count].leg = leg;
			ret_streams->streams[streams_count].label = label;
			ret_streams->streams[streams_count].medianum = medianum;
			ret_streams->count++;
		}
	}
}

static void rtpengine_copy_streams_to_xavp(
		str *stream_xavp, bencode_item_t *streams)
{
	bencode_item_t *item, *medias;
	str label_str = STR_NULL;
	str tag = STR_NULL;
	int label;

	if(!streams) {
		return;
	}

	if(stream_xavp->len == 0) {
		LM_DBG("stream_xavp is not set, nothing to do\n");
		return;
	}

	sr_xval_t stream_val;
	sr_xval_t tag_val;
	sr_xval_t media_xval;
	sr_xavp_t *media_xavp = NULL;

	str label_name = str_init("label");
	str tag_name = str_init("tag");

	// Iterate through each item (each item is related to a participant)
	for(item = streams->child; item; item = item->sibling) {
		// Set leg based on participant tag
		tag.s = bencode_dictionary_get_string(item, "tag", &tag.len);
		if(!tag.s) {
			LM_WARN("could not retrieve tag \n");
			continue;
		}
		memset(&tag_val, 0, sizeof(sr_xval_t));
		tag_val.type = SR_XTYPE_STR;
		tag_val.v.s = tag;

		// Retrieve media streams
		medias = bencode_dictionary_get_expect(item, "medias", BENCODE_LIST);
		if(!medias) {
			continue;
		}

		media_xavp = NULL;
		xavp_add_value(&tag_name, &tag_val, &media_xavp);
		// Iterate through each media stream
		for(medias = medias->child; medias; medias = medias->sibling) {
			label_str.s = bencode_dictionary_get_string(
					medias, "label", &label_str.len);
			if(str2sint(&label_str, &label) < 0) {
				LM_WARN("invalid label %.*s - not integer - skipping\n",
						label_str.len, label_str.s);
				continue;
			}
			memset(&media_xval, 0, sizeof(sr_xval_t));
			media_xval.type = SR_XTYPE_LONG;
			media_xval.v.l = (long)label;
			if(xavp_add_value(&label_name, &media_xval, &media_xavp) == NULL) {
				LM_ERR("failed to add label %d to xavp\n", label);
				continue;
			}
		}
		memset(&stream_val, 0, sizeof(sr_xval_t));
		stream_val.type = SR_XTYPE_XAVP;
		stream_val.v.xavp = media_xavp;
		if(xavp_add_value(stream_xavp, &stream_val, NULL) == NULL) {
			LM_ERR("failed to add stream xavp for tag %s\n", tag.s);
			xavp_free(media_xavp);
			continue;
		}
	}
}

/**
 * @brief Sends a subscribe request command to RTPEngine to request subscription (i.e. receiving a copy of the media).
 *
 * @details This function interacts with RTPEngine to request a copy of media streams for the given session.
 * Note: The to_tag must be freed after use.
 *
 * @param sess The rtpengine session containing session-related data.
 * @param to_tag Pointer to a participant tag which corresponds to the subscription. If NULL, RTPEngine generates it.
 * @param flags A space-separated string of feature flags used in rtpengine functions.
 * @param subscribe_flags Flags controlling the subscription behavior (e.g., SIPREC mode, inactivity).
 * @param ret_body Pointer to a string to store the SDP body extracted from RTPEngine's response.
 * @param ret_streams Pointer to a structure to store stream data extracted from RTPEngine's response.
 * @return 1 on success, -1 on failure.
 */
static int rtpengine_subscribe_request(struct rtpengine_session *sess,
		str **to_tag, str *flags, unsigned int subscribe_flags, str *ret_body,
		struct rtpengine_streams *ret_streams)
{
	str tmp_to_tag;
	bencode_item_t *dict;

	// Wrap the subscribe request command and send to RTPEngine
	dict = w_rtpengine_subscribe_wrap(
			sess, OP_SUBSCRIBE_REQUEST, *to_tag, flags, subscribe_flags, NULL);
	if(!dict) {
		return -1;
	}
	// Extract SDP body from the RTPEngine response
	if(!bencode_dictionary_get_str_dup(dict, "sdp", ret_body)) {
		LM_ERR("failed to extract sdp body from proxy reply\n");
	}
	// Extract stream data from the RTPEngine response
	if(ret_streams) {
		rtpengine_copy_streams(
				bencode_dictionary_get(dict, "tag-medias"), ret_streams, sess);
	}
	// Extract and allocate a new to-tag from the RTPEngine response
	if(!bencode_dictionary_get_str(dict, "to-tag", &tmp_to_tag)) {
		LM_ERR("failed to extract to-tag from proxy reply\n");
	} else {
		*to_tag = rtpengine_new_subs(&tmp_to_tag);
	}
	// Free the bencode buffer
	bencode_buffer_free(bencode_item_buffer(dict));

	return 1;
}

/**
 * @brief Sends a subscribe answer command to RTPEngine.
 *
 * @details If successful, media forwarding will start to the endpoint given in the SDP (body param).
 *
 * @param sess The rtpengine session containing session-related data.
 * @param to_tag The participant tag identifying the subscription.
 * @param flags A space-separated string of feature flags used in rtpengine functions.
 * @param body The SDP body to send as part of the subscribe answer command.
 * @return 1 on success, -1 on failure.
 */
static int rtpengine_subscribe_answer(
		struct rtpengine_session *sess, str *to_tag, str *flags, str *body)
{
	bencode_item_t *ret;

	// Wrap the subscribe answer command and send to RTPEngine
	ret = w_rtpengine_subscribe_wrap(
			sess, OP_SUBSCRIBE_ANSWER, to_tag, flags, 0, body);
	if(!ret) {
		return -1;
	}
	// Free the bencode buffer
	bencode_buffer_free(bencode_item_buffer(ret));
	return ret != NULL;
}

/**
 * @brief Sends an unsubscribe command to RTPEngine to stop an established subscription.
 *
 * @details The subscription to be stopped is identified by the provided to-tag.
 *
 * @param sess The rtpengine session containing session-related data.
 * @param to_tag The participant tag identifying the subscription to stop.
 * @param flags: A space-separated string of feature flags used in rtpengine functions.
 * @return 1 on success, -1 on failure.
 */
static int rtpengine_unsubscribe(
		struct rtpengine_session *sess, str *to_tag, str *flags)
{
	bencode_item_t *ret;

	// Wrap the unsubscribe command and send to RTPEngine
	ret = w_rtpengine_subscribe_wrap(
			sess, OP_UNSUBSCRIBE, to_tag, flags, 0, NULL);
	if(!ret) {
		return -1;
	}
	// Free the bencode buffer
	bencode_buffer_free(bencode_item_buffer(ret));

	return ret != NULL;
}
