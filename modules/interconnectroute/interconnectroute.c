#include "interconnectroute.h"
#include "../../sr_module.h"
#include "fixups.h"
#include "query.h"
#include "db.h"
#include "config.h"

MODULE_VERSION

str interconnectroute_db_url = str_init(DEFAULT_IXDB_URL);
str voice_service_code = str_init("ext.01.001.8.32260@3gpp.org_1000");
str video_service_code = str_init("ext.01.001.8.32260@3gpp.org_1001");	

static int mod_init(void);
static int child_init(int);
//static int mi_child_init(void);
static void mod_destroy(void);


static int w_ix_orig_trunk_query(struct sip_msg* msg);
static int w_ix_term_trunk_query(struct sip_msg* msg, char* ext_trunk_id);

/************* Module Exports **********************************************/
static cmd_export_t cmds[]={
	{"ix_orig_trunk_query",  (cmd_function)w_ix_orig_trunk_query,  0, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ix_term_trunk_query",  (cmd_function)w_ix_term_trunk_query,  1, ix_trunk_query_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]= {
    { "db_url", PARAM_STR, &interconnectroute_db_url },
    { "voice_service_code", PARAM_STR, &voice_service_code },
    { "video_service_code", PARAM_STR, &video_service_code },
    {0,0,0}
};

static rpc_export_t rpc_methods[];

struct module_exports exports = {
	"interconnectroute",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Export parameters */
	0,          /* exported statistics */
	0,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* Module initialization function */
	0,          /* Response function */
	mod_destroy,/* Destroy function */
	child_init  /* Child initialization function */
};

/************* Interface Functions *****************************************/

/**
 *
 * @return 0 on success, -1 on failure
 */
static int mod_init(void) {
    
    if (interconnectroute_db_init() != 0) {
	LM_ERR("Failed to initialise DB connection\n");
	return -1;
    };
    return 0;
}

static int child_init(int rank) {
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return interconnectroute_db_open();
}


static int w_ix_orig_trunk_query(struct sip_msg* msg) {
    return ix_orig_trunk_query(msg);
}

static int w_ix_term_trunk_query(struct sip_msg* msg, char* ext_trunk_id) {
    return ix_term_trunk_query(msg, ext_trunk_id);
}

//static int mi_child_init(void) {
//	if(mode == CARRIERROUTE_MODE_DB){
//		return carrierroute_db_open();
//	}
//	return 0;
//}

static void mod_destroy(void) {
//	if(mode == CARRIERROUTE_MODE_DB){
//		carrierroute_db_close();
//	}
//	destroy_route_data();
}

//static const char *rpc_cr_reload_routes_doc[2] = {
//	"Reload routes", 0
//};

//static void rpc_cr_reload_routes(rpc_t *rpc, void *c) {
//
//}

//static rpc_export_t rpc_methods[] = {
////	{ "cr.reload_routes",  rpc_cr_reload_routes, rpc_cr_reload_routes_doc, 0},
//	{0, 0, 0, 0}
//};