/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#include <time.h>
#include <unistd.h>
#include "../../mem/shm_mem.h"
#include "../../timer.h"
#include "sms_report.h"
#include "sms_funcs.h"

#define REPORT_TIMEOUT     1*60*60   // one hour
#define START_ERR_MSG      "Your message (or part of it) couldn't be "\
                           "delivered. The SMS Center said: "
#define START_ERR_MSG_LEN  (strlen(START_ERR_MSG))
#define END_ERR_MSG        ". The message was: "
#define END_ERR_MSG_LEN    (strlen( END_ERR_MSG))

struct report_cell {
	int             status;
	time_t          timeout;
	char            *text;
	unsigned int    text_len;
	struct sms_msg  *sms;
};

struct report_cell *report_queue=0;
typedef time_t (get_time_func)(void);
get_time_func  *get_time;




/*-------------- Function to set time - from ser or system ------------------*/
/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* gets the time from ser */
static time_t get_time_ser(void)
{
	return  get_ticks();
}
/* gets the time from system */
static time_t get_time_sys(void)
{
	return time(0);
}
/* detects if the ser time function get_ticks works, and depending of that
   sets the correct time function to be used */
void set_gettime_function(void)
{
	unsigned int t1,t2;

	t1 = get_ticks();
	sleep(2);
	t2 = get_ticks();
	if (!t1 && !t2) {
		get_time = get_time_sys;
		LM_INFO("using system time func.\n");
	} else {
		get_time = get_time_ser;
		LM_INFO("using ser time func.\n");
	}
}




static inline void free_report_cell(struct report_cell *cell)
{
	if (!cell)
		return;
	if (cell->sms && !(--(cell->sms->ref)))
		shm_free(cell->sms);
	cell->sms = 0;
	cell->status = 0;
	cell->timeout = 0;
	cell->text = 0;
	cell->text_len = 0;
}




int init_report_queue(void)
{
	report_queue = (struct report_cell*)
		shm_malloc(NR_CELLS*sizeof(struct report_cell));
	if (!report_queue) {
		LM_ERR("no more free pkg_mem!\n");
		return -1;
	}
	memset( report_queue , 0 , NR_CELLS*sizeof(struct report_cell) );
	return 1;
}




void destroy_report_queue(void)
{
	int i;

	if (report_queue){
		for(i=0;i<NR_CELLS;i++)
			if (report_queue[i].sms)
				free_report_cell(&(report_queue[i]));
		shm_free(report_queue);
		report_queue = 0;
	}
}




void add_sms_into_report_queue(int id, struct sms_msg *sms, char *p, int l)
{
	if (report_queue[id].sms){
		LM_INFO("old message still waiting for report at location %d"
				" -> discarding\n",id);
		free_report_cell(&(report_queue[id]));
	}

	sms->ref++;
	report_queue[id].status = -1;
	report_queue[id].sms = sms;
	report_queue[id].text = p;
	report_queue[id].text_len = l;
	report_queue[id].timeout = get_time() + REPORT_TIMEOUT;
}




int  relay_report_to_queue(int id, char *phone, int status, int *old_status)
{
	struct report_cell *cell;
	int    ret_code;

	cell = &(report_queue[id]);
	ret_code = 0;

	/* first, do we have a match into the sms queue? */
	if (!cell->sms) {
		LM_INFO("report received for cell %d,"
				" but the sms was already trashed from queue!\n",id);
		goto done;
	}
	if (strlen(phone)!=cell->sms->to.len ||
	strncmp(phone,cell->sms->to.s,cell->sms->to.len)) {
		LM_INFO("report received for cell %d, but the phone nr is different"
				"->old report->ignored\n",id);
		goto done;
	}

	if (old_status)
		*old_status = cell->status;
	cell->status = status;
	if (status>=0 && status<32) {
		LM_DBG("sms %d confirmed with code %d\n", id, status);
		ret_code = 2; /* success */
	} else if (status<64) {
		/* provisional report */
		LM_DBG("sms %d received prov. report with"
			" code %d\n",id, status);
		ret_code = 1; /* provisional */
	} else {
		LM_DBG("sms %d received error report with code %d\n",id, status);
		ret_code = 3; /* error */
	}

done:
	return ret_code;
}




