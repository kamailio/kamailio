#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../tm/tm_load.h"
//#include "../sl/sl.h"
#include "../../modules_k/dialog/dlg_load.h"


MODULE_VERSION

/* module function prototypes */
static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

void trace_tm(struct cell* t, int type, struct tmcb_params *ps);
//void trace_sl(unsigned int types, struct sip_msg* req, struct sl_cb_param *sl_param);
void trace_dlg(struct dlg_cell* dlg, int type, struct dlg_cb_params * params);

int m_add(struct sip_msg * msg);

static struct tm_binds tmb;
//static register_slcb_t register_slcb_f=NULL;	/*!< stateless callback registration */
static struct dlg_binds dlg_api;


static param_export_t params[] = {
    {0, 0, 0}
};



struct module_exports exports = {
    "ui-test", 
    0,       /*!< Exported commands */
    0,       /*!< Exported RPC methods */
    params,     /*!< Exported parameters */
    mod_init,   /*!< module initialization function */
    0,          /*!< response function */
    mod_destroy,/*!< destroy function */
	0,
    child_init  /*!< child initialization function */
};


static int mod_init(void)
{
    LM_NOTICE("initializing ui-test...\n");

    /* register callbacks to TM */
    if (load_tm_api(&tmb)!=0) {
        LM_ERR("can't load tm api. Is module tm loaded?\n");
        return -1;
    }

    if(tmb.register_tmcb(0, 0, TMCB_REQUEST_IN, trace_tm, 0, 0) <=0) {
        LM_ERR("can't register for TMCB_REQUEST_IN\n");
        return -1;
    }

    /* register sl callback */
//    register_slcb_f = (register_slcb_t)find_export("register_slcb", 0, 0);
//    if(register_slcb_f==NULL) {
//        LM_ERR("can't load sl api. Is module sl loaded?\n");
//        return -1;
//    }
//    if(register_slcb_f(SLCB_REPLY_OUT, trace_sl, NULL)!=0) {
//        LM_ERR("can't register for SLCB_REPLY_OUT\n");
//        return -1;
//    }
//    if(register_slcb_f(SLCB_ACK_IN, trace_sl, NULL)!=0) {
//        LM_ERR("can't register for SLCB_ACK_IN\n");
//        return -1;
//    }

    /* register initial dialog callback */
    if (load_dlg_api(&dlg_api) != 0) {
        LM_ERR("can't load dialog api.\n");
        return(-1);
    }
    if (dlg_api.register_dlgcb(NULL, DLGCB_CREATED, trace_dlg, NULL, NULL) < 0) {
        LM_ERR("can't register for DLGCB_CREATED\n");
        return -1;
    }

    LM_NOTICE("initialization complete.\n");
    return 0;
}


static int child_init(int rank)
{
    return 0;
}


static void mod_destroy(void)
{
}

