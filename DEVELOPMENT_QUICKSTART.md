# Kamailio Development Quick Start Guide
## Your Path from FreeSWITCH to Kamailio

---

## Welcome!

Since you have FreeSWITCH + Lua + C experience, you're well-prepared for Kamailio development. This guide will get you started quickly.

---

## Learning Path

### Phase 1: Understanding (Week 1)
1. ✅ Read the analysis report (KAMAILIO_ANALYSIS_REPORT.md)
2. ✅ Understand SIP proxy vs B2BUA difference
3. ✅ Learn Kamailio configuration syntax (.cfg)
4. ✅ Set up development environment

### Phase 2: Configuration (Week 2)
1. Build basic SIP proxy
2. Add authentication
3. Implement routing logic
4. Test with SIP clients

### Phase 3: Lua Development (Week 3-4)
1. Convert config logic to Lua (KAMAILIO_LUA_GUIDE.md)
2. Integrate with databases
3. Add custom business logic
4. Build REST APIs integration

### Phase 4: C Modules (Week 5+)
1. Study existing modules
2. Create simple module (KAMAILIO_C_MODULE_GUIDE.md)
3. Performance optimization
4. Advanced features

---

## Git Workflow for Public Fork

### Initial Setup

```bash
# Clone your fork
git clone http://127.0.0.1:44017/git/rajeswaran140/kamailio.git
cd kamailio

# Add upstream (official Kamailio)
git remote add upstream https://github.com/kamailio/kamailio.git

# Verify
git remote -v
```

### Daily Workflow

```bash
# 1. Create feature branch
git checkout -b feature/my-feature

# 2. Make changes
# ... edit files ...

# 3. Commit (don't commit secrets!)
git add .
git commit -m "Add my feature"

# 4. Push to your fork
git push origin feature/my-feature

# 5. Keep updated with upstream
git fetch upstream
git merge upstream/master
```

### Working with Sensitive Data

**NEVER commit:**
- Database passwords
- API keys
- Production IPs
- Customer data

**Use environment variables or local configs:**

```bash
# Create .gitignore additions
cat >> .gitignore <<EOF
# Local development
etc/*-local.cfg
.env
config/local/
*.secret
credentials.json
EOF
```

**Example: Safe configuration**

```cfg
# kamailio.cfg (committed)
#!trydef DBURL "mysql://kamailio:CHANGEME@localhost/kamailio"

# kamailio-local.cfg (NOT committed, in .gitignore)
#!define DBURL "mysql://kamailio:real_password@192.168.1.10/kamailio"
```

---

## Development Environment Setup

### 1. Build Kamailio from Source

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y \
    gcc g++ bison flex make \
    libmysqlclient-dev libssl-dev libcurl4-openssl-dev \
    libxml2-dev libpcre3-dev libradcli-dev \
    python3-dev liblua5.1-0-dev libhiredis-dev \
    git

# Build Kamailio
cd kamailio/src
make cfg
make all
sudo make install

# Or with specific modules
make include_modules="app_lua app_python3 db_mysql ndb_redis" cfg
make all
sudo make install
```

### 2. Install RTPEngine (for media)

```bash
# Since you're not using FreeSWITCH, use RTPEngine for media relay
sudo apt-get install -y rtpengine rtpengine-kernel-dkms

# Configure
sudo nano /etc/rtpengine/rtpengine.conf
# Set: interface = <your-ip>

# Start
sudo systemctl start rtpengine
```

### 3. Install Database

```bash
# PostgreSQL (recommended)
sudo apt-get install -y postgresql postgresql-contrib

# Create database
sudo -u postgres createdb kamailio
sudo -u postgres createuser kamailio -P

# Or MySQL
sudo apt-get install -y mysql-server
sudo mysql_secure_installation

# Create Kamailio schema
kamdbctl create
```

### 4. Install Redis

```bash
sudo apt-get install -y redis-server

# Configure for production
sudo nano /etc/redis/redis.conf
# Set: bind 127.0.0.1
# Set: maxmemory 256mb
# Set: maxmemory-policy allkeys-lru