void check_timeout_in_report_queue(void)
{
	int i;
	time_t current_time;

	current_time = get_time();
	for(i=0;i<NR_CELLS;i++)
		if (report_queue[i].sms && report_queue[i].timeout<=current_time) {
			LM_INFO("[%lu,%lu] record %d is discarded (timeout), having status"
				" %d\n", (long unsigned int)current_time,
				(long unsigned int)report_queue[i].timeout,
				i,report_queue[i].status);
			free_report_cell(&(report_queue[i]));
		}
}




void remove_sms_from_report_queue(int id)
{
	free_report_cell(&(report_queue[id]));
}




str* get_text_from_report_queue(int id)
{
	static str text;

	text.s = report_queue[id].text;
	text.len = report_queue[id].text_len;
	return &text;
}




struct sms_msg* get_sms_from_report_queue(int id)
{
	return ( report_queue[id].sms);
}




str* get_error_str(int status)
{
	static str err_str;

	switch (status) {
		/*
		case 0: strcat(sms->ascii,"Ok,short message received by the SME");
			break;
		case 1: strcat(sms->ascii,"Ok,short message forwarded by the SC to"
			" the SME but the SC is unable to confirm delivery");
			break;
		case 2: strcat(sms->ascii,"Ok,short message replaced by the SC");
			break;
		case 32: strcat(sms->ascii,"Still trying,congestion");
			break;
		case 33: strcat(sms->ascii,"Still trying,SME busy");
			break;
		case 34: strcat(sms->ascii,"Still trying,no response from SME");
			break;
		case 35: strcat(sms->ascii,"Still trying,service rejected");
			break;
		case 36: strcat(sms->ascii,"Still trying,quality of service not"
			" available");
			break;
		case 37: strcat(sms->ascii,"Still trying,error in SME");
			break;
		case 48:
			err_str.s =
			START_ERR_MSG"Delivery is not possible"END_ERR_MSG;
			err_str.len = 24 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		*/
		case 64:
			err_str.s =
			START_ERR_MSG"Error, remote procedure error"END_ERR_MSG;
			err_str.len = 29 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 65: 
			err_str.s =
			START_ERR_MSG"Error,incompatible destination"END_ERR_MSG;
			err_str.len = 30 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 66: 
			err_str.s =
			START_ERR_MSG"Error,connection rejected by SME"END_ERR_MSG;
			err_str.len = 32 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 67:
			err_str.s = START_ERR_MSG"Error,not obtainable"END_ERR_MSG;
			err_str.len = 20 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 68:
			err_str.s =
			START_ERR_MSG"Error,quality of service not available"END_ERR_MSG;
			err_str.len = 38 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 69:
			err_str.s =
			START_ERR_MSG"Error,no interworking available"END_ERR_MSG;
			err_str.len = 31 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 70:
			err_str.s =
			START_ERR_MSG"Error,SM validity period expired"END_ERR_MSG;
			err_str.len = 32 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 71:
			err_str.s =
			START_ERR_MSG"Error,SM deleted by originating SME"END_ERR_MSG;
			err_str.len = 35 + START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 72:
			err_str.s =
			START_ERR_MSG"Error,SM deleted by SC administration"END_ERR_MSG;
			err_str.len = 37+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 73:
			err_str.s = START_ERR_MSG"Error,SM does not exist"END_ERR_MSG;
			err_str.len = 29+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 96:
			err_str.s = START_ERR_MSG"Error,congestion"END_ERR_MSG;
			err_str.len = 23+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 97:
			err_str.s = START_ERR_MSG"Error,SME busy"END_ERR_MSG;
			err_str.len = 14+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 98:
			err_str.s = START_ERR_MSG"Error,no response from SME"END_ERR_MSG;
			err_str.len = 26+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 99:
			err_str.s = START_ERR_MSG"Error,service rejected"END_ERR_MSG;
			err_str.len = 22+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 100:
			err_str.s =
			START_ERR_MSG"Error,quality of service not available"END_ERR_MSG;
			err_str.len = 38+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		case 101:
			err_str.s = START_ERR_MSG"Error,error in SME"END_ERR_MSG;
			err_str.len = 18+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
			break;
		default:
			err_str.s = START_ERR_MSG"Unknown error code"END_ERR_MSG;
			err_str.len = 18+ START_ERR_MSG_LEN + END_ERR_MSG_LEN;
	}
	return &err_str;
}
