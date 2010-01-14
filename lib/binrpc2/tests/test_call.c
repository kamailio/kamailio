
#include <stdlib.h>
#include <stdio.h>
#include <mcheck.h>
#include <assert.h>

#define BINRPC_NO_TLS
#include <binrpc.h>

#define STDDEF \
	uint8_t *clone; \
	const brpc_bin_t *sbuff; \
	size_t len; \
	brpc_t *req, *rcv; \
	BRPC_STR_STATIC_INIT(method, "fcall"); \
	const brpc_str_t *mcall;

#define NETSIM(_msg_) \
	do { \
		assert((sbuff = brpc_serialize(_msg_))); \
		dupbuff(sbuff, &clone, &len); \
		brpc_finish(_msg_); \
		dumpbuff(clone, len); \
		assert((rcv = brpc_raw(clone, len))); \
		assert(brpc_unpack(rcv)); \
		if (brpc_type(rcv) == BRPC_CALL_REQUEST) { \
			assert((mcall = brpc_method(rcv))); \
			assert(mcall->len == method.len); \
			assert(! strncmp(mcall->val, method.val, method.len)); \
		} \
	} while (0)

void dupbuff(const brpc_bin_t *orig, uint8_t **replica, size_t *len)
{
	*replica = (uint8_t *)malloc(orig->len * sizeof(uint8_t));
	assert(*replica);
	memcpy(*replica, orig->val, orig->len);
	*len = orig->len;
}

void dumpbuff(uint8_t *buff, size_t len)
{
	int i;
	printf("Buffer [%u]:\n", len);
	for (i = 0; i < len; i ++)
		printf("0x%.2x ", buff[i]);
	printf("\n");

}

void test1(void)
{
	brpc_val_t *name, *value, *map, *list;
	brpc_val_t *val, *val1, *val2, *val3;
	brpc_val_t *avp, *avp1, *avp2;
	size_t vno;
	STDDEF;


	assert((req = brpc_req(method, 55)));
	vno = 0;

	assert((val = brpc_int(10)));
	assert(brpc_add_val(req, val)); vno ++;

	assert((val = brpc_cstr("cucu")));
	assert(brpc_add_val(req, val)); vno ++;
	assert((name = brpc_cstr("nnne")));
	assert((value = brpc_int(11)));
	assert((avp = brpc_avp(name, value)));
	assert(brpc_add_val(req, avp)); vno ++;

	assert((list = brpc_list(NULL)));

	assert((val = brpc_cstr("XXX")));
	assert(brpc_list_add(list, val));
	assert((val = brpc_int(23)));
	assert(brpc_list_add(list, val));
	assert(brpc_add_val(req, list)); vno ++;

	assert((name = brpc_int(44)));
	assert((value = brpc_int(22)));
	assert((avp = brpc_avp(name, value)));
	assert(brpc_add_val(req, avp)); vno ++;

	assert((map = brpc_map(NULL)));
	assert((name = brpc_cstr("cucu")));
	assert((value = brpc_bin((uint8_t *)"bubu", sizeof("bubu"))));
	assert((avp = brpc_avp(name, value)));
	assert(brpc_map_add(map, avp));
	assert((name = brpc_cstr("dumum")));
	assert((list = brpc_list(NULL)));
	assert((val = brpc_int(-11111)));
	assert(brpc_list_add(list, val));
	assert((val = brpc_cstr("miau")));
	assert(brpc_list_add(list, val));
	assert((val = brpc_bin((uint8_t *)"cucucu", sizeof("cucucu"))));
	assert(brpc_list_add(list, val));
	assert((avp = brpc_avp(name, list)));
	assert(brpc_map_add(map, avp));
	//brpc_add_val(req, map); vno ++; //fail test
	
	assert((val1 = brpc_int(-11111)));
	assert((val2 = brpc_cstr("cucu")));
	assert((val3 = brpc_bin((uint8_t *)"s", 1)));
	assert((val = brpc_list(val1, val2, val3, NULL)));
	assert(brpc_add_val(req, val)); vno ++;

	assert((avp1 = brpc_avp(brpc_int(22), brpc_int(33))));
	assert((avp2 = brpc_avp(brpc_int(22), brpc_int(33))));
	assert(brpc_add_val(req, brpc_map(avp1, avp2, NULL)));

	assert((name = brpc_int(12)));
	assert((value = brpc_cstr("xyxyxxx")));
	assert((avp = brpc_avp(name, value)));
	assert((name = brpc_cstr("oneavp")));
	assert((avp = brpc_avp(name, avp)));
	assert(brpc_map_add(map, avp));

	assert((list = brpc_list(NULL)));
	assert(brpc_list_add(list, map));
	assert(brpc_add_val(req, list)); vno ++;

	assert((val = brpc_null(BRPC_VAL_BIN)));
	assert(brpc_add_val(req, val)); vno ++;

	assert((val = brpc_bin((uint8_t *)"", 0)));
	assert(brpc_add_val(req, val)); vno ++;

	NETSIM(req);

	assert(brpc_type(rcv) == BRPC_CALL_REQUEST);
	brpc_finish(rcv);
}