void trace_tm(struct cell* t, int type, struct tmcb_params *ps)
{
    struct sip_msg* msg;

    LM_NOTICE("TRACE_TM CALLED\n");

    //    if(parse_from_header(msg)==-1 || msg->from==NULL || get_from(msg)==NULL) {
    //        LM_ERR("cannot parse FROM header\n");
    //        return;
    //    }
    //
    //    if(parse_headers(msg, HDR_CALLID_F, 0)!=0) {
    //        LM_ERR("cannot parse call-id\n");
    //        return;
    //    }

    if (type&TMCB_REQUEST_IN) {
        LM_NOTICE("request was received\n");
        if(t==NULL || ps==NULL) {
            LM_NOTICE("no uas request, local transaction\n");
            return;
        }

        msg = ps->req;
        if(msg==NULL) {
            LM_NOTICE("no uas request, local transaction\n");
            return;
        }
        //	str *method = &REQ_LINE(msg).method;

        //	    if (tmb.register_tmcb(0, t, TMCB_REQUEST_BUILT, trace_tm, 0, 0) < 0) {
        //		LM_ERR("can't register for remaining tm events\n");
        //		return;
        //	    }

        LM_NOTICE("new incoming request -- concluding tm callback registrations.\n");

		int tmcb_types = TMCB_RESPONSE_IN | TMCB_E2EACK_IN | TMCB_REQUEST_FWDED | TMCB_RESPONSE_FWDED | TMCB_ON_FAILURE_RO | TMCB_ON_FAILURE | TMCB_RESPONSE_OUT | TMCB_LOCAL_COMPLETED | TMCB_LOCAL_RESPONSE_OUT | TMCB_ACK_NEG_IN | TMCB_REQ_RETR_IN | TMCB_LOCAL_RESPONSE_IN | /*TMCB_LOCAL_REQUEST_IN |*/ TMCB_DLG | TMCB_DESTROY | TMCB_E2ECANCEL_IN | TMCB_E2EACK_RETR_IN | TMCB_RESPONSE_READY | TMCB_REQUEST_PENDING | TMCB_REQUEST_SENT | TMCB_RESPONSE_SENT;
		#ifdef WITH_AS_SUPPORT
		tmcb_types |= TMCB_DONT_ACK;
		#endif
//		#ifdef TMCB_ONSEND
//		tmcb_types |= (TMCB_REQUEST_SENT | TMCB_RESPONSE_SENT);
//		#endif
        if (tmb.register_tmcb( 0, t, tmcb_types, trace_tm, 0, 0) < 0) {
            LM_ERR("can't register for remaining tm events\n");
            return;
        }
    }

    if (type&TMCB_REQUEST_IN) {
//        LM_NOTICE("request was received\n");
    }
	else if (type&TMCB_RESPONSE_IN) {
        LM_NOTICE("response was received\n");
    }
    else if (type&TMCB_E2EACK_IN) {
        LM_NOTICE("e2e ACK observed (INVITE completed)\n");
    }
    else if (type&TMCB_REQUEST_FWDED) {
        LM_NOTICE("request about to be forwarded\n");
    }
    else if (type&TMCB_RESPONSE_FWDED) {
        LM_NOTICE("response about to be forwarded\n");
    }
    else if (type&TMCB_ON_FAILURE_RO) {
        LM_NOTICE("failed reply received or timer occurred (RO)\n");
    }
    else if (type&TMCB_ON_FAILURE) {
        LM_NOTICE("failed reply received or timer occurred\n");
    }
    else if (type&TMCB_RESPONSE_OUT) {
        LM_NOTICE("response was sent\n");
    }
    else if (type&TMCB_LOCAL_COMPLETED) {
        LM_NOTICE("locally initiated transaction completed\n");
    }
    else if (type&TMCB_LOCAL_RESPONSE_OUT) {
        LM_NOTICE("locally generated response was sent\n");
    }
    else if (type&TMCB_ACK_NEG_IN) {
        LM_NOTICE("negative ACK was received\n");
    }
    else if (type&TMCB_REQ_RETR_IN) {
        LM_NOTICE("retried request was received\n");
    }
    else if (type&TMCB_LOCAL_RESPONSE_IN) {
        LM_NOTICE("locally generated response was received\n");
    }
    else if (type&TMCB_LOCAL_REQUEST_IN) {
        LM_NOTICE("locally generated request was received\n");
    }
    else if (type&TMCB_DLG) {
        LM_NOTICE("tm dialog created\n");
    }
    else if (type&TMCB_DESTROY) {
        LM_NOTICE("transaction was destroyed\n");
    }
    else if (type&TMCB_E2ECANCEL_IN) {
        LM_NOTICE("e2e CANCEL observed\n");
    }
    else if (type&TMCB_E2EACK_RETR_IN) {
        LM_NOTICE("retried e2e ACK was received\n");
    }
    else if (type&TMCB_RESPONSE_READY) {
        LM_NOTICE("response about to be sent\n");
    }
    else if (type&TMCB_REQUEST_PENDING) {
        LM_NOTICE("request-pending about to be returned\n");
    }
	#ifdef WITH_AS_SUPPORT
    else if (type&TMCB_DONT_ACK) {
        LM_NOTICE("don't ack observed\n");
    }
	#endif
//	#ifdef WITH_ONSEND
    else if (type&TMCB_REQUEST_SENT) {
        LM_NOTICE("request was sent (possibly retransmitted)\n");
    }
    else if (type&TMCB_RESPONSE_SENT) {
        LM_NOTICE("response was sent (possibly retransmitted)\n");
    }
//	#endif
	else {
		LM_NOTICE("unknown tmcb type: %x\n", type);
	}

    if (ps == NULL) {
        LM_NOTICE("no tm parameters\n");
        return;
    }

    //    if (ps->req != NULL) LM_NOTICE("addr req (msg id: %d): %p", ps->req->id, ps->req); else LM_NOTICE("addr req: <none>\n");
    //    if (ps->rpl != NULL) LM_NOTICE("addr rpl (msg id: %d): %p", ps->rpl->id, ps->rpl); else LM_NOTICE("addr rpl: <none>\n");
    //    if (ps->extra1 != NULL) LM_NOTICE("addr extra: %p", ps->extra1); else LM_NOTICE("addr extra: <none>\n");

    if (ps->send_buf.s != NULL && ps->send_buf.len > 0) {
        LM_NOTICE("send buffer size is: %d\n", ps->send_buf.len);
        LM_NOTICE("send buffer is:\n%.*s\n", ps->send_buf.len, ps->send_buf.s);
    } else {
        LM_NOTICE("no send buffer attached.\n");
    }

    LM_NOTICE("Trying req/rpl...\n");
    if (ps->req != NULL) {
        msg = ps->req;
        LM_NOTICE("req size is: %d\n", msg->len);
        LM_NOTICE("req message (with id %d) is:\n%.*s\n", msg->id, msg->len, msg->buf);
    }
    if (ps->rpl != NULL && ps->rpl != FAKED_REPLY) {
        msg = ps->rpl;
        LM_NOTICE("rpl size is: %d\n", msg->len);
        LM_NOTICE("rpl message (with id %d) is:\n%.*s\n", msg->id, msg->len, msg->buf);
    }
}


