# Kamailio C Module Development Guide
## For Developers with C Experience

---

## Overview

Since you have C development experience from FreeSWITCH, creating Kamailio modules will be familiar. Kamailio modules follow a similar plugin architecture.

---

## Module Structure

### Basic Module Template

```
src/modules/mymodule/
├── Makefile
├── CMakeLists.txt
├── mymodule.c          # Main module file
├── mymodule.h          # Header file
├── functions.c         # Module functions
├── doc/
│   └── mymodule_admin.xml
└── README
```

---

## Minimal Module Example

**mymodule.c**
```c
#include <stdio.h>
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_param.h"

MODULE_VERSION

/* Module parameters */
static int default_timeout = 5;
static str server_url = str_init("http://localhost:8080");

/* Function prototypes */
static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);
static int my_function(struct sip_msg* msg, char* param1, char* param2);

/* Module parameter definitions */
static param_export_t params[] = {
    {"timeout",    INT_PARAM, &default_timeout},
    {"server_url", PARAM_STR, &server_url},
    {0, 0, 0}
};

/* Exported functions */
static cmd_export_t cmds[] = {
    {"my_function", (cmd_function)my_function, 2, fixup_spve_spve, 0, ANY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
    "mymodule",      /* module name */
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,            /* exported functions */
    params,          /* exported parameters */
    0,               /* RPC methods */
    0,               /* pseudo-variables */
    0,               /* response handling function */
    mod_init,        /* module init function */
    child_init,      /* per-child init function */
    mod_destroy      /* module destroy function */
};

/* Module initialization - called once */
static int mod_init(void)
{
    LM_INFO("mymodule initialized with timeout=%d, server=%.*s\n",
            default_timeout, server_url.len, server_url.s);

    /* Initialize resources here */

    return 0;  /* 0 = success, -1 = error */
}

/* Child process initialization - called per worker */
static int child_init(int rank)
{
    LM_DBG("Child process %d initialized\n", rank);

    /* Initialize per-process resources (DB connections, etc.) */

    return 0;
}

/* Module cleanup */
static void mod_destroy(void)
{
    LM_INFO("mymodule destroyed\n");

    /* Free resources here */
}

/* Example module function */
static int my_function(struct sip_msg* msg, char* param1, char* param2)
{
    str p1, p2;

    /* Get parameter values */
    if(fixup_get_svalue(msg, (gparam_t*)param1, &p1) != 0) {
        LM_ERR("cannot get first parameter\n");
        return -1;
    }

    if(fixup_get_svalue(msg, (gparam_t*)param2, &p2) != 0) {
        LM_ERR("cannot get second parameter\n");
        return -1;
    }

    LM_INFO("my_function called with: %.*s, %.*s\n",
            p1.len, p1.s, p2.len, p2.s);

    /* Do your work here */

    return 1;  /* 1 = success, -1 = error, 0 = drop */
}
```

---

## Makefile

**Makefile**
```makefile
# Kamailio module makefile
include ../../Makefile.defs
auto_gen=
NAME=mymodule.so

LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
```

**CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.5)

project(mymodule)

kamailio_module(
    NAME mymodule
    SOURCES
        mymodule.c
        functions.c
)
```

---

## Building the Module

```bash
# From Kamailio source root
cd src/modules/mymodule

# Build
make

# Install
sudo make install

# Or build with main Kamailio
cd ../../
make include_modules="mymodule" modules
```

---

## Accessing SIP Message Components

### Basic Message Access

```c
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"

int process_message(struct sip_msg* msg)
{
    /* Parse headers first */
    if(parse_headers(msg, HDR_FROM_F | HDR_TO_F, 0) < 0) {
        LM_ERR("failed to parse headers\n");
        return -1;
    }

    /* Access From header */
    if(msg->from) {
        struct to_body* from = get_from(msg);
        LM_INFO("From URI: %.*s\n",
                from->uri.len, from->uri.s);
        LM_INFO("From Display: %.*s\n",
                from->display.len, from->display.s);
    }

    /* Access To header */
    if(msg->to) {
        struct to_body* to = get_to(msg);
        LM_INFO("To URI: %.*s\n",
                to->uri.len, to->uri.s);
    }

    /* Access Request-URI */
    LM_INFO("Request-URI: %.*s\n",
            msg->first_line.u.request.uri.len,
            msg->first_line.u.request.uri.s);

    /* Access method */
    LM_INFO("Method: %.*s\n",
            msg->first_line.u.request.method.len,
            msg->first_line.u.request.method.s);

    /* Access Call-ID */
    if(msg->callid) {
        LM_INFO("Call-ID: %.*s\n",
                msg->callid->body.len,
                msg->callid->body.s);
    }

    return 1;
}
```

### Parse and Access URI

```c
#include "../../core/parser/parse_uri.h"

