#include "functions.h"
#include "curl_api.h"
int bind_curl_api(curl_api_t *api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->curl_connect = curl_con_query_url;

	return 0;
}