//void trace_sl( unsigned int types, struct sip_msg* req, struct sl_cb_param *sl_param)
//{
//    LM_NOTICE("TRACE_SL CALLED\n");
//
//    if (types&SLCB_REPLY_OUT) {
//        LM_NOTICE("reply was sent (stateless)\n");
//    }
//    if (types&SLCB_ACK_IN) {
//        LM_NOTICE("ack was received (stateless)\n");
//    }
//
//    LM_NOTICE("SIP message is: %.*s\n", req->len, req->buf);
//}


void trace_dlg(struct dlg_cell* dlg, int type, struct dlg_cb_params *params)
{
    LM_NOTICE("TRACE_DLG CALLED\n");

    if (type&DLGCB_CREATED) {
        LM_NOTICE("new dialog created -- concluding dialog callback registrations.\n");

        if (dlg_api.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_CONFIRMED | DLGCB_REQ_WITHIN | DLGCB_EXPIRED | DLGCB_EARLY | DLGCB_RESPONSE_FWDED | DLGCB_RESPONSE_WITHIN | DLGCB_DESTROY, trace_dlg, NULL, NULL) != 0) {
//        if (dlg_api.register_dlgcb(dlg, DLGCB_UNCONFIRMED | DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_CONFIRMED | DLGCB_REQ_WITHIN | DLGCB_EXPIRED | DLGCB_EARLY | DLGCB_RESPONSE_FWDED | DLGCB_RESPONSE_WITHIN | DLGCB_DESTROY, trace_dlg, NULL, NULL) != 0) {
            LM_ERR("can't register for remaining dialog events\n");
        }

    }

    if (params == NULL || params->msg == NULL) {
        LM_ERR("no SIP message attached in trace_dlg()\n");
        return;
    }

    if (type&DLGCB_REQ_WITHIN) {
        LM_NOTICE("sequential request was observed\n");
    }

    if (type&DLGCB_TERMINATED) {
        LM_NOTICE("dialog was terminated\n");
    }

    if (type&DLGCB_FAILED) {
        LM_NOTICE("dialog failed\n");
    }

    if (type&DLGCB_CONFIRMED) {
        LM_NOTICE("dialog was confirmed\n");
    }

    if (type&DLGCB_EXPIRED) {
        LM_NOTICE("dialog expired\n");
    }

    if (type&DLGCB_EARLY) {
        LM_NOTICE("early dialog was established\n");
    }

    if (type&DLGCB_RESPONSE_FWDED) {
        LM_NOTICE("response was forwarded\n");
    }

    if (type&DLGCB_RESPONSE_WITHIN) {
        LM_NOTICE("sequential response was observed\n");
    }
    if (type&DLGCB_DESTROY) {
        LM_NOTICE("destroyed\n");
    }
}
