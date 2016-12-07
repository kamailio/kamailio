/* Copyright information is at the end of the file */

#include <xmlrpc-c/config.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#ifdef _WIN32
#  include <io.h>
#else
#  include <signal.h>
#  include <sys/wait.h>
#  include <grp.h>
#endif

#include "abyss_mallocvar.h"
#include <xmlrpc-c/abyss.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include "abyss_xmlrpc_int.h"
#include <xmlrpc-c/server_abyss.h>


/*=========================================================================
**  die_if_fault_occurred
**=========================================================================
**  If certain kinds of out-of-memory errors occur during server setup,
**  we want to quit and print an error.
*/

static void die_if_fault_occurred(xmlrpc_env *env) {
    if (env->fault_occurred) {
        fprintf(stderr, "Unexpected XML-RPC fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}



static void
addAuthCookie(xmlrpc_env * const envP,
              TSession *   const abyssSessionP,
              const char * const authCookie) {

    const char * cookieResponse;
    
    xmlrpc_asprintf(&cookieResponse, "auth=%s", authCookie);
    
    if (cookieResponse == xmlrpc_strsol)
        xmlrpc_faultf(envP, "Insufficient memory to generate cookie "
                      "response header.");
    else {
        ResponseAddField(abyssSessionP, "Set-Cookie", cookieResponse);
    
        xmlrpc_strfree(cookieResponse);
    }
}   
    


static void 
sendXmlData(xmlrpc_env * const envP,
            TSession *   const abyssSessionP, 
            const char * const body, 
            size_t       const len,
            bool         const chunked) {
/*----------------------------------------------------------------------------
   Generate an HTTP response containing body 'body' of length 'len'
   characters.

   This is meant to run in the context of an Abyss URI handler for
   Abyss session 'abyssSessionP'.
-----------------------------------------------------------------------------*/
    const char * http_cookie = NULL;
        /* This used to set http_cookie to getenv("HTTP_COOKIE"), but
           that doesn't make any sense -- environment variables are not
           appropriate for this.  So for now, cookie code is disabled.
           - Bryan 2004.10.03.
        */

    /* Various bugs before Xmlrpc-c 1.05 caused the response to be not
       chunked in the most basic case, but chunked if the client explicitly
       requested keepalive.  I think it's better not to chunk, because
       it's simpler, so I removed this in 1.05.  I don't know what the
       purpose of chunking would be, and an original comment suggests
       the author wasn't sure chunking was a good idea.

       In 1.06 we added the user option to chunk.
    */
    if (chunked)
        ResponseChunked(abyssSessionP);

    ResponseStatus(abyssSessionP, 200);

    if (http_cookie)
        /* There's an auth cookie, so pass it back in the response. */
        addAuthCookie(envP, abyssSessionP, http_cookie);

    if ((size_t)(uint32_t)len != len)
        xmlrpc_faultf(envP, "XML-RPC method generated a response too "
                      "large for Abyss to send");
    else {
        uint32_t const abyssLen = (uint32_t)len;

        ResponseContentType(abyssSessionP, "text/xml; charset=\"utf-8\"");
        ResponseContentLength(abyssSessionP, abyssLen);
        
        ResponseWriteStart(abyssSessionP);
        ResponseWriteBody(abyssSessionP, body, abyssLen);
        ResponseWriteEnd(abyssSessionP);
    }
}



static void
sendError(TSession *   const abyssSessionP, 
          unsigned int const status) {
/*----------------------------------------------------------------------------
  Send an error response back to the client.
   
-----------------------------------------------------------------------------*/
    ResponseStatus(abyssSessionP, (uint16_t) status);
    ResponseError(abyssSessionP);
}



static void
traceChunkRead(TSession * const abyssSessionP) {

    fprintf(stderr, "XML-RPC handler got a chunk of %u bytes\n",
            (unsigned int)SessionReadDataAvail(abyssSessionP));
}



static void
refillBufferFromConnection(xmlrpc_env * const envP,
                           TSession *   const abyssSessionP,
                           const char * const trace) {
/*----------------------------------------------------------------------------
   Get the next chunk of data from the connection into the buffer.
-----------------------------------------------------------------------------*/
    abyss_bool succeeded;

    succeeded = SessionRefillBuffer(abyssSessionP);

    if (!succeeded)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TIMEOUT_ERROR, "Timed out waiting for "
            "client to send its POST data");
    else {
        if (trace)
            traceChunkRead(abyssSessionP);
    }
}



static void
getBody(xmlrpc_env *        const envP,
        TSession *          const abyssSessionP,
        size_t              const contentSize,
        const char *        const trace,
        xmlrpc_mem_block ** const bodyP) {
/*----------------------------------------------------------------------------
   Get the entire body, which is of size 'contentSize' bytes, from the
   Abyss session and return it as the new memblock *bodyP.

   The first chunk of the body may already be in Abyss's buffer.  We
   retrieve that before reading more.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * body;

    if (trace)
        fprintf(stderr, "XML-RPC handler processing body.  "
                "Content Size = %u bytes\n", (unsigned)contentSize);

    body = xmlrpc_mem_block_new(envP, 0);
    if (!envP->fault_occurred) {
        size_t bytesRead;
        const char * chunkPtr;
        size_t chunkLen;

        bytesRead = 0;

        while (!envP->fault_occurred && bytesRead < contentSize) {
            SessionGetReadData(abyssSessionP, contentSize - bytesRead, 
                               &chunkPtr, &chunkLen);
            bytesRead += chunkLen;

            assert(bytesRead <= contentSize);

            XMLRPC_MEMBLOCK_APPEND(char, envP, body, chunkPtr, chunkLen);
            if (bytesRead < contentSize)
                refillBufferFromConnection(envP, abyssSessionP, trace);
        }
        if (envP->fault_occurred)
            xmlrpc_mem_block_free(body);
        else
            *bodyP = body;
    }
}



static void
storeCookies(TSession *     const httpRequestP,
             unsigned int * const httpErrorP) {
/*----------------------------------------------------------------------------
   Get the cookie settings from the HTTP headers and remember them for
   use in responses.
-----------------------------------------------------------------------------*/
    const char * const cookie = RequestHeaderValue(httpRequestP, "cookie");
    if (cookie) {
        /* 
           Setting the value in an environment variable doesn't make
           any sense.  So for now, cookie code is disabled.
           -Bryan 04.10.03.

        setenv("HTTP_COOKIE", cookie, 1);
        */
    }
    /* TODO: parse HTTP_COOKIE to find auth pair, if there is one */

    *httpErrorP = 0;
}




static void
validateContentType(TSession *     const httpRequestP,
                    unsigned int * const httpErrorP) {
/*----------------------------------------------------------------------------
   If the client didn't specify a content-type of "text/xml", return      
   "400 Bad Request".  We can't allow the client to default this header,
   because some firewall software may rely on all XML-RPC requests
   using the POST method and a content-type of "text/xml". 
-----------------------------------------------------------------------------*/
    const char * const content_type =
        RequestHeaderValue(httpRequestP, "content-type");

    if (content_type == NULL)
        *httpErrorP = 400;
    else {
        const char * const sempos = strchr(content_type, ';');
        unsigned int baselen;
            /* Length of the base portion of the content type, e.g.
               "text/xml" int "text/xml;charset=utf-8"
            */

        if (sempos)
            baselen = sempos - content_type;
        else
            baselen = strlen(content_type);

        if (!xmlrpc_strneq(content_type, "text/xml", baselen))
            *httpErrorP = 400;
        else
            *httpErrorP = 0;
    }
}



static void
processContentLength(TSession *     const httpRequestP,
                     size_t *       const inputLenP,
                     unsigned int * const httpErrorP) {
/*----------------------------------------------------------------------------
  Make sure the content length is present and non-zero.  This is
  technically required by XML-RPC, but we only enforce it because we
  don't want to figure out how to safely handle HTTP < 1.1 requests
  without it.  If the length is missing, return "411 Length Required". 
-----------------------------------------------------------------------------*/
    const char * const content_length = 
        RequestHeaderValue(httpRequestP, "content-length");

    if (content_length == NULL)
        *httpErrorP = 411;
    else {
        if (content_length[0] == '\0')
            *httpErrorP = 400;
        else {
            unsigned long contentLengthValue;
            char * tail;
        
            contentLengthValue = strtoul(content_length, &tail, 10);
        
            if (*tail != '\0')
                /* There's non-numeric crap in the length */
                *httpErrorP = 400;
            else if (contentLengthValue < 1)
                *httpErrorP = 400;
            else if ((unsigned long)(size_t)contentLengthValue 
                     != contentLengthValue)
                *httpErrorP = 400;
            else {
                *httpErrorP = 0;
                *inputLenP = (size_t)contentLengthValue;
            }
        }
    }
}



static void
traceHandlerCalled(TSession * const abyssSessionP) {
    
    const char * methodDesc;
    const TRequestInfo * requestInfoP;

    fprintf(stderr, "xmlrpc_server_abyss URI path handler called.\n");

    SessionGetRequestInfo(abyssSessionP, &requestInfoP);

    fprintf(stderr, "URI = '%s'\n", requestInfoP->uri);

    switch (requestInfoP->method) {
    case m_unknown: methodDesc = "unknown";   break;
    case m_get:     methodDesc = "get";       break;
    case m_put:     methodDesc = "put";       break;
    case m_head:    methodDesc = "head";      break;
    case m_post:    methodDesc = "post";      break;
    case m_delete:  methodDesc = "delete";    break;
    case m_trace:   methodDesc = "trace";     break;
    case m_options: methodDesc = "m_options"; break;
    default:        methodDesc = "?";
    }
    fprintf(stderr, "HTTP method = '%s'\n", methodDesc);

    if (requestInfoP->query)
        fprintf(stderr, "query (component of URL)='%s'\n",
                requestInfoP->query);
    else
        fprintf(stderr, "URL has no query component\n");
}



static void
processCall(TSession *        const abyssSessionP,
            size_t            const contentSize,
            xmlrpc_registry * const registryP,
            bool              const wantChunk,
            const char *      const trace) {
/*----------------------------------------------------------------------------
   Handle an RPC request.  This is an HTTP request that has the proper form
   to be one of our RPCs.

   Its content length is 'contentSize' bytes.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    if (trace)
        fprintf(stderr,
                "xmlrpc_server_abyss URI path handler processing RPC.\n");

    xmlrpc_env_init(&env);

    if (contentSize > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        xmlrpc_env_set_fault_formatted(
            &env, XMLRPC_LIMIT_EXCEEDED_ERROR,
            "XML-RPC request too large (%ld bytes)", (long)contentSize);
    else {
        xmlrpc_mem_block *body=0;
        /* Read XML data off the wire. */
        getBody(&env, abyssSessionP, contentSize, trace, &body);
        if (!env.fault_occurred) {
            xmlrpc_mem_block * output;
            /* Process the RPC. */
            output = xmlrpc_registry_process_call(
                &env, registryP, NULL, 
                XMLRPC_MEMBLOCK_CONTENTS(char, body),
                XMLRPC_MEMBLOCK_SIZE(char, body));
            if (!env.fault_occurred) {
                /* Send out the result. */
                sendXmlData(&env, abyssSessionP, 
                            XMLRPC_MEMBLOCK_CONTENTS(char, output),
                            XMLRPC_MEMBLOCK_SIZE(char, output),
                            wantChunk);
                
                XMLRPC_MEMBLOCK_FREE(char, output);
            }
            XMLRPC_MEMBLOCK_FREE(char, body);
        }
    }
    if (env.fault_occurred) {
        if (env.fault_code == XMLRPC_TIMEOUT_ERROR)
            sendError(abyssSessionP, 408); /* 408 Request Timeout */
        else
            sendError(abyssSessionP, 500); /* 500 Internal Server Error */
    }

    xmlrpc_env_clean(&env);
}



/****************************************************************************
    Abyss handlers (to be registered with and called by Abyss)
****************************************************************************/

static const char * trace_abyss;



struct uriHandlerXmlrpc {
/*----------------------------------------------------------------------------
   This is the part of an Abyss HTTP request handler (aka URI handler)
   that is specific to the Xmlrpc-c handler.
-----------------------------------------------------------------------------*/
    xmlrpc_registry * registryP;
    const char *      uriPath;  /* malloc'ed */
    bool              chunkResponse;
        /* The handler should chunk its response whenever possible */
};



static void
termUriHandler(void * const arg) {

    struct uriHandlerXmlrpc * const uriHandlerXmlrpcP = arg;

    xmlrpc_strfree(uriHandlerXmlrpcP->uriPath);
    free(uriHandlerXmlrpcP);
}



static void
handleXmlrpcReq(URIHandler2 * const this,
                TSession *    const abyssSessionP,
                abyss_bool *  const handledP) {
/*----------------------------------------------------------------------------
   Our job is to look at this HTTP request that the Abyss server is
   trying to process and see if we can handle it.  If it's an XML-RPC
   call for this XML-RPC server, we handle it.  If it's not, we refuse
   it and Abyss can try some other handler.

   Our return code is TRUE to mean we handled it; FALSE to mean we didn't.

   Note that failing the request counts as handling it, and not handling
   it does not mean we failed it.

   This is an Abyss HTTP Request handler -- type URIHandler2.
-----------------------------------------------------------------------------*/
    struct uriHandlerXmlrpc * const uriHandlerXmlrpcP = this->userdata;

    const TRequestInfo * requestInfoP;

    if (trace_abyss)
        traceHandlerCalled(abyssSessionP);

    SessionGetRequestInfo(abyssSessionP, &requestInfoP);

    /* Note that requestInfoP->uri is not the whole URI.  It is just
       the "file name" part of it.
    */
    if (strcmp(requestInfoP->uri, uriHandlerXmlrpcP->uriPath) != 0)
        /* It's for the path (e.g. "/RPC2") that we're supposed to
           handle.
        */
        *handledP = FALSE;
    else {
        *handledP = TRUE;

        /* We understand only the POST HTTP method.  For anything else, return
           "405 Method Not Allowed". 
        */
        if (requestInfoP->method != m_post)
            sendError(abyssSessionP, 405);
        else {
            unsigned int httpError;
            storeCookies(abyssSessionP, &httpError);
            if (httpError)
                sendError(abyssSessionP, httpError);
            else {
                unsigned int httpError;
                validateContentType(abyssSessionP, &httpError);
                if (httpError)
                    sendError(abyssSessionP, httpError);
                else {
                    unsigned int httpError;
                    size_t contentSize;

                    processContentLength(abyssSessionP, 
                                         &contentSize, &httpError);
                    if (httpError)
                        sendError(abyssSessionP, httpError);
                    else 
                        processCall(abyssSessionP, contentSize,
                                    uriHandlerXmlrpcP->registryP,
                                    uriHandlerXmlrpcP->chunkResponse,
                                    trace_abyss);
                }
            }
        }
    }
    if (trace_abyss)
        fprintf(stderr, "xmlrpc_server_abyss URI path handler returning.\n");
}



/*=========================================================================
**  xmlrpc_server_abyss_default_handler
**=========================================================================
**  This handler returns a 404 Not Found for all requests. See the header
**  for more documentation.
*/

static xmlrpc_bool 
xmlrpc_server_abyss_default_handler(TSession * const sessionP) {

    if (trace_abyss)
        fprintf(stderr, "xmlrpc_server_abyss default handler called.\n");

    sendError(sessionP, 404);

    return TRUE;
}



static void 
sigchld(int const signalClass ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   This is a signal handler for a SIGCHLD signal (which informs us that
   one of our child processes has terminated).

   The only child processes we have are those that belong to the Abyss
   server (and then only if the Abyss server was configured to use
   forking as a threading mechanism), so we respond by passing the
   signal on to the Abyss server.
-----------------------------------------------------------------------------*/
#ifndef WIN32
    bool childrenLeft;
    bool error;

    assert(signalClass == SIGCHLD);

    error = false;
    childrenLeft = true;  /* initial assumption */
    
    /* Reap defunct children until there aren't any more. */
    while (childrenLeft && !error) {
        int status;
        pid_t pid;

        pid = waitpid((pid_t) -1, &status, WNOHANG);
    
        if (pid == 0)
            childrenLeft = false;
        else if (pid < 0) {
            /* because of ptrace */
            if (errno != EINTR)   
                error = true;
        } else
            ServerHandleSigchld(pid);
    }
#endif /* WIN32 */
}


struct signalHandlers {
    struct sigaction pipe;
    struct sigaction chld;
};



static void
setupSignalHandlers(struct signalHandlers * const oldHandlersP) {
#ifndef WIN32
    struct sigaction mysigaction;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* This signal indicates connection closed in the middle */
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, &oldHandlersP->pipe);
    
    /* This signal indicates a child process (request handler) has died */
    mysigaction.sa_handler = sigchld;
    sigaction(SIGCHLD, &mysigaction, &oldHandlersP->chld);
#endif
}    



