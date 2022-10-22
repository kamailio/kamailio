#ifndef _SECFILTER_H
#define _SECFILTER_H

#include "../../core/str_list.h"
#include "../../core/sr_module.h"

#define BL_UA 0
#define BL_COUNTRY 1
#define BL_FDOMAIN 2
#define BL_TDOMAIN 3
#define BL_CDOMAIN 4
#define BL_IP 5
#define BL_FNAME 6
#define BL_TNAME 7
#define BL_CNAME 8
#define BL_FUSER 9
#define BL_TUSER 10
#define BL_CUSER 11
#define WL_UA 12
#define WL_COUNTRY 13
#define WL_FDOMAIN 14
#define WL_TDOMAIN 15
#define WL_CDOMAIN 16
#define WL_IP 17
#define WL_FNAME 18
#define WL_TNAME 19
#define WL_CNAME 20
#define WL_FUSER 21
#define WL_TUSER 22
#define WL_CUSER 23
#define BL_DST 24
#define BL_SQL 25

typedef struct _secf_info
{
	struct str_list *ua;
	struct str_list *country;
	struct str_list *domain;
	struct str_list *user;
	struct str_list *ip;
	struct str_list *dst;
} secf_info_t, *secf_info_p;

typedef struct _secf_data
{
	gen_lock_t lock;
	secf_info_t wl; /* whitelist info */
	secf_info_t wl_last;
	secf_info_t bl; /* blacklist info */
	secf_info_t bl_last;
} secf_data_t, *secf_data_p;

extern secf_data_p *secf_data;
extern secf_data_p secf_data_1;
extern secf_data_p secf_data_2;

extern int *secf_stats;
void secf_reset_stats(void);

int secf_append_rule(int action, int type, str *value);

/* Get header values from message */
int secf_get_ua(struct sip_msg *msg, str *ua);
int secf_get_from(struct sip_msg *msg, str *name, str *user, str *domain);
int secf_get_to(struct sip_msg *msg, str *name, str *user, str *domain);
int secf_get_contact(struct sip_msg *msg, str *user, str *domain);

/* Database functions */
int secf_init_db(void);
int secf_init_data(void);
void secf_free_data(secf_data_p secf_fdata);
int secf_load_db(void);

/* Extern variables */
extern str secf_db_url;
extern str secf_table_name;
extern str secf_action_col;
extern str secf_type_col;
extern str secf_data_col;
extern int secf_dst_exact_match;
extern int secf_reload_delta;
extern int secf_reload_interval;
extern time_t *secf_rpc_reload_time;

/* RPC commands */
void secf_rpc_reload(rpc_t *rpc, void *ctx);
void secf_rpc_print(rpc_t *rpc, void *ctx);
void secf_rpc_stats(rpc_t *rpc, void *ctx);
void secf_rpc_stats_reset(rpc_t *rpc, void *ctx);
void secf_rpc_add_dst(rpc_t *rpc, void *ctx);
void secf_rpc_add_bl(rpc_t *rpc, void *ctx);
void secf_rpc_add_wl(rpc_t *rpc, void *ctx);

#endif