void test2()
{
	STDDEF;

	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "dcd <c<cd>> [ddd] {<cd><dd>} {<dd><c[ddd<cc>]>}", 
			33, "ana are mere", -22, 
			"ab", "ba", 34,
			4, 5, 6,
			"c", 1, 2, 3,
			44, 45, "cucu", 0, 0, 0, "a", NULL));

	NETSIM(req);

	brpc_finish(rcv);
}

void test3()
{
#define C1	"c-1"
#define C2	"c-2"
#define C3	"c-3"
#define C4	"c-4"
#define D1	0xd1
#define D2	0xd2
	char *ci1, *co1;
	char *ci2, *co2;
	char *ci3, *co3;
	char *ci4, *co4;
	int di1, *do1;
	int di2, *do2;
	STDDEF;

	assert((req = brpc_req(method, 55)));

	ci1 = C1;
	ci2 = C2;
	ci3 = C3;
	ci4 = C4;
	di1 = D1;
	di2 = D2;

	assert(brpc_asm(req, "cdc <cc> d", 
			ci1, di1, ci2, 
			ci3, ci4,
			di2));

	NETSIM(req);

	assert((brpc_dsm(rcv, "cdc <cc> d",
			&co1, &do1, &co2,
			&co3, &co4,
			&do2)));

	assert(! strcmp(co1, C1));
	assert(! strcmp(co2, C2));
	assert(! strcmp(co3, C3));
	assert(! strcmp(co4, C4));
	assert(*do1 == D1);
	assert(*do2 == D2);

	brpc_finish(rcv);
}

void test4()
{
	char Ci[10 * 4], *ci[10], *co[10], *cci, *cco;
	int di[5], *Do[5];
	int i;
	STDDEF;

	for (i = 0; i < 10; i ++) {
		sprintf(&Ci[i * 4], "C-%d", i);
		ci[i] = &Ci[i * 4];
	}
	for (i = 0; i < 5; i ++)
		di[i] = 0xd0 + i;
	
	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "c<c<c<c<c<c<c<c[dd[d]{<cd>}]>>>>>>>dc", 
			ci[0], ci[1], ci[2], ci[3], ci[4], ci[5], ci[6], ci[7],
			di[0], di[1], di[2], ci[8], di[3],
			di[4], ci[9]));

	NETSIM(req);

	assert(brpc_dsm(rcv, "c<c<c<c<c<c<c<c[dd[d]{<cd>}]>>>>>>>dc",
			&co[0], &co[1], &co[2], &co[3], &co[4], &co[5], &co[6], &co[7],
			&Do[0], &Do[1], &Do[2], &co[8], &Do[3],
			&Do[4], &co[9]));

	for (i = 0; i < 10; i ++) {
		cci = ci[i];
		cco = co[i];
		assert(strlen(cci) == strlen(cco));
		assert(! strcmp(cci, cco));
	}
	for (i = 0; i < 5; i ++)
		assert(di[i] = *Do[i]);

	brpc_finish(rcv);
}

