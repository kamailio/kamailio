/*! \file
 * \brief MSILO API
 *
 */

#ifndef _MSILO_API_H_
#define _MSILO_API_H_

//#include "../../sr_module.h"

typedef int (*msilo_f)(struct sip_msg*, str*);
typedef struct msilo_api {
	msilo_f m_store;
	msilo_f m_dump;
} msilo_api_t;

typedef int (*bind_msilo_f)(msilo_api_t* api);

/**
 * @brief Load the MSILO API
 */
static inline int load_msilo_api(msilo_api_t *api)
{
	bind_msilo_f bindmsilo;

	bindmsilo = (bind_msilo_f)find_export("bind_msilo", 1, 0);
	if(bindmsilo == 0) {
		LM_ERR("cannot find bind_msilo\n");
		return -1;
	}
	if(bindmsilo(api)<0)
	{
		LM_ERR("cannot bind msilo api\n");
		return -1;
	}
	return 0;
}

#endif
