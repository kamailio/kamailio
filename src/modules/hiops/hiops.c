/*
 * Copyright (C) 2019 Mojtaba Esfandiari.S, Nasim-Telecom
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "hiops.h"
//#include "../../core/mem/mem.h"
//#include "../../core/mem/shm_mem.h"


MODULE_VERSION

//static int pv_get_liid(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

int th_msg_received(sr_event_param_t *evp);
//int th_msg_received(sr_event_param_t *evp);
//int th_msg_sent(sr_event_param_t *evp);



static param_export_t params[] = {
//        {"hi1_ipaddress", STR_PARAM|USE_FUNC_PARAM, (void*)set_hi1_ipaddress},  //deleted
        {"hi1_sock", STR_PARAM|USE_FUNC_PARAM, (void*)hi1_sock_set_param},
        {"hi1_data", INT_PARAM, &hi1_data},
        {"hi1_eventrb",   PARAM_STRING, &hi1_eventrb},
        {"hi1_worker_process",   PARAM_INT, &worker_process},
        {"hi2_mid_ims_session",  PARAM_INT, &mid_ims_session},
        {"hi2_1xx_provisinal",  PARAM_INT, &provisinal_responses},
        {"hi2_200_successful",  PARAM_INT, &successful_responses},
        {"hi2_3xx_redirection",  PARAM_INT, &redirection_responses},
        {"hi2_4xx_request_failure",  PARAM_INT, &request_failure_responses},
        {"hi2_5xx_server_failure",  PARAM_INT, &server_failure_responses},
        {"hi2_6xx_global_failure",  PARAM_INT, &server_failure_responses},

        {0,0,0}
};

static pv_export_t mod_pvs[] = {
        { {"liid", sizeof("liid")-1}, PVT_OTHER, hi1_pv_get_liid, 0, 0, 0, 0, 0 },
        { {"target", sizeof("target")-1}, PVT_OTHER, hi1_pv_get_target, 0, 0, 0, 0, 0 },
        { {"event", sizeof("event")-1}, PVT_OTHER, hi1_pv_get_event, 0, 0, 0, 0, 0 },

        { {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/**
 * initial function, This function is run when module is loaded on kamailio start.
 * @return success:0, error:-1
 */
static int mod_init(void) {
    LM_DBG("hiops, mod_init function\n");

    /* load the htable API */
    if (htable_load_api(&htable_api) != 0) {
                LM_ERR("can't load htable API\n");
        return -1;
    }

//    default value for shared memory varibales.It is called when the module is loaded.
    hi1_shm_memory();
    hi2_shm_memory();

//      add event route block for hi1ops.
    hi1_eventrb_init();

    register_procs((worker_process) ? worker_process : LIMD_WORKERS_PROCESS);
    cfg_register_child((worker_process) ? worker_process : LIMD_WORKERS_PROCESS);

    sr_event_register_cb(SREV_NET_DATA_IN, th_msg_received);


//    sr_event_register_cb(SREV_NET_DATA_IN, th_msg_received);
//    sr_event_register_cb(SREV_NET_DATA_OUT, th_msg_sent);

    return 0;
}

/**
 * child function, This function is run for every fork process.
 * @param rank
 * @return success:0, error:-1
 */
static int child_init(int rank) {
    int pid;

    if (rank == PROC_INIT) {
        LM_DBG("hiops, rank is PROC_INIT in child_init function\n");
        hi1_socket_open();
        /* Add some extra works here... */

        return 0;
    }

    if (rank == PROC_TCP_MAIN) {
        LM_DBG("hiops, rank is PROC_TCP_MAIN in child_init function\n");
        /* Add some extra works here... */

        return 0;
    }

    if (rank == PROC_MAIN) {
        int i = 0;
        int worker_no =  (worker_process) ? worker_process : LIMD_WORKERS_PROCESS;
        LM_DBG("hiops, rank is PROC_MAIN in child_init function\n");

        while (i++ < worker_no) {
            pid = fork_process(LIMD_WORKERS_RANKING, "LIMD_WORKER_PROCESS", 1);
            if (pid < 0)
                return -1; /* error */
            if (pid == 0) {
                /* child space, initialize the config framework */
                if (cfg_child_init())
                    return -1;

                /* Add some extra works here... */
                return 0;
            }
        }

        return 0;
    }

    if (rank == LIMD_WORKERS_RANKING) {
        //start worker jobs from here, in other works, start extra worker process here.
        hi1_socket_listen();
        /* Add some extra works here... */

    }

    return 0;
}


