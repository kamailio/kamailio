
#include <inttypes.h>

#include "cobj.h"

#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/trim.h"

MODULE_VERSION

static int w_call_obj_get(struct sip_msg *msg, char *result);

static int w_call_obj_free(struct sip_msg* msg, char* num_obj);

static int mod_init(void);
static void mod_destroy(void);

/**
 * Module parameters
 */
/* Actually, negative or zero values are not allowed. */
int call_obj_start = 0;
int call_obj_end = 0;

/* module commands */
static cmd_export_t cmds[] = {
	{"call_obj_get", (cmd_function)w_call_obj_get, 1, fixup_pvar_null, fixup_free_pvar_null, ANY_ROUTE},
	{"call_obj_free", (cmd_function)w_call_obj_free, 1, fixup_var_str_1, 0, ANY_ROUTE},
	{ 0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"start", PARAM_INT, &call_obj_start},
	{"end", PARAM_INT, &call_obj_end},
	{0, 0, 0}
};

/* RPC commands. */

static void rpc_call_obj_free(rpc_t *rpc, void *ctx)
{
	str obj_str;
	int obj_num;
	
	if (rpc->scan(ctx, "S", &obj_str) < 1) {
		rpc->fault(ctx, 400, "required object number argument");
		return;
	}

	if (str2int(&obj_str, (unsigned int*)&obj_num)) {
		LM_ERR("Cannot convert %.*s to number\n", obj_str.len, obj_str.s);
		rpc->fault(ctx, 400, "cannot convert string to number");
		return;
	}
	LM_DBG("Param value: %d\n", obj_num);

	if (cobj_free(obj_num)) {
		LM_ERR("Freeing object: %d\n", obj_num);
		rpc->fault(ctx, 500, "error freeing object");
		return;
	}

	return;
}

static void rpc_call_obj_stats(rpc_t *rpc, void *ctx)
{
	cobj_stats_t stats;
	
	if (cobj_stats_get(&stats)) {
		LM_ERR("Cannot get statistics for module\n");
		rpc->fault(ctx, 500, "cannot get statistics for module");
		return;
	}

	if (rpc->rpl_printf(ctx, "Start: %d  End: %d", stats.start, stats.end) < 0) {
		return;
	}

	int total = stats.end - stats.start + 1;
	double percentage = 100.0 * stats.assigned / total;
	if (rpc->rpl_printf(ctx, "Total: %d  Assigned: %d  (%.*f%%)",
						total, stats.assigned, 2, percentage)) {
		return;
	}

	return;
}

static void rpc_call_obj_free_all(rpc_t *rpc, void *ctx)
{
	cobj_free_all();

	return;
}

static void rpc_call_obj_list(rpc_t *rpc, void *ctx)
{
	int duration = 0;
	int limit = 0; /* Maximum number of objects to return. 0 means unlimited. */
	cobj_elem_t *list = NULL;

	int rc = rpc->scan(ctx, "d*d", &duration, &limit);
	if (rc != -1 && rc != 2) {
		rpc->fault(ctx, 400, "requires arguments for duration number (and optionally limit)");
		goto clean;
	}
	
	if (duration < 0) {
		rpc->fault(ctx, 400, "duration argument shouldn\'t be negative");
		goto clean;
	}

	if (limit < 0) {
		rpc->fault(ctx, 400, "limit argument shouldn\'t be negative");
		goto clean;
	}
	
	uint64_t current_ts;
	uint64_t dur_ms = duration;
	dur_ms *= 1000; /* duration in milliseconds */
	if (get_timestamp(&current_ts)) {
		LM_ERR("error getting timestamp");
		rpc->fault(ctx, 500, "error getting timestamp");
		goto clean;
	}

	if (current_ts < dur_ms) {
		rpc->fault(ctx, 400, "duration is too long");
		goto clean;
	}

	uint64_t timestamp = current_ts - dur_ms;
	int num = cobj_get_timestamp(timestamp, &list, limit);
	if (num < 0) {
		rpc->fault(ctx, 500, "error getting call list");
		goto clean;
	}

	rpc->rpl_printf(ctx, "Number of calls: %d", num);
	if (limit && limit < num) {
		rpc->rpl_printf(ctx, "Showing only: %d", limit);
	}
	cobj_elem_t *elem = list;
	while (elem) {
		rpc->rpl_printf(ctx, "%d  ts: %" PRIu64 "  Call-ID: %.*s", elem->number,
						elem->timestamp, elem->callid.len, elem->callid.s);
		elem = elem->next;
	}

clean:

	/* Free list */
	if (list) {
		cobj_free_list(list);
	}
	
	return;
}

static const char* rpc_call_obj_free_all_doc[2] = {
	"Free all objects at once",
	0
};
	
