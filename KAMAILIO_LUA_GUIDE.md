# Kamailio Lua Development Guide
## For Developers with FreeSWITCH/Lua Experience

---

## Quick Start: Kamailio + Lua

### 1. Installation

```bash
# Install Kamailio with Lua module
sudo apt-get install kamailio kamailio-lua-modules

# Or build from source with Lua support
cd kamailio/src
make include_modules="app_lua" cfg
make all
sudo make install
```

### 2. Basic Configuration

**kamailio.cfg**
```cfg
#!KAMAILIO

# Load Lua module
loadmodule "app_lua.so"

# Point to your Lua script
modparam("app_lua", "load", "/etc/kamailio/scripts/main.lua")

# Enable Lua logging
modparam("app_lua", "log_mode", 1)

# Main routing logic
request_route {
    # Call Lua function
    if (!lua_run("handle_request")) {
        sl_send_reply("500", "Internal Error");
        exit;
    }
}

reply_route {
    lua_run("handle_reply");
}
```

---

## Lua API Reference (KEMI)

### Basic Structure

**/etc/kamailio/scripts/main.lua**
```lua
-- Import Kamailio module
local KSR = require("KSR")

-- Initialize function (called once at startup)
function mod_init()
    KSR.info("Lua script initialized\n")
    return 1
end

-- Child init (called per worker process)
function child_init(rank)
    KSR.info("Child process " .. rank .. " initialized\n")
    return 1
end

-- Main request handler
function handle_request()
    KSR.info("Request from: " .. KSR.pv.get("$fu") .. "\n")

    -- Handle different methods
    if KSR.is_REGISTER() then
        return handle_register()
    elseif KSR.is_INVITE() then
        return handle_invite()
    elseif KSR.is_BYE() then
        return handle_bye()
    end

    return 1
end

-- Reply handler
function handle_reply()
    KSR.info("Reply status: " .. KSR.pv.get("$rs") .. "\n")
    return 1
end
```

---

## FreeSWITCH vs Kamailio Lua Comparison

### Session vs Transaction Paradigm

**FreeSWITCH (Session-based):**
```lua
-- FreeSWITCH: You have a "session" object
session:answer()
session:execute("playback", "/tmp/hello.wav")
session:hangup()
```

**Kamailio (Message-based):**
```lua
-- Kamailio: You process SIP messages
function handle_invite()
    -- No "session" - just route the message
    KSR.rr.record_route()
    KSR.registrar.lookup("location")
    KSR.tm.t_relay()
    return 1
end
```

**Key Difference:**
- FreeSWITCH: You control the call (B2BUA)
- Kamailio: You route the call (Proxy)

---

## Common Kamailio Lua Patterns

### 1. Registration Handler

```lua
function handle_register()
    local user = KSR.pv.get("$rU")
    local domain = KSR.pv.get("$rd")
    local contact = KSR.pv.get("$ct")

    KSR.info("REGISTER: " .. user .. "@" .. domain .. "\n")

    -- Authenticate
    if KSR.auth_db.auth_check(domain, "subscriber", 1) < 0 then
        KSR.auth.auth_challenge(domain, 0)
        return -1
    end

    -- Save location
    if KSR.registrar.save("location", 0) < 0 then
        KSR.sl.sl_reply_error()
        return -1
    end

    return 1
end
```

### 2. Call Routing

```lua
function handle_invite()
    local caller = KSR.pv.get("$fU")  -- From User
    local callee = KSR.pv.get("$rU")  -- Request User
    local callid = KSR.pv.get("$ci")  -- Call-ID

    KSR.info("INVITE: " .. caller .. " -> " .. callee .. " [" .. callid .. "]\n")

    -- Record routing (for subsequent in-dialog requests)
    KSR.rr.record_route()

    -- Lookup user location
    if KSR.registrar.lookup("location") < 0 then
        KSR.sl.send_reply(404, "Not Found")
        return -1
    end

    -- Start accounting
    KSR.acc.setflag(2)  -- FLT_ACC

    -- Relay the call
    KSR.tm.t_relay()

    return 1
end
```