void test5()
{
	char Ci[10 * 4], *ci[10], *co[10], *cci, *cco;
	int di[5], *Do[5];
	int i;
	STDDEF;

	for (i = 0; i < 10; i ++) {
		sprintf(&Ci[i * 4], "C-%d", i);
		ci[i] = &Ci[i * 4];
	}
	for (i = 0; i < 5; i ++)
		di[i] = 0xd0 + i;
	
	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "c<c<c<c<c<c<c<c[dd[d]dc{<cd>}]>>>>>>>",
			ci[0], ci[1], ci[2], ci[3], ci[4], ci[5], ci[6], ci[7],
			di[0], di[1], di[2], di[3], ci[8], ci[9], di[4]));

	NETSIM(req);

	assert(brpc_dsm(rcv, "c<c<c<c<c<c<c<c[dd[d]dc{<cd>}]>>>>>>>",
			&co[0], &co[1], &co[2], &co[3], &co[4], &co[5], &co[6], &co[7],
			&Do[0], &Do[1], &Do[2], &Do[3], &co[8], &co[9], &Do[4]));

	for (i = 0; i < 10; i ++) {
		cci = ci[i];
		cco = co[i];
		assert(strlen(cci) == strlen(cco));
		assert(! strcmp(cci, cco));
	}
	for (i = 0; i < 5; i ++)
		assert(di[i] = *Do[i]);

	brpc_finish(rcv);
}

void test6()
{
	int di = 43, *od;
	STDDEF;
	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "[[[[[[[d]]]]]]]", di));
	NETSIM(req);
	assert(brpc_dsm(rcv, "[[[[[[[d]]]]]]]", &od));
	assert(od);
	assert(di == *od);
	brpc_finish(rcv);
}

void test7()
{
	brpc_int_t *di = NULL, *od;
	STDDEF;
	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "[[[[[[[i]]]]]]]", di));
	NETSIM(req);
	assert(brpc_dsm(rcv, "[[[[[[[i]]]]]]]", &od));
	assert(di == od);
	brpc_finish(rcv);
}

void test8()
{
	brpc_int_t *di = NULL;
	char *ci = "foo", *co;
	STDDEF;
	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "[[[[[[[ic]]]]]]]", di, ci));
	NETSIM(req);
	assert(brpc_dsm(rcv, "[[[[[[[.c]]]]]]]", &co));
	assert(! strcmp(ci, co));
	brpc_finish(rcv);
}

void test9()
{
	brpc_int_t *di = NULL, *od1, *od2 = (brpc_int_t *)0x1;
	char *ci = "foo", *co;
	STDDEF;
	assert((req = brpc_req(method, 55)));
	assert(brpc_asm(req, "[[[ [[    [ic]iicii    ]] idi ]]]", di, ci, 
			di, di, ci, di, di, 
			di, 10, di));
	NETSIM(req);
	assert(brpc_dsm(rcv, "[[[ [[    ...c*    ]] .di ]]]", &co, &od1, 
			&od2));
	assert(! strcmp(ci, co));
	assert(*od1 == 10);
	assert(! od2);
	brpc_finish(rcv);
}

void test10()
{
	STDDEF;
	assert((req = brpc_req(method, 55)));
	NETSIM(req);
	brpc_finish(rcv);
}

void test11()
{
	brpc_int_t *di = NULL, *od;
	char *desc, *repr;
	STDDEF;
	assert((req = brpc_req(method, 55)));
	repr = "[[[[[[[i]]]]]]]";
	assert(brpc_asm(req, repr, di));
	NETSIM(req);
	
	desc = brpc_repr(rcv, NULL); //TODO: check len
	assert(strlen(repr) == strlen(desc));
	assert(strcmp(repr, desc) == 0);
	
	assert(brpc_dsm(rcv, desc, &od));
	free(desc);
	assert(di == od);
	brpc_finish(rcv);
}

