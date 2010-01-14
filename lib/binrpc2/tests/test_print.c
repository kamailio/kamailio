#include <assert.h>
#include <stdio.h>
#include <binrpc.h>



int main()
{
#define BUFF_SIZE	1024
	char buff[BUFF_SIZE];
	brpc_t *req;
	BRPC_STR_STATIC_INIT(mname, "MYmethod");

	assert((req = brpc_req(mname, 1234)));
	assert(brpc_asm(req, "d<cc>d", 1, "ana", "mere", 2));
	assert(brpc_asm(req, "[d<c{<cd>}>cd]", 1, "ana", "mere", 2, "mari", 3));
	assert(0 < brpc_print(req, buff, BUFF_SIZE));
	printf("test1: `%s'\n", buff);



	return 0;
}