### 3. Load Balancing (Dispatcher)

```lua
function route_to_backend()
    -- Select destination from dispatcher set 1
    -- Algorithm 4 = hash over Call-ID
    if KSR.dispatcher.ds_select_dst(1, 4) < 0 then
        KSR.sl.send_reply(503, "Service Unavailable")
        return -1
    end

    -- Set failure route for failover
    KSR.tm.t_on_failure("DISPATCHER_FAILOVER")

    -- Relay
    KSR.tm.t_relay()
    return 1
end

function DISPATCHER_FAILOVER()
    -- Try next destination on failure
    if KSR.tm.t_check_status("408|503") > 0 then
        if KSR.dispatcher.ds_next_dst() > 0 then
            KSR.tm.t_relay()
        end
    end
end
```

### 4. Database Operations

```lua
function check_user_credit(user)
    -- SQL query
    local query = "SELECT credit FROM accounts WHERE username='" .. user .. "'"

    -- Execute via sqlops module
    if KSR.sqlops.sql_query("db", query, "credit_result") < 0 then
        KSR.err("Database query failed\n")
        return false
    end

    -- Get result
    local credit = KSR.pv.get("$avp(credit_result)")

    KSR.info("User " .. user .. " has credit: " .. credit .. "\n")

    return tonumber(credit) > 0
end

function handle_invite()
    local caller = KSR.pv.get("$fU")

    -- Check credit before routing
    if not check_user_credit(caller) then
        KSR.sl.send_reply(402, "Payment Required")
        return -1
    end

    -- Continue with call
    route_call()
    return 1
end
```

### 5. External API Integration

```lua
-- Use http_async_client for non-blocking HTTP requests
function check_external_api(caller, callee)
    local url = "http://api.example.com/check"
    local data = '{"caller":"' .. caller .. '","callee":"' .. callee .. '"}'

    -- Synchronous HTTP request (use with caution)
    if KSR.http_client then
        local response = KSR.http_client.query(url, data)
        return response
    end

    return nil
end

-- Better: Async pattern with HTTP callback
function handle_invite()
    local caller = KSR.pv.get("$fU")
    local callee = KSR.pv.get("$rU")

    -- Store data for callback
    KSR.pv.sets("$avp(caller)", caller)
    KSR.pv.sets("$avp(callee)", callee)

    -- Suspend transaction for async operation
    KSR.tm.t_suspend()

    -- Make async HTTP call (pseudo-code - actual implementation varies)
    -- When response arrives, call http_callback()

    return 1
end
```

### 6. Redis Integration

```lua
function store_in_redis(key, value)
    -- Use ndb_redis module
    local cmd = 'SET ' .. key .. ' ' .. value

    if KSR.redis.redis_cmd("srv1", cmd, "result") < 0 then
        KSR.err("Redis command failed\n")
        return false
    end

    return true
end

function get_from_redis(key)
    local cmd = 'GET ' .. key

    if KSR.redis.redis_cmd("srv1", cmd, "result") < 0 then
        return nil
    end

    return KSR.pv.get("$avp(result)")
end

-- Example: Session tracking
function handle_invite()
    local callid = KSR.pv.get("$ci")
    local caller = KSR.pv.get("$fU")

    -- Store call info in Redis
    store_in_redis("call:" .. callid, caller)

    -- Continue routing
    return route_call()
end
```

### 7. Custom Routing Logic

```lua
function route_by_prefix(number)
    -- Extract prefix
    local prefix = string.sub(number, 1, 3)

    -- Route based on prefix
    if prefix == "800" then
        -- Toll-free to gateway 1
        KSR.pv.sets("$du", "sip:192.168.1.10:5060")
    elseif prefix == "900" then
        -- Premium to gateway 2
        KSR.pv.sets("$du", "sip:192.168.1.11:5060")
    else
        -- Default gateway
        KSR.pv.sets("$du", "sip:192.168.1.1:5060")
    end

    KSR.tm.t_relay()
    return 1
end

function handle_invite()
    local number = KSR.pv.get("$rU")
    return route_by_prefix(number)
end
```

