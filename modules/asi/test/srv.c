#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>

#include <binrpc.h>

#define ERREXIT(msg, args...) \
	do { \
		fprintf(stderr, "ERROR (%d): " msg " (%s [%d]).\n", __LINE__, \
				strerror(errno), errno, ##args); \
		exit (-1); \
	} while (0)

const BRPC_STR_STATIC_INIT(ASIVer, "ASI_Version");
const BRPC_STR_STATIC_INIT(SetASId, "Set_AS_ID");
const BRPC_STR_STATIC_INIT(SIPMeth, "SIP_Methods");
const BRPC_STR_STATIC_INIT(DGSTFor, "Digest_For");
const BRPC_STR_STATIC_INIT(INVITE, "INVITE");
const BRPC_STR_STATIC_INIT(ACK, "ACK");

static int as_id = -1;
static int id_age;

brpc_t *asi_version(brpc_t *req)
{
	brpc_int_t *ver = NULL;
	brpc_t *rpl;
	brpc_val_t *val;

	assert(brpc_dsm(req, "d", &ver));
	assert(ver);
	assert(*ver == 1);

	assert(	(rpl = brpc_rpl(req)) &&
			(val = brpc_int(1)) &&
			brpc_add_val(rpl, val));
	return rpl;
}

brpc_t *set_as_id(brpc_t *req)
{
	int *loc_as_id;
	brpc_t *rpl;
	brpc_val_t *val;
	int old_age;

	assert(	brpc_dsm(req, "d", &loc_as_id) &&
			loc_as_id);
	assert(loc_as_id);
	if (as_id < 0) {
		as_id = *loc_as_id;
		id_age = (int)time(NULL);
		old_age = id_age;
	} else {
		old_age = id_age;
		if (as_id != *loc_as_id) {
			id_age = (int)time(NULL);
			printf("updating ID: previous=%d, new=%d; ts=%d.\n", 
					as_id, *loc_as_id, id_age);
			as_id = *loc_as_id;
		}
	}

	assert(	(rpl = brpc_rpl(req)) &&
			(val = brpc_int(old_age)) &&
			brpc_add_val(rpl, val));
	return rpl;
}

brpc_t *sip_methods(brpc_t *req)
{
	brpc_t *rpl = NULL;
	char *methods[] = {"INVITE", "ACK", "BYE", "OPTIONS", NULL};
	int i;

	assert(! brpc_val_cnt(req));
	assert((rpl = brpc_rpl(req)));
	for (i = 0; methods[i]; i ++)
		assert(brpc_asm(rpl, "c", methods[i]));
	return rpl;
}

brpc_t *digest_for(brpc_t *req)
{
	brpc_t *rpl = NULL;
	brpc_str_t *mname = NULL;

	assert(brpc_dsm(req, "s", &mname));
	assert(mname);

	assert((rpl = brpc_rpl(req)));
	assert(brpc_asm(rpl, "{ <c:[cccc dss]>, <c:[ccc]>, <c:[cc]> }", 
			"1", "%ru", "%ci", "$cucu", "@hf_value.contact[\"*\"]", -1, mname, NULL,
			"2", "%rs", "%rr", "%tt",
			"3", "%rs", "%rr"));
	return rpl;
}

brpc_t *invite(brpc_t *req)
{
	brpc_t *rpl;
	char buff[1024];
	brpc_print(req, buff, sizeof(buff));
	printf("request: %s.\n", buff);
	assert((rpl = brpc_rpl(req)));
	assert(brpc_asm(rpl, "dc", 100, "OK"));
	return rpl;
}

brpc_t *(*ack)(brpc_t *req) = invite;

static void logbrpc(int level, const char *fmt, ...)
{
	va_list ap;

#ifdef NODEBUG //NDEBUG cuts assert()'s out
	if (LOG_INFO < (level & 0x7))
		return;
#endif

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void *worker(void *param)
{
	pid_t tid;
	int cltfd;
	brpc_t *req, *rpl;
	
	cltfd = (int)param;
	tid = pthread_self();
	while ((req = brpc_recv(cltfd, 0))) {
		printf("[%u]: new request (%.*s) received.\n", tid,
				BRPC_STR_FMT(brpc_method(req)));
		assert((rpl = brpc_cb_run(req)));
		assert(brpc_send(cltfd, rpl, 30*1000));
		brpc_finish(req);
		brpc_finish(rpl);
	}
	printf("[%u]: connection to client closed.\n", tid);
	close(cltfd);
	return NULL;
}

int main(int argc, char **argv)
{
	char *uri;
	brpc_addr_t *addr, from;
	int sockfd, cltfd;
	pthread_t tid;

	if (argc < 2) {
		printf("USAGE: %s <uri>\n", argv[0]);
		return 1;
	} else {
		uri = argv[1];
		printf("Using URI: <%s>.\n", uri);
	}

	brpc_log_setup(logbrpc);

	if (! (addr = brpc_parse_uri(uri)))
		ERREXIT("failed to parse URI: %s [%d]", brpc_strerror(), brpc_errno);
	if (addr->socktype == SOCK_DGRAM)
		ERREXIT("will not use dgram sockets.\n");

	if (! brpc_cb_init(10, /*no req*/0))
		ERREXIT("failed to init callbacks: %s [%d]", brpc_strerror(), 
				brpc_errno);

	if (! (	brpc_cb_req(ASIVer.val, NULL, asi_version, NULL) &&
			brpc_cb_req(SetASId.val, NULL, set_as_id, NULL) &&
			brpc_cb_req(SIPMeth.val, NULL, sip_methods, NULL) &&
			brpc_cb_req(DGSTFor.val, NULL, digest_for, NULL) &&
			brpc_cb_req(INVITE.val, NULL, invite, NULL) &&
			brpc_cb_req(ACK.val, NULL, ack, NULL)))
		ERREXIT("failed to register REQ callbacks: %s [%d]", brpc_strerror(),
				brpc_errno);

	as_id = -1;
	id_age = (int)time(NULL);
	if ((sockfd = brpc_socket(addr, /*block*/true, /*bind*/true)) < 0)
		ERREXIT("failed to get listening socket: %s [%d]", brpc_strerror(), 
				brpc_errno);
	if ((listen(sockfd, 10)) < 0)
		ERREXIT("failed to listening on socket: %s [%d]", strerror(errno), 
				errno);

	while (true) {
		from.addrlen = sizeof(from.sockaddr);
		cltfd = accept(sockfd, (struct sockaddr *)&from.sockaddr, 
				&from.addrlen);
		if (cltfd < 0)
			ERREXIT("failed to accept client");
		assert(! pthread_create(&tid, NULL, &worker, (void *)cltfd));
		printf("[%lu]: new connection.\n", (unsigned long)tid);
	}

	close(sockfd);
	brpc_cb_close();
	return 0;
}