static void
restoreSignalHandlers(struct signalHandlers const oldHandlers) {
#ifndef WIN32

    sigaction(SIGPIPE, &oldHandlers.pipe, NULL);
    sigaction(SIGCHLD, &oldHandlers.chld, NULL);

#endif
}



static void
runServerDaemon(TServer *  const serverP,
                runfirstFn const runfirst,
                void *     const runfirstArg) {

    struct signalHandlers oldHandlers;

    setupSignalHandlers(&oldHandlers);

    ServerUseSigchld(serverP);

    ServerDaemonize(serverP);
    
    /* We run the user supplied runfirst after forking, but before accepting
       connections (helpful when running with threads)
    */
    if (runfirst)
        runfirst(runfirstArg);

    ServerRun(serverP);

    restoreSignalHandlers(oldHandlers);
}



static void
setHandler(xmlrpc_env *      const envP,
           TServer *         const srvP,
           const char *      const uriPath,
           xmlrpc_registry * const registryP,
           bool              const chunkResponse) {
    
    struct uriHandlerXmlrpc * uriHandlerXmlrpcP;
    URIHandler2 uriHandler;
    abyss_bool success;

    trace_abyss = getenv("XMLRPC_TRACE_ABYSS");
                                 
    MALLOCVAR_NOFAIL(uriHandlerXmlrpcP);

    uriHandlerXmlrpcP->registryP     = registryP;
    uriHandlerXmlrpcP->uriPath       = strdup(uriPath);
    uriHandlerXmlrpcP->chunkResponse = chunkResponse;

    uriHandler.handleReq2 = handleXmlrpcReq;
    uriHandler.handleReq1 = NULL;
    uriHandler.userdata   = uriHandlerXmlrpcP;
    uriHandler.init       = NULL;
    uriHandler.term       = &termUriHandler;

    ServerAddHandler2(srvP, &uriHandler, &success);

    if (!success)
        xmlrpc_faultf(envP, "Abyss failed to register the Xmlrpc-c request "
                      "handler.  ServerAddHandler2() failed.");

    if (envP->fault_occurred)
        free(uriHandlerXmlrpcP);
}



