#ifndef RLS_API_H
#define RLS_API_H
#include "../../str.h"

typedef int (*rls_handle_subscribe_t)(struct sip_msg*, str, str);
typedef int (*rls_handle_subscribe0_t)(struct sip_msg*);
typedef int (*rls_handle_notify_t)(struct sip_msg*, char*, char*);

typedef struct rls_binds {
	rls_handle_subscribe_t rls_handle_subscribe;
	rls_handle_subscribe0_t rls_handle_subscribe0;
	rls_handle_notify_t rls_handle_notify;
} rls_api_t;

typedef int (*bind_rls_f)(rls_api_t*);

int bind_rls(struct rls_binds*);

inline static int rls_load_api(rls_api_t *pxb)
{
	bind_rls_f bind_rls_exports;
	if (!(bind_rls_exports = (bind_rls_f)find_export("bind_rls", 1, 0)))
	{
		LM_ERR("Failed to import bind_rls\n");
		return -1;
	}
	return bind_rls_exports(pxb);
}

#endif /*RLS_API_H*/
