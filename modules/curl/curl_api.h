#ifndef _CURL_CPI_H_
#define _CURL_API_H_

#include "../../sr_module.h"
#include "functions.h"

typedef int (*curlapi_curlconnect_f)(struct sip_msg *msg, const str *connection, const str* _url, str* _result, const char *contenttype, const str* _post);

typedef struct curl_api {
	curlapi_curlconnect_f	curl_connect;
} curl_api_t;

typedef int (*bind_curl_api_f)(curl_api_t *api);
int bind_curl_api(curl_api_t *api);

/**
 * @brief Load the CURL API
 */
static inline int curl_load_api(curl_api_t *api)
{
	bind_curl_api_f bindcurl;

	bindcurl = (bind_curl_api_f)find_export("bind_curl", 0, 0);
	if(bindcurl == 0) {
		LM_ERR("cannot find bind_curl\n");
		return -1;
	}
	if (bindcurl(api) < 0)
	{
		LM_ERR("cannot bind curl api\n");
		return -1;
	}
	return 0;
}

#endif