void
xmlrpc_server_abyss_set_handler(xmlrpc_env *      const envP,
                                TServer *         const srvP,
                                const char *      const uriPath,
                                xmlrpc_registry * const registryP) {

    setHandler(envP, srvP, uriPath, registryP, false);
}

    

static void
setHandlers(TServer *         const srvP,
            const char *      const uriPath,
            xmlrpc_registry * const registryP,
            bool              const chunkResponse) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    trace_abyss = getenv("XMLRPC_TRACE_ABYSS");
                                 
    setHandler(&env, srvP, uriPath, registryP, chunkResponse);
    
    if (env.fault_occurred)
        abort();

    ServerDefaultHandler(srvP, xmlrpc_server_abyss_default_handler);

    xmlrpc_env_clean(&env);
}



void
xmlrpc_server_abyss_set_handlers2(TServer *         const srvP,
                                  const char *      const uriPath,
                                  xmlrpc_registry * const registryP) {

    setHandlers(srvP, uriPath, registryP, false);
}



void
xmlrpc_server_abyss_set_handlers(TServer *         const srvP,
                                 xmlrpc_registry * const registryP) {

    setHandlers(srvP, "/RPC2", registryP, false);
}



static void
oldHighLevelAbyssRun(xmlrpc_env *                      const envP ATTR_UNUSED,
                     const xmlrpc_server_abyss_parms * const parmsP,
                     unsigned int                      const parmSize) {
/*----------------------------------------------------------------------------
   This is the old deprecated interface, where the caller of the 
   xmlrpc_server_abyss API supplies an Abyss configuration file and
   we use it to daemonize (fork into the background, chdir, set uid, etc.)
   and run the Abyss server.

   The new preferred interface, implemented by normalLevelAbyssRun(),
   instead lets Caller set up the process environment himself and pass
   Abyss parameters in memory.  That's a more conventional and
   flexible API.
-----------------------------------------------------------------------------*/
    TServer server;
    runfirstFn runfirst;
    void * runfirstArg;
    
    DateInit();
    
    ServerCreate(&server, "XmlRpcServer", 8080, DEFAULT_DOCS, NULL);
    
    ConfReadServerFile(parmsP->config_file_name, &server);
        
    setHandlers(&server, "/RPC2", parmsP->registryP, false);
        
    ServerInit(&server);
    
    if (parmSize >= XMLRPC_APSIZE(runfirst_arg)) {
        runfirst    = parmsP->runfirst;
        runfirstArg = parmsP->runfirst_arg;
    } else {
        runfirst    = NULL;
        runfirstArg = NULL;
    }
    runServerDaemon(&server, runfirst, runfirstArg);

    ServerFree(&server);
}