int parse_request_uri(struct sip_msg* msg)
{
    struct sip_uri parsed_uri;

    /* Parse R-URI */
    if(parse_uri(msg->first_line.u.request.uri.s,
                 msg->first_line.u.request.uri.len,
                 &parsed_uri) < 0) {
        LM_ERR("failed to parse URI\n");
        return -1;
    }

    LM_INFO("URI User: %.*s\n", parsed_uri.user.len, parsed_uri.user.s);
    LM_INFO("URI Host: %.*s\n", parsed_uri.host.len, parsed_uri.host.s);
    LM_INFO("URI Port: %.*s\n", parsed_uri.port.len, parsed_uri.port.s);

    /* Access URI parameters */
    if(parsed_uri.transport.len > 0) {
        LM_INFO("Transport: %.*s\n",
                parsed_uri.transport.len, parsed_uri.transport.s);
    }

    return 1;
}
```

### Add/Modify Headers

```c
#include "../../core/data_lump.h"

int add_custom_header(struct sip_msg* msg)
{
    str hdr_name = str_init("X-Custom-Header");
    str hdr_value = str_init("CustomValue");
    str hdr;
    struct lump* anchor;
    char* buf;

    /* Build header */
    hdr.len = hdr_name.len + 2 + hdr_value.len + 2;  /* ": " + "\r\n" */
    buf = pkg_malloc(hdr.len);
    if(!buf) {
        LM_ERR("out of memory\n");
        return -1;
    }

    memcpy(buf, hdr_name.s, hdr_name.len);
    memcpy(buf + hdr_name.len, ": ", 2);
    memcpy(buf + hdr_name.len + 2, hdr_value.s, hdr_value.len);
    memcpy(buf + hdr_name.len + 2 + hdr_value.len, "\r\n", 2);

    hdr.s = buf;

    /* Get anchor point (before first header) */
    anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
    if(!anchor) {
        pkg_free(buf);
        LM_ERR("failed to get anchor\n");
        return -1;
    }

    /* Insert header */
    if(insert_new_lump_before(anchor, buf, hdr.len, 0) == 0) {
        pkg_free(buf);
        LM_ERR("failed to insert header\n");
        return -1;
    }

    return 1;
}
```

---

## Database Access

```c
#include "../../lib/srdb1/db.h"

/* Database connection */
static db1_con_t* db_conn = NULL;
static db_func_t db_funcs;
static str db_url = str_init("mysql://kamailio:password@localhost/kamailio");

/* Initialize database */
int init_database(void)
{
    /* Bind to database module */
    if(db_bind_mod(&db_url, &db_funcs)) {
        LM_ERR("failed to bind database module\n");
        return -1;
    }

    /* Check required functions */
    if(!DB_CAPABILITY(db_funcs, DB_CAP_QUERY)) {
        LM_ERR("database doesn't support queries\n");
        return -1;
    }

    return 0;
}

/* Connect to database (per-child) */
int connect_database(void)
{
    db_conn = db_funcs.init(&db_url);
    if(!db_conn) {
        LM_ERR("failed to connect to database\n");
        return -1;
    }

    return 0;
}

/* Query database */
int query_user(str* username)
{
    db_key_t keys[] = {"username"};
    db_val_t vals[1];
    db_key_t cols[] = {"credit", "enabled"};
    db1_res_t* res = NULL;
    int ret;

    /* Set query values */
    vals[0].type = DB1_STR;
    vals[0].nul = 0;
    vals[0].val.str_val = *username;

    /* Execute query */
    if(db_funcs.query(db_conn, keys, NULL, vals, cols,
                      1, 2, NULL, &res) < 0) {
        LM_ERR("database query failed\n");
        return -1;
    }

    /* Check results */
    if(RES_ROW_N(res) == 0) {
        LM_INFO("user not found\n");
        db_funcs.free_result(db_conn, res);
        return 0;
    }

    /* Access result data */
    db_row_t* row = RES_ROWS(res);

    int credit = VAL_INT(&ROW_VALUES(row)[0]);
    int enabled = VAL_INT(&ROW_VALUES(row)[1]);

    LM_INFO("User: credit=%d, enabled=%d\n", credit, enabled);

    ret = (enabled && credit > 0) ? 1 : 0;

    /* Free result */
    db_funcs.free_result(db_conn, res);

    return ret;
}

/* Close database connection */
void close_database(void)
{
    if(db_conn) {
        db_funcs.close(db_conn);
        db_conn = NULL;
    }
}
```

---

## Memory Management

Kamailio has different memory types:

```c
/* PKG memory - per-process, faster */
char* buf1 = pkg_malloc(1024);
pkg_free(buf1);

