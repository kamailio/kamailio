#ifndef _H_SMS_REPORT_DEF
#define _H_SMS_REPORT_DEF

#include "../../str.h"
#include "sms_funcs.h"

#define NR_CELLS  256


int   init_report_queue();
void  destroy_report_queue();
void  add_sms_into_report_queue(int id, struct sms_msg *sms, char *, int );
int   relay_report_to_queue(int id, char *phone, int status);
void  check_timeout_in_report_queue();
str*  get_error_str(int status);
void  remove_sms_from_report_queue(int id);
str*  get_text_from_report_queue(int id);
struct sms_msg* get_sms_from_report_queue(int id);
void  set_gettime_function();


#endif