sudo systemctl restart redis
```

---

## Project Structure

```
kamailio/
├── src/
│   ├── modules/              # All modules (255+)
│   │   ├── app_lua/         # Lua integration
│   │   ├── app_python3/     # Python integration
│   │   ├── db_mysql/        # MySQL connector
│   │   └── ...
│   ├── core/                # Core Kamailio
│   └── lib/                 # Shared libraries
│
├── etc/
│   ├── kamailio.cfg         # Main config
│   └── kamailio-local.cfg   # Your overrides (git-ignored)
│
├── utils/
│   ├── kamctl/              # Classic CLI tool
│   └── kamcli/              # Modern Python CLI
│
└── doc/                     # Documentation
```

---

## Your First Kamailio Service

### Scenario: Simple SIP Proxy with Auth

**1. Configuration (etc/kamailio.cfg)**

```cfg
#!KAMAILIO

# Network
listen=udp:0.0.0.0:5060
listen=tcp:0.0.0.0:5060

# Features
#!define WITH_MYSQL
#!define WITH_AUTH
#!define WITH_USRLOCDB

# Database
#!define DBURL "mysql://kamailio:password@localhost/kamailio"

# Load modules
loadmodule "tm.so"
loadmodule "sl.so"
loadmodule "rr.so"
loadmodule "pv.so"
loadmodule "maxfwd.so"
loadmodule "textops.so"
loadmodule "siputils.so"
loadmodule "registrar.so"
loadmodule "usrloc.so"
loadmodule "auth.so"
loadmodule "auth_db.so"
loadmodule "db_mysql.so"

# Module parameters
modparam("usrloc", "db_url", DBURL)
modparam("usrloc", "db_mode", 2)
modparam("auth_db", "db_url", DBURL)
modparam("auth_db", "calculate_ha1", yes)
modparam("auth_db", "password_column", "password")

# Routing
request_route {
    # Sanity checks
    if(!mf_process_maxfwd_header("10")) {
        sl_send_reply("483", "Too Many Hops");
        exit;
    }

    # CANCEL processing
    if(is_method("CANCEL")) {
        if(t_check_trans()) {
            t_relay();
        }
        exit;
    }

    # Record Route
    if(!is_method("REGISTER")) {
        record_route();
    }

    # Handle REGISTER
    if(is_method("REGISTER")) {
        route(AUTH);
        route(REGISTRAR);
        exit;
    }

    # Authentication for calls
    route(AUTH);

    # Handle in-dialog requests
    if(has_totag()) {
        if(loose_route()) {
            t_relay();
            exit;
        }
    }

    # New calls
    if(is_method("INVITE")) {
        route(INVITE);
        exit;
    }

    # Default
    sl_send_reply("404", "Not Found");
}

route[AUTH] {
    if(is_method("REGISTER")) {
        if(!auth_check("$fd", "subscriber", "1")) {
            auth_challenge("$fd", "0");
            exit;
        }
    }
}

route[REGISTRAR] {
    if(!save("location")) {
        sl_reply_error();
    }
}

route[INVITE] {
    if(!lookup("location")) {
        sl_send_reply("404", "Not Found");
        exit;
    }
    t_relay();
}
```

**2. Create database and users**

```bash
# Create database schema
kamdbctl create

# Add users
kamctl add alice@yourdomain.com password123
kamctl add bob@yourdomain.com password456
```

**3. Start Kamailio**

```bash
# Test configuration
kamailio -c

# Start in foreground (debug mode)
kamailio -DD -E

# Or start as service
sudo systemctl start kamailio
```

**4. Test with SIP client**

```
Use any SIP client (Linphone, Zoiper, etc.)

Account 1:
- Username: alice@yourdomain.com
- Password: password123
- Server: <your-kamailio-ip>:5060

Account 2:
- Username: bob@yourdomain.com
- Password: password456
- Server: <your-kamailio-ip>:5060

Try calling from alice to bob!
```

---

## Kamailio-Only Architectures

### Option 1: Pure SIP Router + External PBX

```
[Internet/Clients]
        ↓
   [Kamailio]  ← Your focus
        ↓
[Existing PBX/System]
```

**Your role:** Route and secure SIP traffic
**Others handle:** Media, PBX features

### Option 2: Kamailio + RTPEngine

```
[Internet/Clients]
        ↓
   [Kamailio] ← SIP routing, NAT, auth
        ↓
   [RTPEngine] ← Media relay only
        ↓
   [Destination]
```

**You control:** Everything except media transcoding
**RTPEngine does:** RTP relay, NAT, basic media

### Option 3: Kamailio + Microservices

```
[Clients] → [Kamailio] ←→ [Your APIs]
                ↓
            [Database]
            [Redis]
            [Message Queue]