void test12()
{
	char Ci[10 * 4], *ci[10], *co[10], *cci, *cco;
	int di[5], *Do[5];
	char *repr, *indep, *desc;
	int i;
	STDDEF;

	for (i = 0; i < 10; i ++) {
		sprintf(&Ci[i * 4], "C-%d", i);
		ci[i] = &Ci[i * 4];
	}
	for (i = 0; i < 5; i ++)
		di[i] = 0xd0 + i;
	
	assert((req = brpc_req(method, 55)));
	repr = "c<c<c<c<c<c<c<c[dd[d]dc{<cd>}]>>>>>>>";
	indep= "s<s<s<s<s<s<s<s[ii[i]is{<si>}]>>>>>>>";
	assert(brpc_asm(req, repr,
			ci[0], ci[1], ci[2], ci[3], ci[4], ci[5], ci[6], ci[7],
			di[0], di[1], di[2], di[3], ci[8], ci[9], di[4]));

	NETSIM(req);

	desc = brpc_repr(rcv, NULL); //CHECK LEN
	assert(strlen(indep) == strlen(desc));
	assert(strcmp(indep, desc) == 0);
	free(desc);

	assert(brpc_dsm(rcv, "c<c<c<c<c<c<c<c[dd[d]dc{<cd>}]>>>>>>>",
			&co[0], &co[1], &co[2], &co[3], &co[4], &co[5], &co[6], &co[7],
			&Do[0], &Do[1], &Do[2], &Do[3], &co[8], &co[9], &Do[4]));

	for (i = 0; i < 10; i ++) {
		cci = ci[i];
		cco = co[i];
		assert(strlen(cci) == strlen(cco));
		assert(! strcmp(cci, cco));
	}
	for (i = 0; i < 5; i ++)
		assert(di[i] = *Do[i]);

	brpc_finish(rcv);
}

void test13()
{
	char Ci[10 * 4], *ci[10], *co[10], *cci, *cco;
	int di[5], *Do[5];
	char *repr;
	int i, k;
	void *vi[15], *vo[15];
	STDDEF;

	for (i = 0; i < 10; i ++) {
		sprintf(&Ci[i * 4], "C-%d", i);
		ci[i] = &Ci[i * 4];
	}
	for (i = 0; i < 5; i ++)
		di[i] = 0xd0 + i;
	
	for (i = 1; i < 8; i ++)
		vi[i] = (void *)ci[i];
	for (k=0; k < 4; k ++)
		vi[i+k] = (void *)di[k];
	for (; i < 10; i ++)
		vi[i+k] = (void *)ci[i];
	vi[i+k] = (void *)di[k];

	assert((req = brpc_req(method, 55)));
	repr = "c!<c<c<c<c<c<c<c[dd[d]dc{<cd>}]>>>>>>>";
	assert(brpc_asm(req, repr, ci[0], &vi[1]));

	NETSIM(req);

	for (i = 1; i < 8; i ++)
		vo[i] = (void *)&co[i];
	for (k=0; k < 4; k ++)
		vo[i+k] = (void *)&Do[k];
	for (; i < 10; i ++)
		vo[i+k] = (void *)&co[i];
	vo[i+k] = (void *)&Do[k];
	
	repr = "c&<c<c<c<c<c<c<c[dd[d]dc{<cd>}]>>>>>>>";
	assert(brpc_dsm(rcv, repr, &co[0], &vo[1]));

	for (i = 0; i < 10; i ++) {
		cci = ci[i];
		cco = co[i];
		assert(strlen(cci) == strlen(cco));
		assert(! strcmp(cci, cco));
	}
	for (i = 0; i < 5; i ++)
		assert(di[i] = *Do[i]);

	brpc_finish(rcv);
}