### 8. Dialog Tracking

```lua
function handle_invite()
    local caller = KSR.pv.get("$fU")
    local callee = KSR.pv.get("$rU")

    -- Track dialog
    if KSR.dialog.dlg_manage() < 0 then
        KSR.err("Dialog tracking failed\n")
    end

    -- Set dialog timeout (3600 seconds)
    KSR.dialog.set_dlg_timeout(3600)

    -- Store variables in dialog
    KSR.dialog.set_dlg_var("caller_name", caller)
    KSR.dialog.set_dlg_var("start_time", os.time())

    -- Continue routing
    return route_call()
end

function handle_bye()
    -- Get dialog variables
    local caller = KSR.dialog.get_dlg_var("caller_name")
    local start_time = tonumber(KSR.dialog.get_dlg_var("start_time"))
    local duration = os.time() - start_time

    KSR.info("Call ended: " .. caller .. ", duration: " .. duration .. "s\n")

    return 1
end
```

---

## Accessing Pseudo-Variables

Kamailio has extensive pseudo-variables (like FreeSWITCH channel variables):

```lua
-- Read variables
local from_user = KSR.pv.get("$fU")          -- From username
local from_domain = KSR.pv.get("$fd")        -- From domain
local to_user = KSR.pv.get("$tU")            -- To username
local request_user = KSR.pv.get("$rU")       -- Request-URI username
local method = KSR.pv.get("$rm")             -- Request method
local callid = KSR.pv.get("$ci")             -- Call-ID
local source_ip = KSR.pv.get("$si")          -- Source IP
local source_port = KSR.pv.get("$sp")        -- Source port
local user_agent = KSR.pv.get("$ua")         -- User-Agent header

-- Write variables
KSR.pv.sets("$rU", "newuser")                -- Change R-URI username
KSR.pv.sets("$du", "sip:1.2.3.4:5060")      -- Set destination URI

-- AVPs (Attribute-Value Pairs) - temporary storage
KSR.pv.sets("$avp(custom)", "value")
local val = KSR.pv.get("$avp(custom)")

-- Script variables (within transaction)
KSR.pv.sets("$var(counter)", "1")
local count = KSR.pv.get("$var(counter)")
```

**Full list:** https://www.kamailio.org/wikidocs/cookbooks/devel/pseudovariables/

---

## Module Functions from Lua

All Kamailio module functions are accessible:

```lua
-- TM (Transaction Module)
KSR.tm.t_relay()
KSR.tm.t_reply(200, "OK")
KSR.tm.t_on_failure("ROUTE_NAME")

-- RR (Record-Route)
KSR.rr.record_route()
KSR.rr.loose_route()

-- Registrar
KSR.registrar.save("location", 0)
KSR.registrar.lookup("location")

-- Auth
KSR.auth.auth_challenge("domain.com", 0)
KSR.auth_db.auth_check("domain.com", "subscriber", 1)

-- Dispatcher
KSR.dispatcher.ds_select_dst(1, 4)
KSR.dispatcher.ds_next_dst()

-- Dialog
KSR.dialog.dlg_manage()
KSR.dialog.set_dlg_timeout(3600)

-- SL (Stateless)
KSR.sl.send_reply(404, "Not Found")
KSR.sl.sl_reply_error()

-- Textops
KSR.textops.append_hf("X-Custom-Header: value\r\n")
KSR.textops.remove_hf("User-Agent")

-- Sanity
KSR.sanity.sanity_check(1511, 7)

-- Pike (anti-flood)
KSR.pike.pike_check_req()

-- Permissions
KSR.permissions.allow_source_address(1)
```

---

## Advanced Patterns

### Pattern 1: Least-Cost Routing