static void
setAdditionalServerParms(const xmlrpc_server_abyss_parms * const parmsP,
                         unsigned int                      const parmSize,
                         TServer *                         const serverP) {

    /* The following ought to be parameters on ServerCreate(), but it
       looks like plugging them straight into the TServer structure is
       the only way to set them.  
    */

    if (parmSize >= XMLRPC_APSIZE(keepalive_timeout) &&
        parmsP->keepalive_timeout > 0)
        ServerSetKeepaliveTimeout(serverP, parmsP->keepalive_timeout);
    if (parmSize >= XMLRPC_APSIZE(keepalive_max_conn) &&
        parmsP->keepalive_max_conn > 0)
        ServerSetKeepaliveMaxConn(serverP, parmsP->keepalive_max_conn);
    if (parmSize >= XMLRPC_APSIZE(timeout) &&
        parmsP->timeout > 0)
        ServerSetTimeout(serverP, parmsP->timeout);
    if (parmSize >= XMLRPC_APSIZE(dont_advertise))
        ServerSetAdvertise(serverP, !parmsP->dont_advertise);
}



static void
extractServerCreateParms(
    xmlrpc_env *                      const envP,
    const xmlrpc_server_abyss_parms * const parmsP,
    unsigned int                      const parmSize,
    abyss_bool *                      const socketBoundP,
    unsigned int *                    const portNumberP,
    TOsSocket *                       const socketFdP,
    const char **                     const logFileNameP) {
                   

    if (parmSize >= XMLRPC_APSIZE(socket_bound))
        *socketBoundP = parmsP->socket_bound;
    else
        *socketBoundP = FALSE;

    if (*socketBoundP) {
        if (parmSize < XMLRPC_APSIZE(socket_handle))
            xmlrpc_faultf(envP, "socket_bound is true, but server parameter "
                          "structure does not contain socket_handle (it's too "
                          "short)");
        else
            *socketFdP = parmsP->socket_handle;
    } else {
        if (parmSize >= XMLRPC_APSIZE(port_number))
            *portNumberP = parmsP->port_number;
        else
            *portNumberP = 8080;

        if (*portNumberP > 0xffff)
            xmlrpc_faultf(envP,
                          "TCP port number %u exceeds the maximum possible "
                          "TCP port number (65535)",
                          *portNumberP);
    }
    if (!envP->fault_occurred) {
        if (parmSize >= XMLRPC_APSIZE(log_file_name) &&
            parmsP->log_file_name)
            *logFileNameP = strdup(parmsP->log_file_name);
        else
            *logFileNameP = NULL;
    }
}



