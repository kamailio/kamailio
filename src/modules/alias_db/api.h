#ifndef ALIASDB_API_H
#define ALIASDB_API_H
#include "../../core/str.h"

typedef int (*alias_db_lookup_t)(sip_msg_t*, str table);
typedef int (*alias_db_lookup_ex_t)(sip_msg_t*, str table, unsigned long flags);
typedef int (*alias_db_find_t)(sip_msg_t*, str table, char* _in, char* _out, char *flags);

typedef struct alias_db_binds {
	alias_db_lookup_t alias_db_lookup;
	alias_db_lookup_ex_t alias_db_lookup_ex;
	alias_db_find_t alias_db_find;
} alias_db_api_t;

typedef int (*bind_alias_db_f)(alias_db_api_t*);

int bind_alias_db(struct alias_db_binds*);

inline static int alias_db_load_api(alias_db_api_t *pxb)
{
	bind_alias_db_f bind_alias_db_exports;
	if (!(bind_alias_db_exports = (bind_alias_db_f)find_export("bind_alias_db", 1, 0)))
	{
		LM_ERR("Failed to import bind_alias_db\n");
		return -1;
	}
	return bind_alias_db_exports(pxb);
}

#endif /*ALIASDB_API_H*/
