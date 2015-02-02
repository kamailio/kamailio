
#include <sys/time.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../usr_avp.c"
#include "../../pvar.h"
#include "../../lvalue.h"
#include "lib_statsd.h"


MODULE_VERSION

static int mod_init(void);
static int mod_destroy(void);
static int func_gauge(struct sip_msg *msg, char *key, char* val);
static int func_set(struct sip_msg *msg, char *key, char* val);
static int func_time_start(struct sip_msg *msg, char *key);
static int func_time_end(struct sip_msg *msg, char *key);
static int func_incr(struct sip_msg *msg, char *key);
static int func_decr(struct sip_msg *msg, char *key);
static char* get_milliseconds(char *dst);

typedef struct StatsdParams{
    char *ip;
    char *port;
} StatsdParams;

static StatsdParams statsd_params= {};

static cmd_export_t commands[] = {
	{"statsd_gauge", (cmd_function)func_gauge, 2, 0, 0, ANY_ROUTE},
	{"statsd_start", (cmd_function)func_time_start, 1, 0, 0, ANY_ROUTE},
	{"statsd_stop", (cmd_function)func_time_end, 1, 0, 0, ANY_ROUTE},
	{"statsd_incr", (cmd_function)func_incr, 1, 0, 0, ANY_ROUTE},
	{"statsd_decr", (cmd_function)func_decr, 1, 0, 0, ANY_ROUTE},
	{"statsd_set", (cmd_function)func_set, 2, 0, 0, ANY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t parameters[] = {
    {"ip", STR_PARAM, &(statsd_params.ip)},
    {"port", STR_PARAM, &(statsd_params.port)},
    {0, 0, 0}
};

struct module_exports exports = {
    "statsd",    // module name
    DEFAULT_DLFLAGS, // dlopen flags
    commands,        // exported functions
    parameters,      // exported parameters
    NULL,            // exported statistics
    NULL,            // exported MI functions
    NULL,            // exported seudo-variables
    NULL,            // extra processes
    mod_init,        // module init function (before fork. kids will inherit)
    NULL,            // reply processing function
    (destroy_function) mod_destroy,     // destroy function
    NULL       // child init function
};


static int mod_init(void)
{
    int rc = 0;
    if(!statsd_params.ip){
        LM_INFO("Statsd init ip value is null. use default 127.0.0.1\r\n");
    }else{
        LM_INFO("Statsd init ip value %s \r\n", statsd_params.ip);
    }

    if(!statsd_params.port){
        LM_INFO("Statsd init port value is null. use default 8125\r\n");
    } else {
        LM_INFO("Statsd init port value %s\r\n", statsd_params.port);
    }

    rc = statsd_init(statsd_params.ip, statsd_params.port);
    if (rc < 0){
        LM_ERR("Statsd connection failed (ERROR_CODE: %i) \n",rc);
        return -1;
    }else{
        LM_INFO("Statsd: sucess connection to statsd server\n");
    }
    return 0;
}

/**
* destroy module function
*/
static int mod_destroy(void)
{
    statsd_destroy();
    return 0;
}

static int func_gauge(struct sip_msg* msg, char* key, char* val)
{
    return statsd_gauge(key, val);
}

static int func_set(struct sip_msg* msg, char* key, char* val)
{
    return statsd_set(key, val);
}

static int func_time_start(struct sip_msg *msg, char *key)
{
    int_str avp_key, avp_val;
    char unix_time[20];
    get_milliseconds(unix_time);
    avp_key.s.s = key;
    avp_key.s.len = strlen(avp_key.s.s);

    avp_val.s.s = unix_time;
    avp_val.s.len = strlen(avp_val.s.s);

	if (add_avp(AVP_NAME_STR|AVP_VAL_STR, avp_key, avp_val) < 0) {
        LM_ERR("Statsd: time start failed to create AVP\n");
        return -1;
    }
    return 1;
}


static int func_time_end(struct sip_msg *msg, char *key)
{
    char unix_time[20];
    char *endptr;
    long int start_time;
    int result;

    struct search_state st;

    get_milliseconds(unix_time);
    LM_DBG("Statsd: statsd_stop at %s\n",unix_time);
    avp_t* prev_avp;

    int_str avp_value, avp_name;
    avp_name.s.s = key;
    avp_name.s.len = strlen(avp_name.s.s);

    prev_avp = search_first_avp(
        AVP_NAME_STR|AVP_VAL_STR, avp_name, &avp_value, &st);
    if(avp_value.s.len == 0){
        LM_ERR("Statsd: statsd_stop not valid key(%s)\n",key);
        return 1;
    }

    start_time = strtol(avp_value.s.s, &endptr,10);
    if(strlen(endptr) >0){
      LM_DBG(
          "Statsd:statsd_stop not valid key(%s) it's not a number value=%s\n",
          key, avp_value.s.s);
      return 0;
    }

    result = atol(unix_time) - start_time;
    LM_DBG(
        "Statsd: statsd_stop Start_time=%ld unix_time=%ld (%i)\n",
        start_time, atol(unix_time), result);
    destroy_avp(prev_avp);
    return statsd_timing(key, result);
}


static int func_incr(struct sip_msg *msg, char *key)
{
    return statsd_count(key, "+1");
}


static int func_decr(struct sip_msg *msg, char *key)
{
    return statsd_count(key, "-1");
}


char* get_milliseconds(char *dst){
    struct timeval tv;
    long int millis;

    gettimeofday(&tv, NULL);
    millis = (tv.tv_sec * (int)1000) + (tv.tv_usec / 1000);
    snprintf(dst, 21, "%ld", millis);
    return dst;
}
