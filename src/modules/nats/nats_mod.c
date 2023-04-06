/*
 * NATS module interface
 *
 * Copyright (C) 2021 Voxcom Inc
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "defs.h"
#include "nats_mod.h"
#include "nats_pub.h"
#include "../../core/kemi.h"

MODULE_VERSION

init_nats_sub_ptr _init_nats_sc = NULL;
init_nats_server_ptr _init_nats_srv = NULL;
nats_consumer_worker_t *nats_workers = NULL;
nats_pub_worker_t *nats_pub_workers = NULL;
int nats_pub_workers_num = DEFAULT_NUM_PUB_WORKERS;

static int _nats_proc_count = 0;
char *eventData = NULL;

int *nats_pub_worker_pipes_fds = NULL;
int *nats_pub_worker_pipes = NULL;
static str nats_event_callback = STR_NULL;

static nats_evroutes_t _nats_rts = {0};

/* clang-format off */
static pv_export_t nats_mod_pvs[] = {
	{{"natsData", (sizeof("natsData") - 1)}, PVT_OTHER,
				nats_pv_get_event_payload, 0, 0, 0, 0, 0},
	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"nats_url", PARAM_STRING|USE_FUNC_PARAM, (void*)_init_nats_server_url_add},
	{"num_publish_workers", INT_PARAM, &nats_pub_workers_num},
	{"subject_queue_group", PARAM_STRING|USE_FUNC_PARAM, (void*)_init_nats_sub_add},
	{"event_callback", PARAM_STR,   &nats_event_callback},
	{0, 0, 0}
};