static void
createServerBoundSocket(xmlrpc_env * const envP,
                        TOsSocket    const socketFd,
                        const char * const logFileName,
                        TServer *    const serverP,
                        TSocket **   const socketPP) {

    TSocket * socketP;
    const char * error;
    
    SocketUnixCreateFd(socketFd, &socketP);
    
    if (!socketP)
        xmlrpc_faultf(envP, "Unable to create Abyss socket out of "
                      "file descriptor %d.", socketFd);
    else {
        ServerCreateSocket2(serverP, socketP, &error);
        if (error) {
            xmlrpc_faultf(envP, "Abyss failed to create server.  %s",
                          error);
            xmlrpc_strfree(error);
        } else {
            *socketPP = socketP;
                    
            ServerSetName(serverP, "XmlRpcServer");
            
            if (logFileName)
                ServerSetLogFileName(serverP, logFileName);
        }
        if (envP->fault_occurred)
                    SocketDestroy(socketP);
    }
}



static void
createServer(xmlrpc_env *                      const envP,
             const xmlrpc_server_abyss_parms * const parmsP,
             unsigned int                      const parmSize,
             TServer *                         const serverP,
             TSocket **                        const socketPP) {
/*----------------------------------------------------------------------------
   Create a bare server.  It will need further setup before it is ready
   to use.
-----------------------------------------------------------------------------*/
    abyss_bool socketBound;
    unsigned int portNumber = 0;
    TOsSocket socketFd = 0;
    const char * logFileName=NULL;

    extractServerCreateParms(envP, parmsP, parmSize,
                             &socketBound, &portNumber, &socketFd,
                             &logFileName);

    if (!envP->fault_occurred) {
        if (socketBound)
            createServerBoundSocket(envP, socketFd, logFileName,
                                    serverP, socketPP);
        else {
            ServerCreate(serverP, "XmlRpcServer", portNumber, DEFAULT_DOCS, 
                         logFileName);
            
            *socketPP = NULL;
        }
        if (logFileName)
            xmlrpc_strfree(logFileName);
    }
}



