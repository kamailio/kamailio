#include <cds/cds.h>
#include <cds/memory.h>
#include <cds/sync.h>
#include <cds/logger.h>

#if 0

typedef struct {
	int init_cnt;
} init_data_t;

static init_data_t *init = NULL;

/* these functions are internal and thus are not presented in headers !*/
int reference_counter_initialize();
void reference_counter_cleanup();
	
int cds_initialize()
{
	int res = 0;

	/* initialization should be called from one process/thread 
	 * it is not synchronized because it is impossible ! */
	if (!init) {
		init = (init_data_t*)cds_malloc(sizeof(init_data_t));
		if (!init) return -1;
		init->init_cnt = 0;
	}

	if (init->init_cnt > 0) { /* already initialized */
		init->init_cnt++;
		return 0;
	}
	else {
		DEBUG_LOG("init the content\n");
		
		/* !!! put the real initialization here !!! */
		res = reference_counter_initialize();
	}
			
	if (res == 0) init->init_cnt++;
	return res;
}

void cds_cleanup()
{
	if (init) {
		if (--init->init_cnt == 0) {
			DEBUG_LOG("cleaning the content\n");
			
			/* !!! put the real destruction here !!! */
			reference_counter_cleanup();
		
			cds_free(init);
			init = NULL;
		}
	}
}

#endif