/* SHM memory - shared across processes, use for global data */
char* buf2 = shm_malloc(1024);
shm_free(buf2);

/* Always check allocation */
char* buf = pkg_malloc(size);
if(!buf) {
    LM_ERR("out of memory\n");
    return -1;
}
```

---

## Pseudo-Variables (PV)

### Export Custom Pseudo-Variable

```c
#include "../../core/pvar.h"

/* Get PV value */
static int pv_get_custom(struct sip_msg* msg, pv_param_t* param,
                         pv_value_t* res)
{
    if(msg == NULL || res == NULL)
        return -1;

    /* Set string value */
    res->rs.s = "custom_value";
    res->rs.len = strlen(res->rs.s);
    res->flags = PV_VAL_STR;

    return 0;
}

/* Set PV value */
static int pv_set_custom(struct sip_msg* msg, pv_param_t* param,
                         int op, pv_value_t* val)
{
    if(val == NULL)
        return -1;

    /* Handle the set operation */
    LM_INFO("Setting custom PV to: %.*s\n", val->rs.len, val->rs.s);

    return 0;
}

/* PV export definition */
static pv_export_t mod_pvs[] = {
    {{"custom", sizeof("custom")-1}, PVT_OTHER,
     pv_get_custom, pv_set_custom, 0, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

/* In module_exports */
struct module_exports exports = {
    /* ... */
    mod_pvs,         /* pseudo-variables */
    /* ... */
};

/* Usage in config: $custom */
```

---

## RPC Commands

```c
#include "../../core/rpc.h"

/* RPC command handler */
static void rpc_status(rpc_t* rpc, void* ctx)
{
    void* handle;

    /* Create response structure */
    if(rpc->add(ctx, "{", &handle) < 0) {
        rpc->fault(ctx, 500, "Internal error");
        return;
    }

    /* Add fields */
    rpc->struct_add(handle, "sd",
                    "status", "running",
                    "calls", 42);

    /* Response sent automatically */
}

/* RPC commands export */
static rpc_export_t rpc_cmds[] = {
    {"mymodule.status", rpc_status, 0, 0},
    {0, 0, 0, 0}
};

/* In module_exports */
struct module_exports exports = {
    /* ... */
    rpc_cmds,        /* RPC methods */
    /* ... */
};

/* Call via kamcmd: kamcmd mymodule.status */
```

---

## Timers

```c
#include "../../core/timer.h"

/* Timer callback */
static void timer_routine(unsigned int ticks, void* param)
{
    LM_INFO("Timer fired at tick %u\n", ticks);

    /* Do periodic work */
}

/* Register timer in mod_init() */
static int mod_init(void)
{
    /* Register timer - fires every 10 seconds */
    if(register_timer(timer_routine, NULL, 10) < 0) {
        LM_ERR("failed to register timer\n");
        return -1;
    }

    return 0;
}
```

---

## Locking (Shared Memory)

```c
#include "../../core/locking.h"

static gen_lock_t* my_lock = NULL;

/* Initialize lock */
int init_lock(void)
{
    my_lock = lock_alloc();
    if(!my_lock) {
        LM_ERR("failed to allocate lock\n");
        return -1;
    }

    if(!lock_init(my_lock)) {
        LM_ERR("failed to initialize lock\n");
        lock_dealloc(my_lock);
        return -1;
    }

    return 0;
}

/* Use lock */
void critical_section(void)
{
    lock_get(my_lock);

    /* Critical section - shared memory access */

    lock_release(my_lock);
}

/* Destroy lock */
void cleanup_lock(void)
{
    if(my_lock) {
        lock_destroy(my_lock);
        lock_dealloc(my_lock);
    }
}
```

---

## Example: Custom Authentication Module

```c
#include "../../core/sr_module.h"
#include "../../core/parser/parse_from.h"
#include "../../lib/srdb1/db.h"
#include <openssl/sha.h>

MODULE_VERSION

/* Database */
static db1_con_t* db_conn = NULL;
static db_func_t db_funcs;
static str db_url = str_init("mysql://user:pass@localhost/db");
static str table_name = str_init("api_keys");

/* Forward declarations */
static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);
static int check_api_key(struct sip_msg* msg, char* p1, char* p2);

/* Exported functions */
static cmd_export_t cmds[] = {
    {"check_api_key", (cmd_function)check_api_key, 0, 0, 0, ANY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
    "auth_api",
    DEFAULT_DLFLAGS,
    cmds,
    0,    /* params */
    0,    /* RPC */
    0,    /* PVs */
    0,    /* response */
    mod_init,
    child_init,
    mod_destroy
};

static int mod_init(void)
{
    /* Bind database */
    if(db_bind_mod(&db_url, &db_funcs)) {
        LM_ERR("failed to bind database\n");
        return -1;
    }

    return 0;
}

static int child_init(int rank)
{
    /* Connect to database */
    db_conn = db_funcs.init(&db_url);
    if(!db_conn) {
        LM_ERR("failed to connect to database\n");
        return -1;
    }

    return 0;
}

static void mod_destroy(void)
{
    if(db_conn) {
        db_funcs.close(db_conn);
    }
}

/* Extract API key from Authorization header */
static int get_api_key(struct sip_msg* msg, str* api_key)
{
    str auth_header;

    /* Get Authorization header */
    if(parse_headers(msg, HDR_AUTHORIZATION_F, 0) < 0) {
        return -1;
    }

    if(!msg->authorization) {
        return -1;
    }

    auth_header = msg->authorization->body;

    /* Simple parsing: "Bearer <key>" */
    if(auth_header.len > 7 &&
       strncmp(auth_header.s, "Bearer ", 7) == 0) {
        api_key->s = auth_header.s + 7;
        api_key->len = auth_header.len - 7;
        return 0;
    }

    return -1;
}

/* Validate API key against database */
static int validate_key(str* api_key)
{
    db_key_t keys[] = {"api_key", "enabled"};
    db_val_t vals[2];
    db_key_t cols[] = {"user_id"};
    db1_res_t* res = NULL;
    int ret;

    /* Set query values */
    vals[0].type = DB1_STR;
    vals[0].nul = 0;
    vals[0].val.str_val = *api_key;

    vals[1].type = DB1_INT;
    vals[1].nul = 0;
    vals[1].val.int_val = 1;  /* enabled = 1 */

    /* Query database */
    if(db_funcs.query(db_conn, keys, NULL, vals, cols,
                      2, 1, NULL, &res) < 0) {
        LM_ERR("database query failed\n");
        return -1;
    }

    ret = (RES_ROW_N(res) > 0) ? 1 : 0;

    db_funcs.free_result(db_conn, res);

    return ret;
}

/* Main function */
static int check_api_key(struct sip_msg* msg, char* p1, char* p2)
{
    str api_key;

    /* Extract API key */
    if(get_api_key(msg, &api_key) < 0) {
        LM_INFO("No API key found\n");
        return -1;
    }

    LM_INFO("Checking API key: %.*s\n", api_key.len, api_key.s);

    /* Validate */
    if(validate_key(&api_key) <= 0) {
        LM_INFO("Invalid API key\n");
        return -1;
    }

    LM_INFO("Valid API key\n");
    return 1;
}
```

**Usage in kamailio.cfg:**
```cfg
loadmodule "auth_api.so"

request_route {
    if(!check_api_key()) {
        send_reply("401", "Unauthorized");
        exit;
    }

    # Continue processing
}
```

---

## Debugging

### Compile with Debug Symbols

```bash
make mode=debug
```

### Use GDB

```bash
# Start Kamailio in foreground
kamailio -DD -E

# Attach GDB to running process
ps aux | grep kamailio
sudo gdb -p <PID>

# Set breakpoint
(gdb) break my_function
(gdb) continue
```

### Logging Macros

```c
LM_DBG("Debug: %d\n", value);      /* Debug level */
LM_INFO("Info: %s\n", string);     /* Info level */
LM_WARN("Warning\n");              /* Warning */
LM_ERR("Error: %d\n", errno);      /* Error */
LM_CRIT("Critical!\n");            /* Critical */
LM_ALERT("Alert!\n");              /* Alert */
```

---

## Best Practices

1. **Always check return values**
2. **Free allocated memory (prevent leaks)**
3. **Use appropriate memory type (pkg vs shm)**
4. **Lock shared memory access**
5. **Validate input parameters**
6. **Keep functions small and focused**
7. **Document your code**
8. **Test thoroughly**

---

## Resources

- **Module Development:** https://www.kamailio.org/wikidocs/tutorials/devel/modules/
- **Core API:** Browse `src/core/*.h` files
- **Example Modules:** Study existing modules in `src/modules/`
- **Mailing List:** sr-dev@lists.kamailio.org

---

## Recommended Modules to Study

- **textops** - Simple text operations (good starting point)
- **htable** - Hash table implementation (shared memory)
- **dispatcher** - Load balancing logic
- **auth_db** - Database integration
- **tm** - Transaction management (advanced)

Your C experience will be very valuable for creating high-performance Kamailio modules!
