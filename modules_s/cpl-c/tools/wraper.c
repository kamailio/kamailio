#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>



/**************************  VARIABLES ************************************/

int   debug = 9;
int   log_stderr = 1;
char  *mem_block = 0;




/**************************  MEMORY  ***********************************/
struct fm_block{};

#ifdef DBG_F_MALLOC
void* fm_malloc(struct fm_block* bl,unsigned int size,char* file, char* func,
					unsigned int line)
{
	return malloc(size);
}
#else
void* fm_malloc(struct fm_block* bl, unsigned int size)
{
	return malloc(size);
}
#endif


#ifdef DBG_F_MALLOC
void  fm_free(struct fm_block *bl, void* p, char* file, char* func, 
				unsigned int line)
{
	free(p);
}
#else
void  fm_free(struct fm_block* bl, void* p)
{
	free(p);
}
#endif




/****************************  DEBUG  *************************************/

void dprint(char * format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}


