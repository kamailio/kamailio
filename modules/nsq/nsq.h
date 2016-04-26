#ifndef __nsq_h
#define __nsq_h

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <ev.h>
#include <evbuffsock.h>

#include "utlist.h"

typedef enum {NSQ_FRAME_TYPE_RESPONSE, NSQ_FRAME_TYPE_ERROR, NSQ_FRAME_TYPE_MESSAGE} frame_type;
struct NSQDConnection;
struct NSQMessage;

struct NSQReader {
    char *topic;
    char *channel;
    void *ctx; //context for call back
    int max_in_flight;
    struct NSQDConnection *conns;
    struct NSQLookupdEndpoint *lookupd;
    struct ev_timer lookupd_poll_timer;
    struct ev_loop *loop;
    void *httpc;
    void (*connect_callback)(struct NSQReader *rdr, struct NSQDConnection *conn);
    void (*close_callback)(struct NSQReader *rdr, struct NSQDConnection *conn);
    void (*msg_callback)(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx);
};

struct NSQReader *new_nsq_reader(struct ev_loop *loop, const char *topic, const char *channel, void *ctx,
    void (*connect_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*close_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*msg_callback)(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx));
void free_nsq_reader(struct NSQReader *rdr);
int nsq_reader_connect_to_nsqd(struct NSQReader *rdr, const char *address, int port);
int nsq_reader_add_nsqlookupd_endpoint(struct NSQReader *rdr, const char *address, int port);
void nsq_reader_set_loop(struct NSQReader *rdr, struct ev_loop *loop);
void nsq_run(struct ev_loop *loop);

struct NSQDConnection {
    struct BufferedSocket *bs;
    struct Buffer *command_buf;
    uint32_t current_msg_size;
    uint32_t current_frame_type;
    char *current_data;
    struct ev_loop *loop;
    void (*connect_callback)(struct NSQDConnection *conn, void *arg);
    void (*close_callback)(struct NSQDConnection *conn, void *arg);
    void (*msg_callback)(struct NSQDConnection *conn, struct NSQMessage *msg, void *arg);
    void *arg;
    struct NSQDConnection *next;
};

struct NSQDConnection *new_nsqd_connection(struct ev_loop *loop, const char *address, int port,
    void (*connect_callback)(struct NSQDConnection *conn, void *arg),
    void (*close_callback)(struct NSQDConnection *conn, void *arg),
    void (*msg_callback)(struct NSQDConnection *conn, struct NSQMessage *msg, void *arg),
    void *arg);
void free_nsqd_connection(struct NSQDConnection *conn);
int nsqd_connection_connect(struct NSQDConnection *conn);
void nsqd_connection_disconnect(struct NSQDConnection *conn);

void nsq_subscribe(struct Buffer *buf, const char *topic, const char *channel);
void nsq_ready(struct Buffer *buf, int count);
void nsq_finish(struct Buffer *buf, const char *id);
void nsq_requeue(struct Buffer *buf, const char *id, int timeout_ms);
void nsq_nop(struct Buffer *buf);

struct NSQMessage {
    int64_t timestamp;
    uint16_t attempts;
    char id[16+1];
    size_t body_length;
    char *body;
};

struct NSQMessage *nsq_decode_message(const char *data, size_t data_length);
void free_nsq_message(struct NSQMessage *msg);

struct NSQLookupdEndpoint {
    char *address;
    int port;
    struct NSQLookupdEndpoint *next;
};

struct NSQLookupdEndpoint *new_nsqlookupd_endpoint(const char *address, int port);
void free_nsqlookupd_endpoint(struct NSQLookupdEndpoint *nsqlookupd_endpoint);

#endif
