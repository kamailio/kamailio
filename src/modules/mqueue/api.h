/**
 * Copyright (C) 2010 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

		       
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