static const char* rpc_call_obj_stats_doc[2] = {
	"Show statistics about module objects",
	0
};
	
static const char* rpc_call_obj_free_doc[2] = {
	"Free an object so it can be assigned again",
	0
};

static const char* rpc_call_obj_list_doc[2] = {
	"Get a list of objects with a longer duration (in seconds) than a number",
	0
};

static rpc_export_t rpc_cmds[] = {
	{"call_obj.free", rpc_call_obj_free, rpc_call_obj_free_doc, 0},
	{"call_obj.stats", rpc_call_obj_stats, rpc_call_obj_stats_doc, 0},
	{"call_obj.free_all", rpc_call_obj_free_all, rpc_call_obj_free_all_doc, 0},
	{"call_obj.list", rpc_call_obj_list, rpc_call_obj_list_doc, 0},
	{0, 0, 0, 0}
};

struct module_exports exports = {
	"call_obj",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0, //mod_pvs,        /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	0      /* per child init function */
};

static int mod_init(void)
{
	LM_DBG("Start parameter: %d\n", call_obj_start);
	LM_DBG("End parameter: %d\n", call_obj_end);

	if (rpc_register_array(rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	
	if (cobj_init(call_obj_start, call_obj_end)) {
		LM_ERR("Could not start module\n");
		return -1;
	}
	
	return 0;
}

static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");
	cobj_destroy();
}

/**
 * Looks for the Call-ID header
 * On error content pointed by s is undefined.
 *
 * @param msg - the sip message
 * @param s  pointer to str where we will store callid.
 *
 * @returns 0 on success
 */
static int get_call_id(struct sip_msg *msg, str *s)
{
	if (!msg) {
		LM_ERR("No message available\n");
		return -1;
	}

	if ( (!msg->callid && parse_headers(msg, HDR_CALLID_F, 0)!=0) || msg->callid==0 ) {
		LM_ERR("failed to parse Call-ID\n");
		return -1;
	}

  	if( msg->callid->body.s==NULL || msg->callid->body.len==0) {
  		LM_ERR("cannot parse Call-ID header\n");
  		return -1;
  	}

	*s = msg->callid->body;
	trim(s);
	
	return 0;
}

/**
 *
 */
static int w_call_obj_get(struct sip_msg *msg, char *result)
{
	int ret_code = -1; /* It fails by default. */

	if (!msg) {
		LM_ERR("No SIP message found\n");
		goto clean;
	}
	
	pv_spec_t *res;
	
	if(result==NULL)
	{
		LM_ERR("No result variable\n");
		goto clean;
	}
	res = (pv_spec_t *)result;

	str call_id;
	if (get_call_id(msg, &call_id)) {
		LM_ERR("Cannot get callid header\n");
		goto clean;
	}
	LM_DBG("CallId: %.*s\n", call_id.len, call_id.s);

	uint64_t current_ts;
	if (get_timestamp(&current_ts)) {
		LM_ERR("error getting timestamp");
		goto clean;
	}
	
	int obj = cobj_get(current_ts, &call_id);
	if (obj == -1) {
		LM_ERR("Getting object\n");
		goto clean;
	}
	/* obj >= 0 */
	
	pv_value_t val;

	char *obj_str = NULL;
	int len = 0;
	obj_str = int2str((unsigned long)obj, &len);
	if (!obj_str) {
		LM_ERR("Cannot convert number %d to string\n", obj);
		goto clean;
	}
	
	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_STR;
	val.rs.s = obj_str;
	val.rs.len = len;
	LM_DBG("Obj string: %s\n", obj_str);
	
	if(res->setf(msg, &res->pvp, (int)EQ_T, &val)<0)
	{
		LM_ERR("setting result PV failed\n");
		goto clean;
	}

	ret_code = 1;
	
clean:
	return ret_code;
}

/**
 *
 */
static int w_call_obj_free(struct sip_msg* msg, char* num_obj)
{
	int c_obj_num = 0;

	str num_obj_str;
	
	if (get_str_fparam(&num_obj_str, msg, (fparam_t*)num_obj) < 0)
	{
		LM_ERR("failed to get object value\n");
		return -1;
	}

	if (num_obj_str.len == 0)
	{
		LM_ERR("invalid object parameter - empty value\n");
		return -1;
	}
	LM_DBG("Param string value: %.*s\n", num_obj_str.len, num_obj_str.s);

	if (str2int(&num_obj_str, (unsigned int*)&c_obj_num)) {
		LM_ERR("Cannot convert %.*s to number\n", num_obj_str.len, num_obj_str.s);
		return -1;
	}
	LM_DBG("Param value: %d\n", c_obj_num);

	if (cobj_free(c_obj_num)) {
		LM_ERR("Freeing object: %d\n", c_obj_num);
		return -1;
	}

	return 1;
}