```lua
-- Database: routes table
-- | prefix | gateway           | cost  |
-- | 1      | sip:gw1.com:5060 | 0.01  |
-- | 1      | sip:gw2.com:5060 | 0.015 |

function lcr_route(number)
    -- Extract prefix (first 1-5 digits)
    for len = 5, 1, -1 do
        local prefix = string.sub(number, 1, len)
        local query = "SELECT gateway FROM routes WHERE prefix='" .. prefix ..
                     "' ORDER BY cost LIMIT 1"

        if KSR.sqlops.sql_query("db", query, "lcr_gw") >= 0 then
            local gateway = KSR.pv.get("$avp(lcr_gw)")
            if gateway then
                KSR.pv.sets("$du", gateway)
                return true
            end
        end
    end

    return false
end
```

### Pattern 2: Call Limitation

```lua
-- Limit concurrent calls per user
function check_call_limit(user, max_calls)
    -- Use dialog module to count active dialogs
    local key = "calls:" .. user

    -- Get current count from Redis
    local current = tonumber(get_from_redis(key)) or 0

    if current >= max_calls then
        KSR.sl.send_reply(486, "Too Many Calls")
        return false
    end

    -- Increment counter
    store_in_redis(key, tostring(current + 1))

    -- Set dialog callback to decrement on end
    KSR.dialog.dlg_manage()
    KSR.pv.sets("$dlg_var(user)", user)

    return true
end

-- In dialog end event
function dialog_end()
    local user = KSR.pv.get("$dlg_var(user)")
    local key = "calls:" .. user
    local current = tonumber(get_from_redis(key)) or 1
    store_in_redis(key, tostring(current - 1))
end
```

### Pattern 3: WebRTC Handling

```lua
function handle_websocket()
    -- Check if WebSocket request
    if KSR.pv.get("$hdr(Upgrade)") == "websocket" then
        if KSR.websocket.ws_handle_handshake() < 0 then
            return -1
        end
        return 1
    end
    return 0
end

function handle_request()
    -- Check transport
    local proto = KSR.pv.get("$proto")

    if proto == "ws" or proto == "wss" then
        KSR.info("WebSocket client\n")
        -- Add WebSocket-specific handling
    end

    return route_request()
end
```

---

## Debugging Lua Scripts

### Logging

```lua
-- Different log levels
KSR.dbg("Debug message\n")
KSR.info("Info message\n")
KSR.warn("Warning message\n")
KSR.err("Error message\n")

-- Log with variables
KSR.info("User: " .. KSR.pv.get("$fU") .. " from IP: " .. KSR.pv.get("$si") .. "\n")
```

### Print Variables

```lua
function debug_request()
    KSR.info("=== Request Debug ===\n")
    KSR.info("Method: " .. KSR.pv.get("$rm") .. "\n")
    KSR.info("From: " .. KSR.pv.get("$fu") .. "\n")
    KSR.info("To: " .. KSR.pv.get("$tu") .. "\n")
    KSR.info("R-URI: " .. KSR.pv.get("$ru") .. "\n")
    KSR.info("Call-ID: " .. KSR.pv.get("$ci") .. "\n")
    KSR.info("Source: " .. KSR.pv.get("$si") .. ":" .. KSR.pv.get("$sp") .. "\n")
    KSR.info("==================\n")
end
```

### Test Script Syntax

```bash
# Check Kamailio config (includes Lua loading)
kamailio -c -f /etc/kamailio/kamailio.cfg

# Check Lua syntax
lua -c /etc/kamailio/scripts/main.lua

# Run Kamailio with debug
kamailio -DD -E
```

---

## Performance Tips

### 1. Minimize Database Queries
```lua
-- Bad: Query for each request
function handle_invite()
    local query = "SELECT * FROM users WHERE username='" .. KSR.pv.get("$fU") .. "'"
    KSR.sqlops.sql_query("db", query, "result")
    -- ...
end

-- Good: Use htable (in-memory cache)
-- Load data at startup or periodically
-- Access via $sht(users=>username)
```

### 2. Use Local Variables
```lua
-- Cache pseudo-variable access
local from_user = KSR.pv.get("$fU")
local to_user = KSR.pv.get("$tU")

-- Use locals in processing
if from_user == "blocked" then
    -- ...
end
```