/**
 * destroy function, This function is run on kamailio stop.
 */
static void mod_destroy(void)
{
    LM_DBG("<hiops module>, mod_destroy()\n");
//    hi1_socket_close();
    hi1_destructor();
    hi2_destructor();
}

/** module exports */
struct module_exports exports= {
        "hiops",
        DEFAULT_DLFLAGS, /* dlopen flags */
        0,
        params,
        0,          /* exported statistics */
        0  ,        /* exported MI functions */
        mod_pvs,    /* exported pseudo-variables */
        0,          /* extra processes */
        mod_init,   /* module initialization function */
        0,
        mod_destroy,
        child_init           /* per-child init function */
};


int th_msg_received(sr_event_param_t *evp) {

    sip_msg_t msg;
    str *obuf;
//    callid;
//    char correlation[32];

//    if(!hi1_head_listed_p->next){
//        LM_DBG("There is no active liid in hiops service,\n");
//        return -1;
//    }

    obuf = (str *) evp->data;
//    obuf = (str*)data;
    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = obuf->len;

    msg.rcv = *evp->rcv;


//    LM_INFO("<hiops module>---->Here is th_msg_received function.\r\n");

//    LM_DBG("prepare new msg for cseq update operations\n");
    if (parse_msg(msg.buf, msg.len, &msg) != 0) {
                LM_DBG("outbuf buffer parsing failed!");
        return 1;
    }

    if (parse_headers(&msg, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F, 0) != 0) {
//    if (parse_headers(&msg, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F | HDR_CSEQ_F | HDR_VIA1_F | HDR_VIA2_F, 0) != 0) {
//        if (parse_headers(&msg, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F | HDR_VIA1_F | HDR_VIA2_F, 0) != 0) {
                LM_DBG("error parsing header\n");

        return -1;
    }

//    if (received_in_via(&msg))
    if (hi2_check_received_ip_via(&msg))

        LM_INFO("The rcv ip address is exist in via headers.\n");
    else {

        if (hi2_iri_sip_exist_in_htable(&msg) != 0){
//            LM_INFO("The message is retransmited:%d\n", val);
            return 0;
        }

        LM_INFO("insert new record in htable successfully\n");
        hi2_iri_interception(&msg, hi1_head_listed_p->next);

//        if(exist_hash_table())
//        char msg_md5[32];
//        str msg_buf;
//        msg_buf.s = msg.buf;
//        msg_buf.len = msg.len;
//        MD5StringArray(msg_md5, &msg_buf, 1);
//
//        str ht, key;
//        ht.s = "checkertrance";
//        ht.len = 13;
//
//        key.s = msg_md5;
//        key.len = 32;
//
//        int_str value;
//        value.n = 1;
//
//        int val;
//
//        if (htable_api.get_expire(&ht, &key, &val) != 0) {
//                    LM_INFO("ERROR while insert new record in htable\n");
//            return;
//        } else {
//            if (val) {
//                        LM_INFO("The message is retransmited:%d\n", val);
//                        LM_INFO("MD5 message: <%.*s>\n", 32, msg_md5);
//                return 0;
//            } else
//                        LM_INFO ("The message is not retransmited:%d\n", val);
//                    LM_INFO("MD5 message: <%.*s>\n", 32, msg_md5);
//        }
//
//        if (htable_api.set(&ht, &key, 0, &value, 1) != 0) {
//                    LM_INFO("ERROR while insert new record in htable\n");
//        }
//
//                LM_INFO("insert new record in htable successfully\n");

    }
    return 0;

}



