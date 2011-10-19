#ifndef PUAUSRLOC_API_H
#define PUAUSRLOC_API_H
#include "../../str.h"

typedef int (*pua_set_publish_t)(struct sip_msg*, char *, char *);

typedef struct pua_usrloc_binds {
	pua_set_publish_t pua_set_publish;
} pua_usrloc_api_t;

typedef int (*bind_pua_usrloc_f)(pua_usrloc_api_t*);

int bind_pua_usrloc(struct pua_usrloc_binds*);

inline static int pua_usrloc_load_api(pua_usrloc_api_t *pxb)
{
	bind_pua_usrloc_f bind_pua_usrloc_exports;
	if (!(bind_pua_usrloc_exports = (bind_pua_usrloc_f)find_export("bind_pua_usrloc", 1, 0)))
	{
		LM_ERR("Failed to import bind_pua_usrloc\n");
		return -1;
	}
	return bind_pua_usrloc_exports(pxb);
}

#endif /*PUAUSRLOC_API_H*/
