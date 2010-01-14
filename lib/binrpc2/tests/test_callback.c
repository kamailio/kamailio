#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <binrpc.h>

brpc_t *req_cb_fn1_t1(brpc_t *_, void *__) {return NULL;}
brpc_t *req_cb_fn2_t1(brpc_t *_, void *__) {return NULL;}

void test1()
{
	brpc_cb_init(111, 0);

	assert (brpc_cb_req("req_cb_fn1_t1", "[ddd]", req_cb_fn1_t1, NULL, NULL));
	assert (brpc_cb_req("req_cb_fn1_t1", "[dcdd]", req_cb_fn2_t1, "foo", 
		NULL));
	assert (! brpc_cb_req("req_cb_fn1_t1", "[ddd]", req_cb_fn2_t1, NULL, 
		NULL));
	assert (brpc_cb_req("req_cb_fn2_t1", "[ddd]", req_cb_fn2_t1, "foo", NULL));

	assert (brpc_cb_req_rem("req_cb_fn1_t1", "[ddd]"));
	assert (brpc_cb_req_rem("req_cb_fn1_t1", "[dcdd]"));
	assert (! brpc_cb_req_rem("req_cb_fn1_t1", "[ddd]"));
	assert (brpc_cb_req_rem("req_cb_fn2_t1", "[ddd]"));


	assert (brpc_cb_req("req_cb_fn1_t1", "[d  d d    ]", req_cb_fn1_t1, NULL,
		NULL));
	assert (! brpc_cb_req("req_cb_fn1_t1", "[ddd]", req_cb_fn2_t1, NULL,
		NULL));
	assert (! brpc_cb_req("req_cb_fn1_t2", "![cddd]", req_cb_fn2_t1, NULL,
		NULL));
	assert (! brpc_cb_req("req_cb_fn1_t2", "[c!ddd]", req_cb_fn2_t1, NULL,
		NULL));
	assert (brpc_cb_req("req_cb_fn1_t1", "[d  c d    ]", req_cb_fn2_t1, NULL,
		NULL));
	
	assert (! brpc_cb_req_rem("req_cb_fn1_t1", "[dcdd]"));
	assert (brpc_cb_req_rem("req_cb_fn1_t1", "[ddd]"));
	assert (brpc_cb_req_rem("req_cb_fn1_t1", "[dcd]"));
	
	
	assert (! brpc_cb_req("req_cb_fn1_t2", "[<<cddd]", req_cb_fn2_t1, NULL,
		NULL));
	assert (! brpc_cb_req("req_cb_fn1_t2", "<<<<<<<<<", req_cb_fn2_t1, NULL,
		NULL));
	assert (! brpc_cb_req("req_cb_fn1_t2", ">>>>>>>>>", req_cb_fn2_t1, NULL,
		NULL));

	brpc_cb_close();
}


brpc_t *req_cb_fn1_t2(brpc_t *req, void *_)
{
	int *a, *b, *c;
	brpc_int_t *d;
	void *params[4];

	brpc_dsm(req, "![ddd]i", params);
	a = (int *)params[0];
	b = (int *)params[1];
	c = (int *)params[2];
	d = (brpc_int_t *)params[3];

	assert(a); assert(*a == 1);
	assert(b); assert(*b == 22);
	assert(c); assert(*c == 333);
	assert(! d);
	
	return (brpc_t *)-1;
}

void test2()
{
	brpc_t *req;
	brpc_str_t method = {"req_cb_fn1_t2", sizeof("req_cb_fn1_t2")};

	brpc_cb_init(111, 11);
	assert(brpc_cb_req("req_cb_fn1_t2", "[ddd]i", req_cb_fn1_t2, NULL, NULL));
	
	assert((req = brpc_req(method, 155)));
	assert(brpc_asm(req, "[ddd]i", 1, 22, 333, NULL));
	assert(brpc_cb_run(req) == (brpc_t *)-1);

	brpc_finish(req);
	assert(brpc_cb_req_rem("req_cb_fn1_t2", "[ddd]i"));
	brpc_cb_close();
}

brpc_t *req_cb_fn1_t3(brpc_t *req, void *_)
{
	return (brpc_t *)-2;
}

void test3() //???
{
	brpc_str_t method = {"req_cb_fn1_t3", sizeof("req_cb_fn1_t3")};
	
	assert(brpc_cb_init(111, 11));

	assert(brpc_cb_req(method.val, "{<s[dd]><cd>}", req_cb_fn1_t3, NULL, 
		NULL));
	assert(brpc_cb_req_rem(method.val, "{<s[dd]><cd>}"));

	brpc_cb_close();
}

brpc_t *req_cb_fn1_t4(brpc_t *_, void *__)
{
	return (brpc_t *)4;
}

void test4()
{
	brpc_t *req;
	brpc_str_t method = {"req_cb_fn1_t4", sizeof("req_cb_fn1_t4")};
	
	assert(brpc_cb_init(111, 12));
	assert(brpc_cb_req(method.val, "", req_cb_fn1_t4, NULL, NULL));

	assert((req = brpc_req(method, 155)));
	assert(brpc_cb_run(req) == (brpc_t *)4);
	brpc_finish(req);

	assert(brpc_cb_req_rem(method.val, ""));

	brpc_cb_close();
}

void rpl_cb_fn1_t5(brpc_t *rpl, void *opaque)
{
	int *a, *b;
	assert(opaque == (void *)0x10101);
	a = b = 0;
	brpc_dsm(rpl, "dd", &a, &b);
	assert(a);
	assert(*a==-1);
	assert(b);
	assert(*b==-2);
}

void test5()
{
	brpc_t *req, *rpl;
	brpc_str_t method = {"rpl_cb_fn1_t5", sizeof("rpl_cb_fn1_t5")};
	
	assert(brpc_cb_init(111, 12));
	assert((req = brpc_req(method, 155)));

	assert(brpc_cb_rpl(req, rpl_cb_fn1_t5, (void *)0x10101));
	assert((rpl = brpc_rpl(req)));
	assert(brpc_asm(rpl, "dd", -1, -2));
	
	brpc_finish(req);
	assert(brpc_cb_run(rpl) == NULL);

	brpc_finish(rpl);
	brpc_cb_close();
}


void rpl_cb_fn1_t6(brpc_t *rpl, void *opaque)
{
	assert(rpl == NULL);
}

void test6()
{
	brpc_t *req, *rpl;
	brpc_str_t method = {"rpl_cb_fn1_t6", sizeof("rpl_cb_fn1_t6")};
	
	assert(brpc_cb_init(111, 12));
	assert((req = brpc_req(method, 155)));

	assert(brpc_cb_rpl(req, rpl_cb_fn1_t6, (void *)0x10101));
	assert((rpl = brpc_rpl(req)));
	assert(brpc_asm(rpl, "dd", -1, -2));
	
	assert(brpc_cb_rpl_cancel(req));
	assert(! brpc_cb_rpl_cancel(req));
	brpc_finish(req);
	assert(brpc_cb_run(rpl) == NULL);

	brpc_finish(rpl);
	brpc_cb_close();
}

int main()
{
#if 1
	test1(); printf("1: OK\n");
	test2(); printf("2: OK\n");
	test3(); printf("3: OK\n");
	test4(); printf("4: OK\n");
	test5(); printf("5: OK\n");
#endif
	test6(); printf("6: OK\n");
	return 0;
}
