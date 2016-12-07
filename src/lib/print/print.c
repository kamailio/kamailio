/* 
 * example library 
 */



#include <stdio.h>

int stderr_println(char* text)
{
	fprintf(stderr, "%s\n", text);
	return 0;
}