static bool
chunkResponseParm(const xmlrpc_server_abyss_parms * const parmsP,
                  unsigned int                      const parmSize) {

    return
        parmSize >= XMLRPC_APSIZE(chunk_response) &&
        parmsP->chunk_response;
}    



static const char *
uriPathParm(const xmlrpc_server_abyss_parms * const parmsP,
            unsigned int                      const parmSize) {
    
    const char * uriPath;

    if (parmSize >= XMLRPC_APSIZE(uri_path) && parmsP->uri_path)
        uriPath = parmsP->uri_path;
    else
        uriPath = "/RPC2";

    return uriPath;
}


#ifdef XMLRPC_OLD_VERSION
static xmlrpc_server_shutdown_fn shutdownAbyss;

static void
shutdownAbyss(xmlrpc_env * const envP,
              void *       const context,
              const char * const comment ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   Tell Abyss to wrap up whatever it's doing and shut down.

   This is a server shutdown function to be registered in the method
   registry, for use by the 'system.shutdown' system method.

   After we return, Abyss will finish up the system.shutdown and any
   other connections that are in progress, then the call to
   ServerRun() etc. will return.  But Abyss may be stuck waiting for
   something, such as the next HTTP connection.  In that case, until it
   gets what it's waiting for, it won't even know it's supposed t shut
   down.  In particular, a caller of system.shutdown may have to execute
   one more RPC in order for the shutdown to happen.
-----------------------------------------------------------------------------*/
    TServer * const serverP = context;

    xmlrpc_env_init(envP);
    
    ServerTerminate(serverP);
}
#endif