void test14()
{
	char *c_null, *cucu, *bau;
	int *fiftyfour;
	brpc_int_t *i_null;
	void *params[] = {&c_null, &cucu, &fiftyfour, &bau, &i_null};
	STDDEF;
	assert((req = brpc_req(method, 55)));

	assert(brpc_asm(req, "[ccdci]", NULL, "CUCU", 54, "BAU", NULL));
	NETSIM(req);
	assert(brpc_dsm(rcv, "&[ccdci]", params));

	assert(c_null == NULL);
	assert(strcmp(cucu, "CUCU") == 0);
	assert(*fiftyfour == 54);
	assert(strcmp(bau, "BAU") == 0);
	assert(i_null == NULL);

	brpc_finish(rcv);
}

void test15()
{
	char *c_null, *cucu, *bau;
	int fiftyfour;
	brpc_int_t *i_null;
	void *params[5];
	STDDEF;
	assert((req = brpc_req(method, 55)));

	c_null = NULL;
	cucu = "CUCU";
	fiftyfour = 54;
	bau = "BAU";
	i_null = NULL;


	assert(brpc_asm(req, "[ccdci]", c_null, cucu, fiftyfour, bau, i_null));
	NETSIM(req);
	assert(brpc_dsm(rcv, "![ccdci]", params));

	assert((char *)params[0] == c_null);
	assert(strcmp((char *)params[1], cucu) == 0);
	assert(*(int *)params[2] == 54);
	assert(strcmp((char *)params[3], bau) == 0);
	assert((brpc_int_t *)params[4] == i_null);

	brpc_finish(rcv);
}

void test16()
{
	char *c_null, *cucu, *bau;
	int *fiftyfour;
	brpc_int_t *i_null;
	void *params[] = {&c_null, &cucu, &fiftyfour, &bau, &i_null};
	void *params2[3];
	STDDEF;
	assert((req = brpc_req(method, 55)));

	assert(brpc_asm(req, "[ccdci] ss ddi", NULL, "CUCU", 54, "BAU", NULL, 
			NULL, NULL, 1, 22, NULL));
	NETSIM(req);
	assert(brpc_dsm(rcv, "&[ccdci] .. !ddi", params, params2));

	assert(c_null == NULL);
	assert(strcmp(cucu, "CUCU") == 0);
	assert(*fiftyfour == 54);
	assert(strcmp(bau, "BAU") == 0);
	assert(i_null == NULL);

	assert(*(int *)params2[0] == 1);
	assert(*(int *)params2[1] == 22);
	assert((int *)params2[2] == NULL);

	brpc_finish(rcv);
}

void test17()
{
	STDDEF;
	brpc_t *rpl;
	int *a, *c;
	char *b;

	assert((req = brpc_req(method, 555)));
	assert((rpl = brpc_rpl(req)));
	brpc_finish(req);
	assert(brpc_asm(rpl, "dcd <c<cd>> [ddd] {<cd><dd>} {<dd><c[ddd<cc>]>}", 
			33, "ana are mere", -22, 
			"ab", "ba", 34,
			4, 5, 6,
			"c", 1, 2, 3,
			44, 45, "cucu", 0, 0, 0, "a", NULL));

	NETSIM(rpl);

	a = c = 0;
	b = 0;
	assert(brpc_dsm(rcv, "dcd *", &a, &b, &c));
	assert(a);
	assert(*a == 33);
	assert(b);
	assert(strcmp(b, "ana are mere") == 0);
	assert(c);
	assert(*c == -22);


	brpc_finish(rcv);
}

int main()
{
	mtrace ();

#if 1
	test1(); printf("1: OK\n");
	test2(); printf("2: OK\n");
	test3(); printf("3: OK\n");
	test4(); printf("4: OK\n");
	test5(); printf("5: OK\n");
	test6(); printf("6: OK\n");
	test7(); printf("7: OK\n");
	test8(); printf("8: OK\n");
	test9(); printf("9: OK\n");
	test10(); printf("10: OK\n");
	test11(); printf("11: OK\n");
	test12(); printf("12: OK\n");
	test13(); printf("13: OK\n");
	test14(); printf("14: OK\n");
	test15(); printf("15: OK\n");
	test16(); printf("16: OK\n");
	test17(); printf("17: OK\n");
#endif
	
	assert(brpc_errno == 0);

	return 0;
}
