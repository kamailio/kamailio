#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <binrpc.h>

void test1()
{
	char *uri = "brpcns://www.heise.de:2046";
	brpc_addr_t *addr;

	assert((addr = brpc_parse_uri(uri)));
}

void test1_5()
{
	char *uri = "brpcns://localhost:2046";
	brpc_addr_t *addr;

	assert((addr = brpc_parse_uri(uri)));
}

void test1_6()
{
	char *uri = "brpcns://localhost2046";
	brpc_addr_t *addr;

	assert(! (addr = brpc_parse_uri(uri)));
}


void test2()
{
	char *uri = "brpcns://fe80::250:56ff:fec0:8";
	brpc_addr_t *addr;

	assert((addr = brpc_parse_uri(uri)));
}

void test3()
{
	char *uri = "brpc6s://[fe80::250:56ff:fec0:8]";
	brpc_addr_t *addr;

	assert((addr = brpc_parse_uri(uri)));
}

void test4()
{
	char *uri = "brPcns://[fe80::250:56ff:fec0:8]:2046";
	brpc_addr_t *addr;

	assert((addr = brpc_parse_uri(uri)));
}

void test5()
{
	char *uri = "brpxns://[fe80::250:56ff:fec0:8]:2046";
	brpc_addr_t *addr;

	assert(! (addr = brpc_parse_uri(uri)));
}

void test6()
{
	char *uri = "brpcld://~/var/run/unix.sock";
	brpc_addr_t *addr;

	assert((addr = brpc_parse_uri(uri)));
}

#define myassert(expr) \
	do { \
		if (! expr) { \
			printf("ERR:%d: %s [%d].\n", __LINE__, brpc_strerror(), \
					brpc_errno); \
			abort(); \
		} \
	} while (0)

brpc_t *send_recv_msg(char *uri, brpc_t *snd_msg)
{
	brpc_addr_t *addr;
	int srvfd, cltfd, wrkfd;
	brpc_t *rcv_msg;
	brpc_addr_t wrkaddr;

	memset((char *)&wrkaddr, 0, sizeof(brpc_addr_t));
	assert(snd_msg);

	assert((addr = brpc_parse_uri(uri)));
	assert(0 <= (srvfd = brpc_socket(addr, true, true)));
	if (addr->socktype == SOCK_STREAM)
		assert(listen(srvfd, 11) == 0);
	cltfd = -1;
	assert(brpc_connect(addr, &cltfd, 1000000));
	if (addr->socktype == SOCK_STREAM)
		assert(0 <= (wrkfd = accept(srvfd, 
				(struct sockaddr *)&wrkaddr.sockaddr, &wrkaddr.addrlen)));
	else
		assert(0 < (wrkfd = dup(srvfd)));

	assert(brpc_send(cltfd, snd_msg, 500000));
	assert((rcv_msg = brpc_recv(wrkfd, 500000)));

	assert(close(wrkfd) == 0);
	assert(close(srvfd) == 0);
	assert(close(cltfd) == 0);

	return rcv_msg;
}

void uri_tst(char *uri)
{
	int a = 1, b = 22, c = 333;
	int *A, *B, *C;
	brpc_id_t cid = 1234;
	BRPC_STR_STATIC_INIT(mname, "method name");
	brpc_t *snd_msg, *rcv_msg;
	snd_msg = brpc_req(mname, cid);
	assert(brpc_asm(snd_msg, "d<dd>", a, b, c));
	assert((rcv_msg = send_recv_msg(uri, snd_msg)));
	brpc_finish(snd_msg);
	brpc_dsm(rcv_msg, "d<dd>", &A, &B, &C);
	assert(a == *A);
	assert(b == *B);
	assert(c == *C);
	assert(brpc_id(rcv_msg) == cid);
	brpc_finish(rcv_msg);
}

void test7()
{
	uri_tst("brpcns://localhost");
	uri_tst("brpc4d://127.0.0.1:4455");
	uri_tst("brpcns://[::1]:45667");
#if 1

	unlink("/tmp/test_net.usock");
	uri_tst("brpcls:///tmp/test_net.usock");
	
	unlink("/tmp/test_net.usock");
	uri_tst("brpcld:///tmp/test_net.usock");
#endif
}


int main()
{
#if 1
	test1(); printf("1: OK.\n");
	test1_5(); printf("1.5: OK.\n");
	test1_6(); printf("1.6: OK.\n");
	test2(); printf("2: OK.\n");
	test3(); printf("3: OK.\n");
	test4(); printf("4: OK.\n");
	test5(); printf("5: OK.\n");
	test6(); printf("6: OK.\n");
#endif
	test7(); printf("7: OK.\n");
	return 0;
}