### 3. Avoid Heavy Processing in Request Route
```lua
-- Bad: Heavy processing blocks worker
function handle_invite()
    -- Complex calculations
    for i = 1, 1000000 do
        -- something
    end
end

-- Good: Use async or external service
function handle_invite()
    -- Quick check
    if basic_check() then
        -- Delegate heavy work to external service
        http_async_call("/api/process")
    end
end
```

---

## Complete Example: Custom PBX Logic

```lua
-- /etc/kamailio/scripts/pbx.lua

local KSR = require("KSR")

-- Configuration
local VOICEMAIL_NUMBER = "999"
local MAX_CALL_DURATION = 3600  -- 1 hour

function mod_init()
    KSR.info("PBX Lua script loaded\n")
    return 1
end

function handle_request()
    -- Sanity checks
    if KSR.sanity.sanity_check(1511, 7) < 0 then
        KSR.err("Sanity check failed\n")
        return -1
    end

    -- Handle registration
    if KSR.is_REGISTER() then
        return handle_register()
    end

    -- Handle in-dialog requests
    if KSR.siputils.has_totag() > 0 then
        if KSR.rr.loose_route() < 0 then
            KSR.sl.send_reply(403, "Forbidden")
            return -1
        end
        KSR.tm.t_relay()
        return 1
    end

    -- Handle new calls
    if KSR.is_INVITE() then
        return handle_invite()
    end

    -- Other methods
    return 1
end

function handle_register()
    local user = KSR.pv.get("$rU")
    local domain = KSR.pv.get("$rd")

    -- Authenticate
    if KSR.auth_db.auth_check(domain, "subscriber", 1) < 0 then
        KSR.auth.auth_challenge(domain, 0)
        return -1
    end

    -- Save location
    KSR.registrar.save("location", 0)
    return 1
end

function handle_invite()
    local caller = KSR.pv.get("$fU")
    local callee = KSR.pv.get("$rU")

    KSR.info("Call: " .. caller .. " -> " .. callee .. "\n")

    -- Check for voicemail
    if callee == VOICEMAIL_NUMBER then
        return route_to_voicemail()
    end

    -- Record-Route
    KSR.rr.record_route()

    -- Start accounting
    KSR.acc.setflag(2)

    -- Track dialog
    KSR.dialog.dlg_manage()
    KSR.dialog.set_dlg_timeout(MAX_CALL_DURATION)

    -- Lookup location
    if KSR.registrar.lookup("location") < 0 then
        -- Not registered - send to voicemail
        return route_to_voicemail()
    end

    -- Set failure route for voicemail on busy
    KSR.tm.t_on_failure("VOICEMAIL_FAILURE")

    -- Relay call
    KSR.tm.t_relay()
    return 1
end

function route_to_voicemail()
    -- In real implementation, route to Asterisk or voicemail server
    KSR.pv.sets("$du", "sip:voicemail.local:5070")
    KSR.tm.t_relay()
    return 1
end

function VOICEMAIL_FAILURE()
    -- Called on failure (busy, no answer, etc.)
    if KSR.tm.t_check_status("486|408|487") > 0 then
        route_to_voicemail()
    end
end

function handle_reply()
    -- Reply processing
    return 1
end
```

---

## Resources

- **Kamailio Lua Documentation:** https://www.kamailio.org/docs/modules/stable/modules/app_lua.html
- **KEMI (Lua API):** https://www.kamailio.org/docs/tutorials/devel/kamailio-kemi-framework/
- **Pseudo-variables:** https://www.kamailio.org/wikidocs/cookbooks/devel/pseudovariables/
- **Module Functions:** https://www.kamailio.org/docs/modules/stable/

---

## Next Steps

1. Start with basic routing in Lua
2. Add authentication and registration
3. Implement custom business logic
4. Integrate with external services (HTTP APIs, Redis)
5. Add monitoring and statistics
6. Optimize performance

Your FreeSWITCH Lua experience will transfer well - the main difference is thinking "proxy/route" instead of "handle/process" the call.
