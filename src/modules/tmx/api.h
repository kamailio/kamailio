#ifndef _TMX_API_H_
#define _TMX_API_H_

typedef int (*tmx_t_suspend_f)(struct sip_msg*, char*, char*);
typedef struct tmx_api {
	tmx_t_suspend_f t_suspend;
} tmx_api_t;

typedef int (*bind_tmx_f)(tmx_api_t* api);

static inline int load_tmx_api(tmx_api_t *api)
{
	bind_tmx_f bindtmx;

	bindtmx = (bind_tmx_f)find_export("bind_tmx", 1, 0);
	if(bindtmx == 0) {
		LM_ERR("cannot find bind_tmx\n");
		return -1;
	}
	if(bindtmx(api)<0)
	{
		LM_ERR("cannot bind tmx api\n");
		return -1;
	}
	return 0;
}

#endif
