#ifndef _SECFILTER_H
#define _SECFILTER_H

#include "../../core/str_list.h"
#include "../../core/sr_module.h"


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

extern secf_data_p secf_data;

int secf_append_rule(int action, int type, str *value);

/* Get header values from message */
int secf_get_ua(struct sip_msg *msg, str *ua);
int secf_get_from(struct sip_msg *msg, str *name, str *user, str *domain);
int secf_get_to(struct sip_msg *msg, str *name, str *user, str *domain);
int secf_get_contact(struct sip_msg *msg, str *user, str *domain);

/* Database functions */
int secf_init_db(void);
int secf_init_data(void);
void secf_free_data(void);
int secf_load_db(void);

/* Extern variables */
extern str secf_db_url;
extern str secf_table_name;
extern str secf_action_col;
extern str secf_type_col;
extern str secf_data_col;
extern int secf_dst_exact_match;

/* RPC commands */
void secf_rpc_reload(rpc_t *rpc, void *ctx);
void secf_rpc_print(rpc_t *rpc, void *ctx);
void secf_rpc_add_dst(rpc_t *rpc, void *ctx);
void secf_rpc_add_bl(rpc_t *rpc, void *ctx);
void secf_rpc_add_wl(rpc_t *rpc, void *ctx);

#endif
