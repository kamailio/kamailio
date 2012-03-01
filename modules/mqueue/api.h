#ifndef _MQUEUE_EXT_API_H_
#define _MQUEUE_EXT_API_H_

typedef int (*mq_add_f)(str*, str*, str*);
typedef struct mq_api {
	mq_add_f add;
} mq_api_t;

typedef int (*bind_mq_f)(mq_api_t* api);

static inline int load_mq_api(mq_api_t *api)
{
	bind_mq_f bindmq;

	bindmq = (bind_mq_f)find_export("bind_mq", 1, 0);
	if(bindmq == 0) {
		LM_ERR("cannot find bind_mq\n");
		return -1;
	}
	if(bindmq(api)<0)
	{
		LM_ERR("cannot bind mq api\n");
		return -1;
	}
	return 0;
}

#endif