static cmd_export_t cmds[] = {
	{"nats_publish", (cmd_function)w_nats_publish_f,
		  2, fixup_publish_get_value, fixup_publish_get_value_free, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"nats",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,					 /* Exported functions */
	params,					 /* Exported parameters */
	0,						 /* exported MI functions */
	nats_mod_pvs,			 /* exported pseudo-variables */
	0,						 /* response function*/
	mod_init,				 /* module initialization function */
	mod_child_init,			 /* per-child init function */
	mod_destroy				 /* destroy function */
};
/* clang-format on */

static void onMsg(
		natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
	nats_on_message_ptr on_message = (nats_on_message_ptr)closure;
	char *data = (char *)natsMsg_GetData(msg);
	eventData = data;
	nats_run_cfg_route(on_message->rt, &on_message->evname);
	eventData = NULL;
	natsMsg_Destroy(msg);
}

static void connectedCB(natsConnection *nc, void *closure)
{
	char url[NATS_URL_MAX_SIZE];
	str evname = str_init("nats:connected");
	natsConnection_GetConnectedUrl(nc, url, sizeof(url));
	nats_run_cfg_route(_nats_rts.connected, &evname);
}

static void disconnectedCb(natsConnection *nc, void *closure)
{
	char url[NATS_URL_MAX_SIZE];
	str evname = str_init("nats:disconnected");
	natsConnection_GetConnectedUrl(nc, url, sizeof(url));
	nats_run_cfg_route(_nats_rts.disconnected, &evname);
}

static void reconnectedCb(natsConnection *nc, void *closure)
{
	char url[NATS_URL_MAX_SIZE];
	str evname = str_init("nats:connected");
	natsConnection_GetConnectedUrl(nc, url, sizeof(url));
	nats_run_cfg_route(_nats_rts.connected, &evname);
}

static void closedCB(natsConnection *nc, void *closure)
{
	bool *closed = (bool *)closure;
	const char *err = NULL;
	natsConnection_GetLastError(nc, &err);
	LM_INFO("connect failed: %s\n", err);
	*closed = true;
}

void nats_consumer_worker_proc(
		nats_consumer_worker_t *worker)
{
	natsStatus s = NATS_OK;

	// create a loop
	natsLibuv_Init();

	worker->uvLoop = uv_default_loop();
	if(worker->uvLoop != NULL) {
		natsLibuv_SetThreadLocalLoop(worker->uvLoop);
	} else {
		s = NATS_ERR;
	}
	if(s != NATS_OK) {
		LM_ERR("could not set event loop [%s]\n", natsStatus_GetText(s));
	}
	if((s = natsConnection_Connect(&worker->nc->conn, worker->nc->opts))
			!= NATS_OK) {
		LM_ERR("could not connect to nats servers [%s]\n",
				natsStatus_GetText(s));
	}

	s = natsOptions_SetEventLoop(worker->nc->opts, (void *)worker->uvLoop,
			natsLibuv_Attach, natsLibuv_Read, natsLibuv_Write,
			natsLibuv_Detach);
	if(s != NATS_OK) {
		LM_ERR("could not set event loop [%s]\n", natsStatus_GetText(s));
	}

	s = natsConnection_QueueSubscribe(&worker->subscription, worker->nc->conn,
			worker->subject, worker->queue_group, onMsg, worker->on_message);
	if(s != NATS_OK) {
		LM_ERR("could not subscribe [%s]\n", natsStatus_GetText(s));
	}

	s = natsSubscription_SetPendingLimits(worker->subscription, -1, -1);
	if(s != NATS_OK) {
		LM_ERR("could not set pending limits [%s]\n", natsStatus_GetText(s));
	}

	// Run the event loop.
	// This call will return when the connection is closed (either after
	// receiving all messages, or disconnected and unable to reconnect).
	if(s == NATS_OK) {
		uv_run(worker->uvLoop, UV_RUN_DEFAULT);
	}
	if(s != NATS_OK) {
		LM_ERR("nats error [%s]\n", natsStatus_GetText(s));
	}
}

static int mod_init(void)
{
	int i = 0;
	int total_procs = _nats_proc_count + nats_pub_workers_num;
	if(faked_msg_init() < 0) {
		LM_ERR("failed to init faked sip message\n");
		return -1;
	}
	register_procs(total_procs);
	cfg_register_child(total_procs);

	nats_pub_worker_pipes_fds =
			(int *)shm_malloc(sizeof(int) * (nats_pub_workers_num)*2);
	nats_pub_worker_pipes =
			(int *)shm_malloc(sizeof(int) * nats_pub_workers_num);
	for(i = 0; i < nats_pub_workers_num; i++) {
		nats_pub_worker_pipes_fds[i * 2] =
				nats_pub_worker_pipes_fds[i * 2 + 1] = -1;
		if(pipe(&nats_pub_worker_pipes_fds[i * 2]) < 0) {
			LM_ERR("worker pipe(%d) failed\n", i);
			return -1;
		}
	}
	for(i = 0; i < nats_pub_workers_num; i++) {
		nats_pub_worker_pipes[i] = nats_pub_worker_pipes_fds[i * 2 + 1];
	}

	nats_workers =
			shm_malloc(_nats_proc_count * sizeof(nats_consumer_worker_t));
	if(nats_workers == NULL) {
		LM_ERR("error in shm_malloc\n");
		return -1;
	}
	memset(nats_workers, 0, _nats_proc_count * sizeof(nats_consumer_worker_t));

	nats_pub_workers =
			shm_malloc(nats_pub_workers_num * sizeof(nats_pub_worker_t));
	if(nats_pub_workers == NULL) {
		LM_ERR("error in shm_malloc\n");
		return -1;
	}
	memset(nats_pub_workers, 0,
			nats_pub_workers_num * sizeof(nats_pub_worker_t));
	return 0;
}

int init_worker(
		nats_consumer_worker_t *worker, char *subject, char *queue_group)
{
	int buffsize = strlen(subject) + 6;
	char routename[buffsize];
	int rt;
	nats_connection_ptr nc = NULL;

	nats_init_environment();
	nc = _init_nats_connection();
	if(nats_init_connection(nc) < 0) {
		LM_ERR("failed to init nat connections\n");
		return -1;
	}

	memset(worker, 0, sizeof(*worker));
	worker->subject = shm_malloc(strlen(subject) + 1);
	strcpy(worker->subject, subject);
	worker->subject[strlen(subject)] = '\0';
	worker->queue_group = shm_malloc(strlen(queue_group) + 1);
	strcpy(worker->queue_group, queue_group);
	worker->queue_group[strlen(queue_group)] = '\0';
	worker->on_message =
			(nats_on_message_ptr)shm_malloc(sizeof(nats_on_message));
	memset(worker->on_message, 0, sizeof(nats_on_message));

	snprintf(routename, buffsize, "nats:%s", subject);
	routename[buffsize] = '\0';

	rt = route_get(&event_rt, routename);
	if(rt < 0 || event_rt.rlist[rt] == NULL) {
		LM_INFO("route [%s] does not exist\n", routename);
		worker->on_message->rt = -1;
	} else {
		worker->on_message->rt = rt;
	}
	worker->on_message->_evname = malloc(buffsize);
	strcpy(worker->on_message->_evname, routename);
	worker->on_message->evname.s = worker->on_message->_evname;
	worker->on_message->evname.len = strlen(worker->on_message->_evname);
	worker->nc = nc;
	return 0;
}

int init_pub_worker(
		nats_pub_worker_t *worker)
{
	nats_connection_ptr nc = NULL;
	nc = _init_nats_connection();
	if(nats_init_connection(nc) < 0) {
		LM_ERR("failed to init nat connections\n");
		return -1;
	}
	memset(worker, 0, sizeof(*worker));
	worker->nc = nc;
	return 0;
}

void worker_loop(int id)
{
	nats_consumer_worker_t *worker = &nats_workers[id];
	nats_consumer_worker_proc(worker);
	for(;;) {
		sleep(1000);
	}
}

int _nats_pub_worker_proc(
		nats_pub_worker_t *worker, int fd)
{
	natsStatus s = NATS_OK;

	natsLibuv_Init();
	worker->fd = fd;
	worker->uvLoop = uv_default_loop();
	if(worker->uvLoop != NULL) {
		natsLibuv_SetThreadLocalLoop(worker->uvLoop);
	} else {
		s = NATS_ERR;
	}
	if(s != NATS_OK) {
		LM_ERR("could not set event loop [%s]\n", natsStatus_GetText(s));
	}

	if((s = natsConnection_Connect(&worker->nc->conn, worker->nc->opts))
			!= NATS_OK) {
		LM_ERR("could not connect to nats servers [%s]\n",
				natsStatus_GetText(s));
	} else {
		connectedCB(worker->nc->conn, NULL);
	}

	s = natsOptions_SetEventLoop(worker->nc->opts, (void *)worker->uvLoop,
			natsLibuv_Attach, natsLibuv_Read, natsLibuv_Write,
			natsLibuv_Detach);
	if(s != NATS_OK) {
		LM_ERR("could not set event loop [%s]\n", natsStatus_GetText(s));
	}

	uv_pipe_init(worker->uvLoop, &worker->pipe, 0);
	uv_pipe_open(&worker->pipe, worker->fd);
	if(uv_poll_init(worker->uvLoop, &worker->poll, worker->fd) < 0) {
		LM_ERR("uv_poll_init failed\n");
		return 0;
	}
	uv_handle_set_data((uv_handle_t *)&worker->poll, (nats_pub_worker_t *)worker);
	if(uv_poll_start(&worker->poll, UV_READABLE | UV_DISCONNECT, _nats_pub_worker_cb)
			< 0) {
		LM_ERR("uv_poll_start failed\n");
		return 0;
	}
	return uv_run(worker->uvLoop, UV_RUN_DEFAULT);
}

/**
 * @brief Initialize async module children
 */
static int mod_child_init(int rank)
{
	init_nats_sub_ptr n;
	int i = 0;
	int newpid;

	if(rank == PROC_INIT) {
		n = _init_nats_sc;
		while(n) {
			if(init_worker(&nats_workers[i], n->sub, n->queue_group) < 0) {
				LM_ERR("failed to init struct for worker [%d]\n", i);
				return -1;
			}
			n = n->next;
			i++;
		}

		for(i = 0; i < nats_pub_workers_num; i++) {
			if(init_pub_worker(&nats_pub_workers[i]) < 0) {
				LM_ERR("failed to init struct for pub worker[%d]\n", i);
				return -1;
			}
		}

		return 0;
	}

	if(rank == PROC_MAIN) {
		for(i = 0; i < _nats_proc_count; i++) {
			newpid = fork_process(PROC_RPC, "NATS Subscriber", 1);
			if(newpid < 0) {
				LM_ERR("failed to fork worker process %d\n", i);
				return -1;
			} else if(newpid == 0) {
				if(cfg_child_init())
					return -1;
				worker_loop(i);
			} else {
				nats_workers[i].pid = newpid;
			}
		}

		for(i = 0; i < nats_pub_workers_num; i++) {
			newpid = fork_process(PROC_NOCHLDINIT, "NATS Publisher", 1);
			if(newpid < 0) {
				LM_ERR("failed to fork worker process %d\n", i);
				return -1;
			} else if(newpid == 0) {
				if(cfg_child_init())
					return -1;
				close(nats_pub_worker_pipes_fds[i * 2 + 1]);
				cfg_update();
				return (_nats_pub_worker_proc(&nats_pub_workers[i], nats_pub_worker_pipes_fds[i * 2]));
			} else {
				nats_pub_workers[i].pid = newpid;
			}
		}
	}

	return 0;
}

int nats_cleanup_init_sub()
{
	init_nats_sub_ptr n0;
	init_nats_sub_ptr n1;
	n0 = _init_nats_sc;
	while(n0) {
		n1 = n0->next;
		if(n0->sub != NULL) {
			shm_free(n0->sub);
		}
		if(n0->queue_group != NULL) {
			shm_free(n0->queue_group);
		}
		shm_free(n0);
		n0 = n1;
	}
	_init_nats_sc = NULL;
	return 0;
}

int nats_init_connection(nats_connection_ptr c)
{
	natsStatus s = NATS_OK;
	bool closed = false;
	int len;
	char *sc;
	int num_servers = 0;
	init_nats_server_ptr s0;

	s0 = _init_nats_srv;
	while(s0) {
		if(s0->url != NULL && num_servers < NATS_MAX_SERVERS) {
			len = strlen(s0->url);
			sc = shm_malloc(len + 1);
			if(!sc) {
				LM_ERR("no shm memory left\n");
				return -1;
			}
			strcpy(sc, s0->url);
			sc[len] = '\0';
			c->servers[num_servers++] = sc;
			LM_INFO("adding server [%s] [%d]\n", sc, num_servers);
		}
		s0 = s0->next;
	}
	if(num_servers == 0) {
		len = strlen(NATS_DEFAULT_URL);
		sc = shm_malloc(len + 1);
		if(!sc) {
			LM_ERR("no shm memory left\n");
			return -1;
		}
		strcpy(sc, NATS_DEFAULT_URL);
		sc[len] = '\0';
		c->servers[0] = sc;
		LM_INFO("using default server [%s]\n", sc);
	}

	// nats create options
	if((s = natsOptions_Create(&c->opts)) != NATS_OK) {
		LM_ERR("could not create nats options [%s]\n", natsStatus_GetText(s));
		return -1;
	}

	// use these defaults
	natsOptions_SetAllowReconnect(c->opts, true);
	natsOptions_SetSecure(c->opts, false);
	natsOptions_SetMaxReconnect(c->opts, 10000);
	natsOptions_SetReconnectWait(c->opts, 2 * 1000);	 // 2s
	natsOptions_SetPingInterval(c->opts, 2 * 60 * 1000); // 2m
	natsOptions_SetMaxPingsOut(c->opts, 2);
	natsOptions_SetIOBufSize(c->opts, 32 * 1024); // 32 KB
	natsOptions_SetMaxPendingMsgs(c->opts, 65536);
	natsOptions_SetTimeout(c->opts, 2 * 1000);				   // 2s
	natsOptions_SetReconnectBufSize(c->opts, 8 * 1024 * 1024); // 8 MB;
	natsOptions_SetReconnectJitter(c->opts, 100, 1000);		   // 100ms, 1s;

	// nats set servers and options
	if((s = natsOptions_SetServers(
				c->opts, (const char **)c->servers, num_servers))
			!= NATS_OK) {
		LM_ERR("could not set nats server[%s]\n", natsStatus_GetText(s));
		return -1;
	}

	// nats set callbacks
	s = natsOptions_SetDisconnectedCB(c->opts, disconnectedCb, NULL);
	if(s != NATS_OK) {
		LM_ERR("could not set disconnect callback [%s]\n",
				natsStatus_GetText(s));
	}

	s = natsOptions_SetReconnectedCB(c->opts, reconnectedCb, NULL);
	if(s != NATS_OK) {
		LM_ERR("could not set reconnect callback [%s]\n",
				natsStatus_GetText(s));
	}

	s = natsOptions_SetRetryOnFailedConnect(c->opts, true, connectedCB, NULL);
	if(s != NATS_OK) {
		LM_ERR("could not set retry on failed callback [%s]\n",
				natsStatus_GetText(s));
	}

	s = natsOptions_SetClosedCB(c->opts, closedCB, (void *)&closed);
	if(s != NATS_OK) {
		LM_ERR("could not set closed callback [%s]\n", natsStatus_GetText(s));
	}
	return 0;
}

int nats_cleanup_init_servers()
{
	init_nats_server_ptr s0;
	init_nats_server_ptr s1;
	s0 = _init_nats_srv;
	while(s0) {
		s1 = s0->next;
		if(s0->url != NULL) {
			shm_free(s0->url);
		}

		shm_free(s0);
		s0 = s1;
	}

	// To silence reports of memory still in used with valgrind
	nats_Close();

	_init_nats_srv = NULL;
	return 0;
}

int nats_cleanup_connection(nats_connection_ptr c)
{
	int s;
	if(c->conn != NULL) {
		natsConnection_Close(c->conn);
		natsConnection_Destroy(c->conn);
	}
	if(c->opts != NULL) {
		natsOptions_Destroy(c->opts);
	}
	for(s = 0; s < NATS_MAX_SERVERS; s++) {
		if(c->servers[s]) {
			shm_free(c->servers[s]);
		}
	}
	shm_free(c);
	return 0;
}

int nats_destroy_workers()
{
	int i;
	nats_consumer_worker_t *worker;
	nats_pub_worker_t *pub_worker;
	if(nats_workers != NULL) {
		for(i = 0; i < _nats_proc_count; i++) {
			worker = &nats_workers[i];
			if(worker != NULL) {
				if(worker->subscription != NULL) {
					natsSubscription_Unsubscribe(worker->subscription);
					natsSubscription_Destroy(worker->subscription);
				}
				if(worker->uvLoop != NULL) {
					uv_loop_close(worker->uvLoop);
				}
				if(worker->subject != NULL) {
					shm_free(worker->subject);
				}
				if(worker->queue_group != NULL) {
					shm_free(worker->queue_group);
				}
				if(worker->nc != NULL) {
					if(nats_cleanup_connection(worker->nc) < 0) {
						LM_ERR("could not cleanup worker connection\n");
					}
				}
				if(worker->on_message != NULL) {
					if (worker->on_message->_evname) {
						free(worker->on_message->_evname);
					}
					shm_free(worker->on_message);
				}
				shm_free(worker);
			}
		}
	}

	if(nats_pub_workers != NULL) {
		for(i = 0; i < nats_pub_workers_num; i++) {
			pub_worker = &nats_pub_workers[i];
			if(pub_worker != NULL) {
				if(pub_worker->nc != NULL) {
					if(nats_cleanup_connection(pub_worker->nc) < 0) {
						LM_ERR("could not cleanup worker connection\n");
					}
				}
				if(uv_is_active((uv_handle_t*)&pub_worker->poll)) {
					uv_poll_stop(&pub_worker->poll);
				}
				shm_free(pub_worker);
			}
		}
	}
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	if(nats_destroy_workers() < 0) {
		LM_ERR("could not cleanup workers\n");
	}
	if(nats_cleanup_init_sub() < 0) {
		LM_INFO("could not cleanup init data\n");
	}
	if(nats_cleanup_init_servers() < 0) {
		LM_INFO("could not cleanup init server data\n");
	}
	if(nats_pub_worker_pipes_fds) {
		shm_free(nats_pub_worker_pipes_fds);
	}
	if(nats_pub_worker_pipes) {
		shm_free(nats_pub_worker_pipes);
	}
}

int _init_nats_server_url_add(modparam_t type, void *val)
{
	char *url = (char *)val;
	int len = strlen(url);
	char *value;
	if(len > NATS_URL_MAX_SIZE) {
		LM_ERR("connection url exceeds max size %d\n", NATS_URL_MAX_SIZE);
		return -1;
	}
	if(strncmp(url, "nats://", 7)) {
		LM_ERR("invalid nats url [%s]\n", url);
		return -1;
	}
	value = pkg_malloc(len + 1);
	if(!value) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	strcpy(value, url);
	value[len] = '\0';
	if(init_nats_server_url_add(url) < 0) {
		LM_ERR("could not add server\n");
	}
	pkg_free(value);
	return 0;
}

int _init_nats_sub_add(modparam_t type, void *val)
{
	char *sub = (char *)val;
	int len = strlen(sub);
	char *s = pkg_malloc(len + 1);
	if(!s) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	strcpy(s, sub);
	s[len] = '\0';
	if(init_nats_sub_add(s) < 0) {
		LM_ERR("could not add init data\n");
	}
	pkg_free(s);
	return 0;
}

/**
 * Invoke an event route block
 */
int nats_run_cfg_route(int rt, str *evname)
{
	struct run_act_ctx ctx;
	sr_kemi_eng_t *keng = NULL;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;

	keng = sr_kemi_eng_get();

	// check for valid route pointer
	if(rt < 0 || !event_rt.rlist[rt]) {
		if (keng == NULL) return 0;
	}

	fmsg = faked_msg_next();
	memcpy(&tmsg, fmsg, sizeof(sip_msg_t));
	fmsg = &tmsg;
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if (rt < 0 && keng) {
		if (sr_kemi_route(keng, fmsg, EVENT_ROUTE,
			&nats_event_callback, evname) < 0) {
			LM_ERR("error running event route kemi callback\n");
		}
		return 0;
	}
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	return 0;
}

void nats_init_environment()
{
	memset(&_nats_rts, 0, sizeof(nats_evroutes_t));
	_nats_rts.connected = route_lookup(&event_rt, "nats:connected");
	if(_nats_rts.connected < 0 || event_rt.rlist[_nats_rts.connected] == NULL)
		_nats_rts.connected = -1;

	_nats_rts.disconnected = route_lookup(&event_rt, "nats:disconnected");
	if(_nats_rts.disconnected < 0
			|| event_rt.rlist[_nats_rts.disconnected] == NULL)
		_nats_rts.disconnected = -1;
}

init_nats_server_ptr _init_nats_server_list_new(char *url)
{
	init_nats_server_ptr p =
			(init_nats_server_ptr)shm_malloc(sizeof(init_nats_server));
	memset(p, 0, sizeof(init_nats_server));
	p->url = shm_malloc(strlen(url) + 1);
	strcpy(p->url, url);
	p->url[strlen(url)] = '\0';
	return p;
}


int init_nats_server_url_add(char *url)
{
	init_nats_server_ptr n;
	n = _init_nats_srv;
	while(n != NULL) {
		n = n->next;
	}
	n = _init_nats_server_list_new(url);
	n->next = _init_nats_srv;
	_init_nats_srv = n;
	return 0;
}

nats_connection_ptr _init_nats_connection()
{
	nats_connection_ptr p =
			(nats_connection_ptr)shm_malloc(sizeof(nats_connection));
	memset(p, 0, sizeof(nats_connection));
	return p;
}

init_nats_sub_ptr _init_nats_sub_new(char *sub, char *queue_group)
{
	init_nats_sub_ptr p = (init_nats_sub_ptr)shm_malloc(sizeof(init_nats_sub));
	memset(p, 0, sizeof(init_nats_sub));
	p->sub = shm_malloc(strlen(sub) + 1);
	strcpy(p->sub, sub);
	p->sub[strlen(sub)] = '\0';
	p->queue_group = shm_malloc(strlen(queue_group) + 1);
	strcpy(p->queue_group, queue_group);
	p->queue_group[strlen(queue_group)] = '\0';
	return p;
}

int init_nats_sub_add(char *sc)
{
	int len;
	char *s;
	char *c;
	init_nats_sub_ptr n;

	if(sc == NULL) {
		return -1;
	}

	len = strlen(sc);
	s = pkg_malloc(len + 1);
	if(!s) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	strcpy(s, sc);
	s[len] = '\0';

	if((c = strchr(s, ':')) != 0) {
		*c = 0;
		for(c = c + 1; !*c; c++)
			;
	}
	if(s == NULL) {
		goto error;
		return -1;
	}
	if(c == NULL) {
		goto error;
		return -1;
	}

	n = _init_nats_sc;
	while(n != NULL) {
		n = n->next;
	}
	n = _init_nats_sub_new(s, c);
	n->next = _init_nats_sc;
	_init_nats_sc = n;
	_nats_proc_count++;


error:
	pkg_free(s);
	return 0;
}

int nats_pv_get_event_payload(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res)
							 : pv_get_strzval(msg, param, res, eventData);
}

/**
 *
 */
int ki_nats_publish(sip_msg_t *msg, str *subject, str *payload)
{
	return w_nats_publish(msg, *subject, *payload);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_nats_exports[] = {
	{ str_init("nats"), str_init("publish"),
		SR_KEMIP_INT, ki_nats_publish,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_nats_exports);
	return 0;
}
