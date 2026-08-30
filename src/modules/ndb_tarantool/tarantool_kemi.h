/*
 * kamailio_tarantool/ndb_tarantool/tarantool_kemi.h
 * KEMI bindings header for ndb_tarantool
 */

#ifndef _TARANTOOL_KEMI_H_
#define _TARANTOOL_KEMI_H_

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct str_s
	{
		char *s;
		int len;
	} str_t;

	int sr_kemi_tarantool_call(
			void *msg, str_t *proc_name, str_t *params_json, str_t *res_dst);
	int sr_kemi_tarantool_eval(
			void *msg, str_t *lua_code, str_t *params_json, str_t *res_dst);

#ifdef __cplusplus
}
#endif

#endif /* _TARANTOOL_KEMI_H_ */
