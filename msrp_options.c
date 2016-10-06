/* V1.1 T.Ntamane controls file xfer size*/
#include <stdlib.h>
#include "../../mem/mem.h"
#include "msrp_options.h"


static msrp_options_t current_msrp_options = MSRP_OPTIONS_DEFAULT;
static int optionsSet = 0;

int init_msrp_options(const msrp_options_t *const opts)
{
 
    if (optionsSet)  
    {  
       LM_ERR("MSRP options already set peviously!!");
       return -1;
    }

    if (opts)
    {
       memcpy(&current_msrp_options, opts, sizeof(msrp_options_t)); 
       ++optionsSet;
    }
    return 0;  
}

void get_current_msrp_options(msrp_options_t *const opts)
{
   
    if (opts) memcpy(opts, &current_msrp_options, sizeof(msrp_options_t));
}

void msrp_print_options() 
{
    LM_INFO("CURRENT msrp_options: max hdr_size[%u]: max_body_size[%u] uses_sys_mem[%c]",
            current_msrp_options.msrp_frame_hdrs_max_size,
            current_msrp_options.msrp_frame_body_max_size,
            current_msrp_options.msrp_frame_use_sys_mem ? 'Y' : 'N');
}

int set_msrp_options_on_buffer(char **buffer)
{

        unsigned int max_frame_size;
        if (!buffer )
        {
            LM_CRIT("null buffer received!!");
            return -1;
        }

        if (!optionsSet)
        {
            LM_WARN("Running on default options :max hdr [%u] max body [%u] use_sys_mem[%c]", 
                     current_msrp_options.msrp_frame_hdrs_max_size, 
                     current_msrp_options.msrp_frame_body_max_size, 
                     current_msrp_options.msrp_frame_use_sys_mem ? 'Y' : 'N'
                   );
        }
        max_frame_size = GET_MSRP_MAX_FRAME_SIZE(&current_msrp_options); 

        if (current_msrp_options.msrp_frame_use_sys_mem)
        {
           *buffer = calloc(1, max_frame_size);
        }       
        else
        {
           *buffer = pkg_malloc(max_frame_size); 
        }     
        if (!*buffer)
        {
            LM_CRIT("Failed to allocate %s memory for buffer!!", current_msrp_options.msrp_frame_use_sys_mem ? "SYSTEM" : "PKG");
            return -1;
        }
        return 0;
}