static void
normalLevelAbyssRun(xmlrpc_env *                      const envP,
                    const xmlrpc_server_abyss_parms * const parmsP,
                    unsigned int                      const parmSize) {
    
    TServer server;
    TSocket * socketP = 0;

    DateInit();

    createServer(envP, parmsP, parmSize, &server, &socketP);

    if (!envP->fault_occurred) {
        struct signalHandlers oldHandlers;

        setAdditionalServerParms(parmsP, parmSize, &server);

        setHandlers(&server, uriPathParm(parmsP, parmSize), parmsP->registryP,
                    chunkResponseParm(parmsP, parmSize));

        ServerInit(&server);
        
        setupSignalHandlers(&oldHandlers);

        ServerUseSigchld(&server);
        
        //if (0)
            /* Too much of a security risk.  In 1.07, there is a server
               parameter to enable this.
            */
        //    xmlrpc_registry_set_shutdown(parmsP->registryP,
        //                                 &shutdownAbyss, &server);
        
        ServerRun(&server);

        restoreSignalHandlers(oldHandlers);

        ServerFree(&server);

        if (socketP)
            SocketDestroy(socketP);
    }
}



void
xmlrpc_server_abyss(xmlrpc_env *                      const envP,
                    const xmlrpc_server_abyss_parms * const parmsP,
                    unsigned int                      const parmSize) {
 
    XMLRPC_ASSERT_ENV_OK(envP);

    if (parmSize < XMLRPC_APSIZE(registryP))
        xmlrpc_faultf(envP,
                      "You must specify members at least up through "
                      "'registryP' in the server parameters argument.  "
                      "That would mean the parameter size would be >= %lu "
                      "but you specified a size of %u",
                      XMLRPC_APSIZE(registryP), parmSize);
    else {
        if (parmsP->config_file_name)
            oldHighLevelAbyssRun(envP, parmsP, parmSize);
        else
            normalLevelAbyssRun(envP, parmsP, parmSize);
    }
}



