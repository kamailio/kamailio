/* V1.1 T.Ntamane controls file xfer size*/
#ifndef MSRP_OPTIONS
#define MSRP_OPTIONS

#include "msrp_parser.h"

typedef struct
{
    unsigned int   msrp_frame_hdrs_max_size;
    unsigned int   msrp_frame_body_max_size;
    unsigned short msrp_frame_use_sys_mem;  /*1 = SYSTEM 0 = PKG*/
}msrp_options_t; 

#define  MSRP_OPTIONS_DEFAULT {MSRP_MAX_HDRS_SIZE, MSRP_MAX_BODY_SIZE, 0};
#define  GET_MSRP_MAX_FRAME_SIZE(msrp_opt)  ((msrp_opt)->msrp_frame_hdrs_max_size + (msrp_opt)->msrp_frame_body_max_size)

int  init_msrp_options(const msrp_options_t *const opts);
void get_current_msrp_options(msrp_options_t *const opts);
int set_msrp_options_on_buffer(char **buffer);
void msrp_print_options();
#endif
