#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "xcap_params.h"

#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/sstr.h>
#include <cds/dstring.h>

/* global variables holding parameters (AVPs not used due to problems with them if
 * new transactions (sending messages, ...) in processing */

/*static struct sip_msg *msg = NULL;*/ /* if parameters stored and msg is different, the store is reinitialized before */
static unsigned int msgid = 0;
static char xcap_root[512] = "";
static char xcap_filename[512] = "";

str default_xcap_root = {s: "", len: 0 };

/* clears all stored parameters */
static void clear_params()
{
	*xcap_root = 0;
	/* TODO: SSL, auth params, ... */
}

static void check_params(struct sip_msg *m)
{
	/* clears all stored parameters if for other message */
	if (m) {
		if ((m->id == msgid)) return; /* don't clean params */
	}
	
	clear_params();
	if (m) msgid = m->id;
	else msgid = 0;
}

static void set_param_value(struct sip_msg *m, char *dst, 
		int dst_size, const char *value)
{
	check_params(m);

	strncpy(dst, value, dst_size - 1);
	dst[dst_size - 1] = 0; 
}

#if 0

/* AVPs are not used due to problems with them !!! */

static int get_xcap_root(str *dst)
{
	avp_t *avp;
	int_str name, val;
	str avp_xcap_root = STR_STATIC_INIT("xcap_root");

	/* if (!dst) return -1; */
	
	name.s = avp_xcap_root;
	avp = search_first_avp(AVP_NAME_STR, name, &val, 0);
	if (avp) {
		/* don't use default - use value from AVP */
		DBG("redefined xcap_root = %.*s\n", FMT_STR(val.s));
		*dst = val.s;
	} 
	else {
		*dst = default_xcap_root;
	}
	
	if (is_str_empty(dst)) return -1;
	return 0;
}

#endif

int fill_xcap_params_impl(struct sip_msg *m, xcap_query_params_t *params)
{
	int res = 0;
	int use_default = 1;
	
	if (!params) return -1;
	
	check_params(m);
	
	use_default = 1;
	if (*xcap_root)
		use_default = 0;
	if (use_default) params->xcap_root = default_xcap_root;
	else {
		params->xcap_root.s = xcap_root;
		params->xcap_root.len = strlen(xcap_root);
	}
	
/*	ERR("**** XCAP PARAMS: ****\n");
	ERR("XCAP ROOT: %.*s\n", FMT_STR(params->xcap_root)); */
	/*ERR"(XCAP FILE: %.*s\n", FMT_STR(params->xcap_file); */
	return res;
}

int set_xcap_root(struct sip_msg* m, char* value, char* _x)
{
	set_param_value(m, xcap_root, sizeof(xcap_root), value);
	return 1;
}

int set_xcap_filename(struct sip_msg* m, char* value, char* _x)
{
	set_param_value(m, xcap_filename, sizeof(xcap_filename), value);
	return 1;
}