/*=========================================================================
  XML-RPC Server Method Registry

  This is an old deprecated form of the server facilities that uses
  global variables.
=========================================================================*/

/* These global variables must be treated as read-only after the
   server has started.
*/

static TServer globalSrv;
    /* When you use the old interface (xmlrpc_server_abyss_init(), etc.),
       this is the Abyss server to which they refer.  Obviously, there can be
       only one Abyss server per program using this interface.
    */

static xmlrpc_registry * builtin_registryP;



void 
xmlrpc_server_abyss_init_registry(void) {

    /* This used to just create the registry and Caller would be
       responsible for adding the handlers that use it.

       But that isn't very modular -- the handlers and registry go
       together; there's no sense in using the built-in registry and
       not the built-in handlers because if you're custom building
       something, you can just make your own regular registry.  So now
       we tie them together, and we don't export our handlers.  
    */
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    builtin_registryP = xmlrpc_registry_new(&env);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);

    setHandlers(&globalSrv, "/RPC2", builtin_registryP, false);
}



xmlrpc_registry *
xmlrpc_server_abyss_registry(void) {

    /* This is highly deprecated.  If you want to mess with a registry,
       make your own with xmlrpc_registry_new() -- don't mess with the
       internal one.
    */
    return builtin_registryP;
}



/* A quick & easy shorthand for adding a method. */
void 
xmlrpc_server_abyss_add_method(char *        const method_name,
                               xmlrpc_method const method,
                               void *        const user_data) {
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method(&env, builtin_registryP, NULL, method_name,
                               method, user_data);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);
}



void
xmlrpc_server_abyss_add_method_w_doc(char *        const method_name,
                                     xmlrpc_method const method,
                                     void *        const user_data,
                                     char *        const signature,
                                     char *        const help) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method_w_doc(
        &env, builtin_registryP, NULL, method_name,
        method, user_data, signature, help);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}



void 
xmlrpc_server_abyss_init(int          const flags ATTR_UNUSED, 
                         const char * const config_file) {

    DateInit();
    MIMETypeInit();

    ServerCreate(&globalSrv, "XmlRpcServer", 8080, DEFAULT_DOCS, NULL);
    
    ConfReadServerFile(config_file, &globalSrv);

    xmlrpc_server_abyss_init_registry();
        /* Installs /RPC2 handler and default handler that use the
           built-in registry.
        */

    ServerInit(&globalSrv);
}



void 
xmlrpc_server_abyss_run_first(runfirstFn const runfirst,
                              void *     const runfirstArg) {
    
    runServerDaemon(&globalSrv, runfirst, runfirstArg);
}



void 
xmlrpc_server_abyss_run(void) {
    runServerDaemon(&globalSrv, NULL, NULL);
}



/*
** Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
** There is more copyright information in the bottom half of this file. 
** Please see it for more details. 
*/
