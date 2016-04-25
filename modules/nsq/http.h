#ifndef __http_h
#define __http_h

#include <ev.h>
#include <curl/curl.h>

struct HttpClient {
    CURLM *multi;
    struct ev_loop *loop;
    struct ev_timer timer_event;
    int still_running;
};

struct HttpResponse {
    int status_code;
    struct Buffer *data;
};

struct HttpRequest {
    CURL *easy;
    char *url;
    struct HttpClient *httpc;
    char error[CURL_ERROR_SIZE];
    struct Buffer *data;
    void (*callback)(struct HttpRequest *req, struct HttpResponse *resp, void *arg);
    void *cb_arg;
};

struct HttpSocket {
    curl_socket_t sockfd;
    CURL *easy;
    int action;
    long timeout;
    struct ev_io ev;
    int evset;
    struct HttpClient *httpc;
};

struct HttpClient *new_http_client(struct ev_loop *loop);
void free_http_client(struct HttpClient *httpc);
struct HttpRequest *new_http_request(const char *url,
    void (*callback)(struct HttpRequest *req, struct HttpResponse *resp, void *arg), void *cb_arg, char *data);
void free_http_request(struct HttpRequest *req);
struct HttpResponse *new_http_response(int status_code, void *data);
void free_http_response(struct HttpResponse *resp);
int http_client_get(struct HttpClient *httpc, struct HttpRequest *req);

#endif