```

**Best for:** CPaaS, custom platforms
**You build:** Business logic in your language
**Kamailio does:** SIP protocol handling

---

## Common Development Tasks

### Task 1: Add Custom Header

**Config way:**
```cfg
append_hf("X-Custom: value\r\n");
```

**Lua way:**
```lua
KSR.textops.append_hf("X-Custom: value\r\n")
```

**C module way:**
```c
add_custom_header(msg);
```

### Task 2: Database Query

**Config way:**
```cfg
sql_query("db", "SELECT...", "result");
```

**Lua way:**
```lua
KSR.sqlops.sql_query("db", "SELECT...", "result")
local value = KSR.pv.get("$avp(result)")
```

**C module way:**
```c
db_funcs.query(db_conn, keys, ops, vals, cols, ...);
```

### Task 3: External API Call

**Lua way:**
```lua
local response = KSR.http_client.query(url, data)
```

**Or use separate service + message queue:**
```
Kamailio → RabbitMQ → Your Service
```

---

## Development Tools

### 1. kamctl - CLI Management

```bash
# User management
kamctl add user@domain password
kamctl rm user@domain
kamctl passwd user@domain newpass

# Statistics
kamctl stats
kamctl monitor

# Database
kamctl db show
```

### 2. kamcli - Modern CLI (Python)

```bash
# Install
pip install kamcli

# Configure ~/.kamcli/kamcli.ini
[main]
sipsrv: 127.0.0.1:5060
kamaddr: 127.0.0.1:5060

# Usage
kamcli ul show
kamcli stats
kamcli rpc stats.get_statistics all
```

### 3. sngrep - SIP Tracing

```bash
sudo apt-get install sngrep
sudo sngrep
```

### 4. homer - SIP Capture

```bash
# Professional SIP monitoring
# https://sipcapture.org/
```

---

## Testing Your Setup

### 1. Basic SIP Test

```bash
# Using sipsak
sudo apt-get install sipsak

# OPTIONS ping
sipsak -s sip:kamailio-ip:5060

# Register test
sipsak -U -s sip:user@domain -a password -r 5060 -i -vv
```

### 2. Load Testing

```bash
# Using SIPp
sudo apt-get install sipp

# Basic call test
sipp -sn uac kamailio-ip:5060 -r 10 -m 100
```

### 3. Monitoring

```bash
# Check Kamailio stats
kamcmd stats.get_statistics all

# Check active dialogs
kamcmd dlg.list

# Check registered users
kamcmd ul.dump
```

---

## Next Steps

1. **Week 1:** Build basic proxy (follow guide above)
2. **Week 2:** Add Lua scripting (see KAMAILIO_LUA_GUIDE.md)
3. **Week 3:** Integrate database and Redis
4. **Week 4:** Add external API integration
5. **Week 5+:** Consider C module development (see KAMAILIO_C_MODULE_GUIDE.md)

---

## Resources Created for You

1. **KAMAILIO_ANALYSIS_REPORT.md** - Complete service analysis
2. **KAMAILIO_LUA_GUIDE.md** - Lua development (leverage your experience)
3. **KAMAILIO_C_MODULE_GUIDE.md** - C module development
4. **This file** - Quick start guide

---

## Community & Support

- **Mailing List:** sr-users@lists.kamailio.org
- **IRC:** #kamailio on Libera.Chat
- **Matrix:** https://riot.kamailio.dev/
- **Forum:** https://www.kamailio.org/w/
- **Documentation:** https://www.kamailio.org/wikidocs/

---

## Key Differences from FreeSWITCH (Quick Reference)

| Aspect | FreeSWITCH | Kamailio |
|--------|------------|----------|
| **Role** | Handles calls | Routes calls |
| **Media** | Built-in | External (RTPEngine) |
| **Config** | XML dialplan | .cfg script |
| **Scripting** | Lua (in call) | Lua (on message) |
| **Scale** | 5K-20K calls | 100K+ calls |
| **Memory** | Higher | Lower |

**Mental Model:**
- FreeSWITCH = You're IN the call (session object)
- Kamailio = You're ROUTING the call (SIP messages)

---

## Pro Tips

1. **Start simple** - Don't enable all modules at once
2. **Use Lua first** - Easier than C modules
3. **Monitor everything** - Set up logging early
4. **Test incrementally** - Add features one at a time
5. **Study existing modules** - Best way to learn C API
6. **Use version control** - Commit often, push regularly
7. **Document configs** - Future you will thank you
8. **Join community** - Ask questions on mailing list

---

Good luck with your Kamailio journey! With your FreeSWITCH background, you'll be productive quickly.

The main mindset shift: Think "router" not "PBX". Focus on routing SIP messages efficiently rather than handling call media.
