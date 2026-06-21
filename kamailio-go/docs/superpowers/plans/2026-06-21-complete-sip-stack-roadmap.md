# Kamailio-Go vs C Kamailio — Detailed Comparison & Roadmap

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detail exactly how kamailio-go compares to the C-language Kamailio SIP server (formerly OpenSER), catalog every significant feature gap, and provide a phase-by-phase implementation plan with file-level precision so any engineer can execute it without domain context.

**Architecture:** Each phase builds one subsystem. Phases 27–30 add the most-commonly-used Kamailio modules (script engine, accounting, topology hiding, parallel forking). Phases 31–34 add carrier-grade features (rate limiting, database backends, B2BUA, IMS completeness). Phase 35 is the "glue" release: binding all modules together through the ProxyCore pipeline.

**Tech Stack:** Go 1.21+, test-only dependencies; no CGO unless a specific section calls for it.

---

## 1. Existing State of kamailio-go

### 1.1 What Is Already Implemented (by module)

| Module | Files | Status |
|--------|-------|--------|
| **SIP Parser** | [parser/](file:///workspace/kamailio-go/internal/core/parser/) — `parser.go`, `msg.go`, `fline.go`, 25+ parse\_ files | ⭐⭐⭐⭐ Full request/reply parsing, URI, Via, Contact, Record-Route, headers, body, session-timers. |
| **Transaction Manager (TM)** | [modules/tm/](file:///workspace/kamailio-go/internal/modules/tm/) — `transaction.go`, `transaction_engine.go`, `retransmit.go` | ⭐⭐⭐ Branch-aware hash table, state machine (Trying/Proceeding/Completed/Confirmed), retransmit timers, local transactions. |
| **Dialog Manager** | [core/dialog/](file:///workspace/kamailio-go/internal/core/dialog/) — `dialog.go`, `callback.go`, `timer.go`, `profile.go` | ⭐⭐⭐ Call-ID+FromTag+ToTag matching, lifecycle callbacks, profile hooks. |
| **User Location (usrloc)** | [core/usrloc/](file:///workspace/kamailio-go/internal/core/usrloc/) — `usrloc.go` | ⭐⭐ AOR → Contact tree, expiry, registrar. No DB persistence, no Path support. |
| **Digest Auth** | [core/auth/](file:///workspace/kamailio-go/internal/core/auth/) — `digest.go` | ⭐⭐ Nonce generation/validation, HA1/HA2, 401/407 challenges. No DB-backed credentials, no QOP, no stale detection. |
| **NAT Traversal** | [core/nat/](file:///workspace/kamailio-go/internal/core/nat/) — `nat.go` | ⭐⭐ Basic NAT detection (src vs Contact mismatch), `rport`/`received`, Contact rewriting. No full symmetric-latching, no media pinhole control. |
| **RTP Engine** | [core/rtpengine/](file:///workspace/kamailio-go/internal/core/rtpengine/) — `rtpengine.go` | ⭐⭐ Offer/Answer/Delete over HTTP JSON + mock UDP. No full SDP rewriting pipeline. |
| **Presence** | [core/presence/](file:///workspace/kamailio-go/internal/core/presence/) — `presence.go`, `pidf.go`, `server.go` | ⭐⭐ PIDF/XPIDF, simple subscription state, MWI. No full PUA integration, no XCAP. |
| **DB Abstraction** | [core/db/](file:///workspace/kamailio-go/internal/core/db/) — `db.go`, `db_memory.go`, `db_redis.go` | ⭐⭐ Unified DB API + memory & Redis backends. Only CRUD. No SQL drivers. |
| **PV / Transformations** | [core/pv/](file:///workspace/kamailio-go/internal/core/pv/) — `pv.go`, `pv_msg.go` | ⭐⭐ Core PV variables, basic transformations (`$rU`, `$fU`, `$ci`). Limited coverage. |
| **Forwarder** | [core/forward/](file:///workspace/kamailio-go/internal/core/forward/) — `forward.go` | ⭐⭐ Stateless forwarding, Via/Record-Route rewriting. |
| **Router / Script Engine** | [core/router/](file:///workspace/kamailio-go/internal/core/router/) — `router.go`, `ruri.go`, `route_dns.go` | ⭐⭐ Action tree skeleton (`ActionType`, `Action`, `ActionElem`). No actual script language or runtime executor. |
| **Proxy Core / Pipeline** | [core/proxy/](file:///workspace/kamailio-go/internal/core/proxy/) — `proxy.go`, `headers.go`, `metrics.go`, `logging.go`, `health.go`, `shutdown.go`, `listener.go` | ⭐⭐⭐ `ResponseAction` dispatch, method routing, metrics, health checks, graceful shutdown. |
| **SDP** | [core/sdp/](file:///workspace/kamailio-go/internal/core/sdp/) — `sdp.go`, `sdpops.go` | ⭐⭐ Basic session/media, SDP build/parse round-trip. |
| **Transport** | [core/transport/](file:///workspace/kamailio-go/internal/core/transport/) — `udp.go`, `tcp.go`, `tls.go` | ⭐⭐⭐ Full UDP/TCP/TLS listener adapters. |
| **CLI / Bootstrap** | [cmd/kamailio/](file:///workspace/kamailio-go/cmd/kamailio/) — `main.go`, `cli.go`, `bootstrap.go` | ⭐⭐⭐ Subcommand dispatcher, config loading, service bootstrap. |
| **IMS** | [ims/](file:///workspace/kamailio-go/internal/ims/) — `pcscf/`, `scscf/`, `auth/aka.go` | ⭐⭐ P-CSCF session handler, S-CSCF register, PANI parsing, AKA auth skeleton. Not feature-complete. |
| **DNS** | [core/dns/](file:///workspace/kamailio-go/internal/core/dns/) — `resolver.go`, `cache.go` | ⭐⭐ Basic resolver + cache. No DNS SRV load balancing failover. |

### 1.2 What the C Implementation Has That We Don't

Below is every meaningful C-Kamailio feature/capability not present in kamailio-go. Items are grouped by functional area, and each is marked with a priority (**P0 = production-blocker**, **P1 = important**, **P2 = nice-to-have**).

#### P0 — Production Blockers

| Feature | C Module(s) | Why It Matters |
|---------|------------|----------------|
| **Stateful routing script engine & executor** | `route.c`, `cfg.y`, `action.c` | Without a working routing script, we cannot make per-request policy decisions. The current `Action` tree is data-only with no runner. |
| **Parallel / serial forking** | `tm.c`, `t_fork.c` | Currently we can forward to exactly one destination. Multi-branch forking is required for any hunt-group or failover scenario. |
| **Accounting (ACC) / CDR generation** | `acc.c`, `acc_db.c`, `acc_radius.c` | Every carrier deployment needs CDRs — at minimum `200 OK`/`BYE` event logs with call duration. |
| **Topology Hiding** | `topoh.c`, `topos.c` | Required by carriers to hide internal topology (domains, IPs, tags) from external entities. |
| **Rate limiting / Flood protection** | `pike.c`, `ratelimit.c` | Defense against SIP brute-force and registration floods. Without this, any public-facing proxy is vulnerable. |
| **`$var(name)` / `$avp(name)` persistent variable store** | `pv_core.c`, `avp.c` | Script actions need per-request mutable state; our PV module is read-only message introspection. |

#### P1 — Important Features

| Feature | C Module(s) | Notes |
|---------|------------|-------|
| **Record-Route / Route set stateful proxying** | `rr.c`, `rr_cb.c` | Needed so in-dialog requests traverse the proxy for the whole call. Proxy struct has `RecordRouteEnabled` but no logic that actually adds/stores the Route set on follow-up requests. |
| **Dialog timeout, early-media detection, per-dialog timers** | `dialog.c` timers | `timer.go` exists but no configurable per-profile timeout / early-to-confirmed state tracking. |
| **Contact-rewriting + media pinhole coordination (nathelper)** | `nathelper.c` | Our `nat.go` rewrites Contact/R-URI, but does not coordinate RTPengine calls for media pinhole. |
| **Full RTP/SRTP key negotiation & media rewriting** | `rtpengine`, `siputils` | We can call an RTPengine; we do not automatically run SDP through it per-dialog. |
| **Auth credential lookup from database** | `auth_db.c` | `digest.go` validates credentials only from in-memory table. |
| **Database drivers beyond memory & Redis** | `db_mysql`, `db_postgres`, `db_sqlite`, `db_text` | Need SQL drivers for production user location and CDR persistence. |
| **DNS SRV failover / NAPTR + weighted routing** | `resolve.c`, `dns_srv.c` | `dns` module does basic A/AAAA; no SRV or weight-based routing. |
| **ENUM / dialplan translation** | `enum.c`, `dialplan.c` | Number normalization, E.164 lookup, prefix routing. |
| **Presence User Agent (PUA) + shared presence + XCAP** | `pua*.c`, `xcap_server`, `presence_xml` | Presence module is minimal; no real NOTIFY pipeline beyond local subscriptions. |
| **Instant MESSAGE store-and-forward (msilo)** | `msilo.c` | Offline message queueing — needed for SMS-over-SIP / enterprise IM. |
| **Statistics / counters / MI/RPC interface** | `stats.c`, `ctl.c`, `jsonrpcs.c` | Currently we only have the `metrics.go` snapshot; no runtime RPC command channel (`kamcmd` equivalent). |
| **In-memory hash table module (htable)** | `htable.c` | Shared key-value store for script access; used for registration caches, rate-limit buckets, feature flags. |
| **AVP dynamic attribute-value pairs** | `avpops.c` | Needed by script engine and auth/DB interactions. |
| **Session-Timers (RFC 4028) enforcement** | `session_timer.c` | Parser has session-timers; no enforcement logic (re-INVITE, 422 handling). |
| **Real pipeline: the `request_route` / `reply_route` / `onreply_route` / `failure_route` blocks** | `route.c`, `route_struct.h` | Current `ProcessRequest` is a hardcoded Go switch; no programmable route block concept. |
| **Refer / Transfer / Replaces header support** | `refer.c`, `textops.c` | REFER handling + Replaces header processing for attended transfer. |
| **TCP connection reuse / keep-alive** | `tcp_main.c` | TCP listeners work but there is no outbound connection pool or keep-alive. |

#### P2 — Nice to Have / Advanced Features

| Feature | C Module(s) | Notes |
|---------|------------|-------|
| **B2BUA (back-to-back user agent)** | `b2b_*.c` | Break the call into two independent dialogs — useful for call transfer, prepaid, session border controller behavior. |
| **WebSocket transport** | `ws_frame.c`, `websocket.c` | Required for WebRTC clients (SIP.js, etc.). |
| **SCTP transport** | `sctp_core.c` | Used by some IMS core network interfaces. |
| **Middleware engines (Lua / Python / JavaScript)** | `app_lua`, `app_python3`, `app_jsdt` | Extend routing with embedded scripts. |
| **Carrier routing / LCR / DRouting** | `carrierroute`, `lcr`, `drouting` | Per-destination carrier selection, least-cost routing, dynamic routing tables. |
| **Call Control / Prepaid** | `cc_*.c`, `dialog_events` | Prepaid call duration limits, mid-call re-authorization. |
| **Advanced presence: shared-dialog, BLA, XMLCurl** | `presence_xml`, `pua_bla`, `xcap_client` | Enterprise PBX features. |
| **JSON / JWT auth** | `auth_jwt.c` | Modern authentication for WebRTC/API-driven deployments. |
| **Quality monitoring (QoS stats, RTCP-XR)** | `rtp_agent.c` | Media quality reporting. |
| **Graphite / StatsD / Prometheus exporter** | `statsd`, `topoh integration` | Expose counters and latency to a metrics pipeline. Our current `/status` endpoint is minimal. |
| **TLS client certificates / SNI** | `tls_server*` | Advanced TLS profile support for trunking. |
| **Kamcmd / kamctl / RPC console** | `ctl`, `mi_xmlrpc` | Runtime remote control interface. |

---

## 2. Phase Organization & Timelines

Total work from Phase 27 through Phase 35 is roughly **~3x the scope of Phase 1-26 combined**. We prioritize by production readiness first — after Phase 30, kamailio-go can handle a real SIP trunking scenario. Phase 31-35 make it carrier-grade.

```
Phase 27 — Script Routing Engine (核心路由脚本引擎)
Phase 28 — Parallel/Searial Forking + DNS SRV  (并行/串行分叉 + DNS SRV)
Phase 29 — Accounting / CDR Generation  (计费 / CDR 生成)
Phase 30 — Rate Limiting + Topology Hiding  (速率限制 + 拓扑隐藏)
Phase 31 — Hash Tables (htable) + AVP Store  (哈希表 + AVP)
Phase 32 — Persistent DB Drivers + Auth DB Lookup  (持久化数据库 + 认证库查询)
Phase 33 — Full NAT / Media Rewrite Pipeline  (完整 NAT / 媒体重写流水线)
Phase 34 — IMS / Presence + MESSAGE msilo  (IMS / Presence + 离线消息)
Phase 35 — Runtime RPC + Pipeline Glue Release  (RPC + 整体粘合发布)
```

Each Phase's tasks are described below, with exact files, function signatures, and test design.

---

## 3. Phase 27 — Script Routing Engine

**Goal:** Replace the current hardcoded Go `switch msg.Method` in [`proxy.go:480`](file:///workspace/kamailio-go/internal/core/proxy/proxy.go#L480-L553) with a programmable script engine. Script text is parsed into an action tree, then executed per-request in `ProcessRequest`.

### 3.1 Files to Create

**File: `internal/core/script/script.go`**

Purpose: Parser + AST + executor for the minimal Kamailio script language.

```go
package script

// RouteBlock represents one `route[NAME] { ... }` block in a script.
// C: `struct action*` inside route.c
type RouteBlock struct {
    Name   string
    Actions []*Action
}

// ActionType enumerates the script primitives we implement first.
// (Our existing router.ActionType in router.go:28 is the skeleton; this replaces and extends it.)
type ActionType int

const (
    ActionForward ActionType = iota  // forward(uri);
    ActionSendReply                    // sl_send_reply(code, reason);
    ActionDrop                         // drop;
    ActionLog                          // xlog("...");
    ActionSetFlag                      // setflag(N);
    ActionResetFlag                    // resetflag(N);
    ActionIf                           // if (expression) { ... }
    ActionAddRecordRoute              // record_route();
    ActionRemoveRecordRoute           // remove_record_route();
    ActionAppendBranch                // append_branch(uri);
    ActionSetURI                      // $ruri = "sip:...";    → rewrites Request-URI
    ActionSetDstURI                   // $du = "sip:...";     → sets destination URI
    ActionRoute                       // route("NAME");
    ActionReturn                      // return;
)

// Action is one script instruction.
// Actions form a linked list; Action.If owns two branches.
type Action struct {
    Type      ActionType
    Arg       string      // literal argument, e.g. a URI or "180 Ringing"
    ArgNum    int         // numeric argument (flag number, status code)
    IfTrue    []*Action   // "true" branch for ActionIf
    IfFalse   []*Action   // "false" branch for ActionIf
    RouteName string      // for ActionRoute
}

// Expr is a boolean expression in `if (expr) {`.
// Implemented: method == "INVITE", uri == "sip:...", pv == "x", flag(N)
type Expr struct {
    Op     string // "==", "!=", "!~", "=~"
    Left   PVRef  // left-hand side, can be a SIP pseudo-variable
    Right  string // literal
    IsFlag bool
    Flag   int
}

// PVRef references a pseudo-variable like $rU, $fU, $ci, $rd.
// Parser turns text into an enum at load time so runtime is just field access.
type PVRef int

const (
    PVReqUser PVRef = iota // $rU
    PVFromUser             // $fU
    PVToUser               // $tU
    PVCallID               // $ci
    PVReqDomain            // $rd
    PVFromDomain           // $fd
    PVMethod               // $rm
    PVStatus               // $rs — reply only
    PVRURI                 // $ru — full request-URI
)

// Script is the parsed, executable program.
type Script struct {
    Root    []*Action           // default request_route block
    Routes  map[string][]*Action // named routes
}

// ParseScript parses text into a Script AST.
// Grammar subset — lines are semicolon-separated, blocks use braces.
//
//   request_route {
//      if (method == "REGISTER") { route(REGISTRAR); }
//      if (method == "INVITE")    { record_route(); route(RELAY); }
//      drop;
//   }
//
//   route[REGISTRAR] {
//      if (!flag(1)) { setflag(1); sl_send_reply("200", "OK"); return; }
//   }
//
//   route[RELAY] {
//      $ruri = "sip:192.168.1.200"; forward();
//   }
func ParseScript(text string) (*Script, error) { ... }
```

**File: `internal/core/script/executor.go`**

```go
package script

import "github.com/kamailio/kamailio-go/internal/core/parser"

// ExecContext carries mutable per-request state for the script.
// C: `struct run_actions_ctx`
type ExecContext struct {
    Msg     *parser.SIPMsg
    SrcAddr net.Addr
    Flags   uint32           // bit field for setflag/resetflag
    RURI    string           // writable copy of request-URI
    DstURI  string           // $du — destination override
    Reply   *ReplyAction     // non-nil means the script decided to reply
    Drops   bool             // drop; was executed
    Logs    []string         // xlog output (useful for tests)
    Vars    map[string]string // $var(name) simple string store
}

// ReplyAction captures what the script decided to reply.
type ReplyAction struct {
    Status  int
    Reason  string
    Headers []string
}

// Execute runs the default request_route block.
func (s *Script) Execute(ctx *ExecContext) error { ... }

// executeOne runs a single action and returns the next action.
func (s *Script) executeOne(a *Action, ctx *ExecContext) error { ... }

// evalExpr evaluates one boolean expression against the context.
func (s *Script) evalExpr(e *Expr, ctx *ExecContext) (bool, error) { ... }
```

### 3.2 Files to Modify

**Modify: `internal/core/proxy/proxy.go`**

Add a field to `ProxyCore`:

```go
type ProxyCore struct {
    ...
    script *script.Script       // new — compiled routing program
}

// SetScript installs a routing script. Passing nil removes it;
// when nil the engine falls back to the hardcoded default pipeline.
func (p *ProxyCore) SetScript(s *script.Script) { ... }
```

Then in the body of `ProcessRequest` (line ~480), add:

```go
// ---- Phase 27: dispatch through routing script if available ----
if p.script != nil {
    ctx := &script.ExecContext{
        Msg:    msg,
        SrcAddr: src,
        RURI:   extractRURI(msg),
        Vars:   make(map[string]string),
    }
    if err := p.script.Execute(ctx); err != nil {
        log.Warn("script error", log.String("err", err.Error()))
    }
    if ctx.Reply != nil {
        return ResponseAction{
            Status: ctx.Reply.Status,
            Reason: ctx.Reply.Reason,
            ExtraHeaders: ctx.Reply.Headers,
            StopRouting: true,
        }
    }
    if ctx.Drops {
        return ResponseAction{StopRouting: true}
    }
    if ctx.DstURI != "" {
        return ResponseAction{Target: ctx.DstURI}
    }
}
// ---- fallback to default pipeline below ----
```

Also add a helper `extractRURI(msg)` that parses the request-URI from `msg.FirstLine.Req.URI`.

### 3.3 Test File

**File: `internal/core/script/script_test.go`**

Use table-driven tests. Examples:

```go
// TestParseAndRun_Forward tests that a simple forward script runs correctly.
func TestParseAndRun_Forward(t *testing.T) {
    src := `request_route { if (method == "INVITE") { $du = "sip:10.0.0.1:5060"; return; } drop; }`
    s, err := script.ParseScript(src)
    if err != nil { t.Fatal(err) }

    msg := buildInviteMsg() // reuse helper from proxy tests
    ctx := &script.ExecContext{Msg: msg, Vars: map[string]string{}}
    if err := s.Execute(ctx); err != nil { t.Fatal(err) }

    if ctx.DstURI != "sip:10.0.0.1:5060" {
        t.Errorf("expected DstURI=sip:10.0.0.1:5060, got %q", ctx.DstURI)
    }
}

func TestParseAndRun_SendReply(t *testing.T) { ... }
func TestParseAndRun_SetFlag_Branch(t *testing.T) { ... }
func TestParseAndRun_NamedRoute(t *testing.T) { ... }
func TestParseAndRun_PVLookup(t *testing.T) {
    // verify $rU / $fU / $ci / $rm work via script expressions
}
func TestScript_ParseErrors(t *testing.T) {
    // malformed text should return a non-nil error
}
```

**Test count estimate:** 10 tests.

---

## 4. Phase 28 — Parallel / Serial Forking + DNS SRV

**Goal:** Allow `append_branch("sip:user@host")` followed by `t_relay()`, so one incoming INVITE goes out to N destinations in parallel; the first 2xx wins. Also add DNS SRV (RFC 2782) so `forward("sip:domain")` can resolve to weighted servers.

### 4.1 Files to Create

**File: `internal/core/fork/fork.go`**

```go
package fork

// Branch represents one outgoing call leg.
type Branch struct {
    URI      string          // target Request-URI
    RcvInfo  *transport.ReceiveInfo
    Status   int             // SIP code received so far, 0=unanswered
    Reason   string
    Took     time.Duration   // how long the branch took
    Dead     bool            // true on final 4xx/5xx/6xx
    Winner   bool            // true if this branch produced the 200 we accepted
}

// Forker coordinates parallel branches for a single incoming request.
type Forker struct {
    mu      sync.Mutex
    branches []*Branch
    done    chan struct{}     // closed when a winning 2xx is seen
    timeout time.Duration     // FR_INVITE timer
    cancel  context.CancelFunc
    winner  *Branch
}

func NewForker(timeout time.Duration) *Forker { ... }

// AddBranch appends a new target. Must be called before Start.
func (f *Forker) AddBranch(uri string) { ... }

// Start dispatches each branch to forwarder in its own goroutine.
// Callers block on WaitForWinner.
func (f *Forker) Start(ctx context.Context, core *proxy.ProxyCore, srcMsg *parser.SIPMsg) error { ... }

// WaitForWinner blocks until the first 2xx branch or timeout/error.
// Returns the winning branch (or nil if all failed).
func (f *Forker) WaitForWinner() (*Branch, []*Branch) { ... }

// Cancel aborts all in-flight branches (sends CANCEL on active legs).
func (f *Forker) Cancel() { ... }
```

**File: `internal/core/fork/serial.go`**

Serial forking (try branch one at a time). Simpler; mostly a loop that waits for each branch's outcome before trying the next.

```go
func SerialFork(ctx context.Context, core *proxy.ProxyCore, srcMsg *parser.SIPMsg, branches []string, perBranchTimeout time.Duration) (*Branch, error) { ... }
```

**File: `internal/core/dns/srv.go`**

```go
package dns

import "net"

// SRVTarget is one SRV record entry after weighting.
type SRVTarget struct {
    Target   string
    Port     int
    Priority uint16
    Weight   uint16
}

// ResolveSRV resolves `_sip._udp.<domain>` into ordered target list.
// RFC 2782 weight/priority ordering is implemented here (lowest priority first;
// within a priority, proportional weight random selection).
func (r *Resolver) ResolveSRV(service, proto, domain string) ([]SRVTarget, error) { ... }

// ResolveSIPHost auto-detects whether a host portion is an IP (no DNS),
// a plain domain (A/AAAA), or a prefixed SRV domain (_sip._udp.x).
// Returns a list of host:port candidates in priority order.
func (r *Resolver) ResolveSIPHost(hostport string) ([]string, error) { ... }
```

### 4.2 Files to Modify

**Modify: `internal/core/proxy/proxy.go`** — expose `Fork` method:

```go
func (p *ProxyCore) ForkRequest(msg *parser.SIPMsg, branches []string, parallel bool, timeout time.Duration) ResponseAction { ... }
```

**Modify: `internal/core/script/executor.go`** — add action handlers:

```go
ActionAppendBranch:   // append_branch("sip:...");
ActionSetDstURI:      // $du = "...";
ActionTForward:       // t_relay(); — dispatches Forker with branches collected
ActionTFork:          // t_fork("uri1", "uri2"); parallel
```

### 4.3 Test Files

**File: `internal/core/fork/fork_test.go`** — 6 tests (parallel happy path, all branches fail, first branch wins, cancel, serial, serial with early fail).

**File: `internal/core/dns/srv_test.go`** — 4 tests (SRV mock server, weighted ordering, IP passthrough, failure fallback to A record).

---

## 5. Phase 29 — Accounting / CDR Generation

**Goal:** Emit a call-detail record on every `INVITE → 200OK → BYE` lifecycle. Write CDRs to: (a) stdout/log, (b) CSV file, (c) in-memory DB via our `core/db` interface, (d) Redis pubsub channel.

### 5.1 Files to Create

**File: `internal/core/acc/acc.go`**

```go
package acc

// CDR holds the fields of a call detail record.
// Mirrors Kamailio's acc module defaults.
type CDR struct {
    CallID       string
    FromTag      string
    ToTag        string
    FromURI      string
    ToURI        string
    RequestURI   string
    SrcIP        string
    DstHost      string
    Direction    string // "in", "out", "local"
    Status       string // "ringing", "ok", "failed", "busy", "cancelled", "timeout"
    StatusCode   int
    InviteTime   time.Time
    ConnectTime  time.Time // when 200 OK seen — zero if never connected
    EndTime      time.Time // when BYE/cancel/timeout seen
    DurationSec  int
    Reason       string
    RTPEngineID  string // optional — media session handle
}

// Backend writes a single CDR somewhere; implementations are pluggable.
type Backend interface {
    Write(ctx context.Context, cdr *CDR) error
    Close() error
}

// AccountingService ties a dialog lifecycle to CDR generation.
// Usage: hook into ProxyCore via dialog callbacks.
type AccountingService struct {
    mu        sync.RWMutex
    backends  []Backend
    pending   map[string]*CDR  // keyed by call-id
}

func NewAccountingService(backends ...Backend) *AccountingService { ... }

// OnInvite records the start of a potential call.
func (a *AccountingService) OnInvite(msg *parser.SIPMsg, src net.Addr) { ... }

// OnReply tracks a reply for a call-in-progress.
func (a *AccountingService) OnReply(msg *parser.SIPMsg) { ... }

// OnBye finalizes a CDR.
func (a *AccountingService) OnBye(msg *parser.SIPMsg) { ... }

// OnCancel marks a call as cancelled.
func (a *AccountingService) OnCancel(msg *parser.SIPMsg) { ... }

// flush writes a completed CDR to every backend.
func (a *AccountingService) flush(cdr *CDR) { ... }
```

**File: `internal/core/acc/csv.go`**

```go
package acc

import "encoding/csv"

// CSVBackend writes CDRs to a file in CSV format. If the file is "stdout",
// it writes to os.Stdout.
type CSVBackend struct { ... }

func NewCSVBackend(path string) (*CSVBackend, error) { ... }
func (b *CSVBackend) Write(ctx context.Context, cdr *CDR) error { ... }
func (b *CSVBackend) Close() error { ... }
```

**File: `internal/core/acc/dbbe.go`** (DB + Redis backends, reuses core/db interface).

### 5.2 Files to Modify

**Modify: `internal/core/proxy/proxy.go`** — hook accounting into the pipeline:

```go
type ProxyCore struct {
    ...
    acc *acc.AccountingService
}
func (p *ProxyCore) SetAccounting(a *acc.AccountingService) { p.acc = a }

// In handleInvite (line ~588):
//   if p.acc != nil { p.acc.OnInvite(msg, src) }
// In handleBye (line ~614):
//   if p.acc != nil { p.acc.OnBye(msg) }
// In handleCancel (line ~627):
//   if p.acc != nil { p.acc.OnCancel(msg) }
// In ProcessReply (line ~554):
//   if p.acc != nil { p.acc.OnReply(msg) }
```

### 5.3 Test File

**File: `internal/core/acc/acc_test.go`** — 8 tests (build CDR from messages, compute duration, CSV round-trip, multi-backend dispatch, nil-backend safety, Redis pubsub via test DB).

---

## 6. Phase 30 — Rate Limiting + Topology Hiding

**Goal:** Two security features in one phase — (a) reject clients that exceed a registration/INVITE threshold, (b) strip and anonymize internal IPs/domains/tags before forwarding to an untrusted peer.

### 6.1 Files to Create

**File: `internal/core/pike/pike.go`**

```go
package pike

// Pike tracks per-source-IP request rates.
// C: `pike.c` — a simple hash of recent timestamps per IP.
type Pike struct {
    mu      sync.RWMutex
    ips     map[string]*ipStat
    limit   int           // max requests per window
    window  time.Duration // sampling window
    janitor *time.Ticker  // cleans old entries
}

type ipStat struct {
    count    int
    hits     []time.Time
    blocked  bool
    blockedUntil time.Time
}

func NewPike(limit int, window time.Duration) *Pike { ... }

// Hit records a request from `ip`. Returns true if the request should
// be allowed, false if the IP is over the threshold.
func (p *Pike) Hit(ip string) (allowed bool, remaining int, untilBlocked time.Duration) { ... }

// IsBlocked reports whether an IP is currently throttled.
func (p *Pike) IsBlocked(ip string) bool { ... }
```

**File: `internal/core/topoh/topoh.go`**

```go
package topoh

// HideStrategy configures which parts of a message get anonymized.
type HideStrategy struct {
    HideIPs      bool   // rewrite internal IPs to a fixed public IP
    HideDomains  bool   // rewrite internal domain to a realm
    HideTags     bool   // generate a new To/From tag, keep a 2-way mapping
    HideCallID   bool   // rewrite Call-ID
    Realm        string // public realm shown externally
    PublicIP     string // public IP shown in Contact/Via
}

// Hider anonymizes messages on send and reverses the mapping on receive.
type Hider struct {
    mu         sync.RWMutex
    strategy   HideStrategy
    tagMap     map[string]string // external → internal tag
    callIDMap  map[string]string // external → internal call-id
}

func NewHider(strategy HideStrategy) *Hider { ... }

// HideForForward rewrites headers on an outgoing request before forwarding.
func (h *Hider) HideForForward(msg *parser.SIPMsg) { ... }

// HideForReply rewrites headers on an outgoing reply.
func (h *Hider) HideForReply(reply *parser.SIPMsg) { ... }

// UnhideForProcessing reverses the mapping on an incoming message.
func (h *Hider) UnhideForProcessing(msg *parser.SIPMsg) { ... }
```

### 6.2 Files to Modify

**Modify: `internal/core/proxy/proxy.go`** — hook both:

```go
type ProxyCore struct {
    ...
    pike *pike.Pike
    topoh *topoh.Hider
}

// In ProcessRequest: after parsing, call
//   if p.pike != nil && !p.pike.Hit(extractIP(src)) {
//       return ResponseAction{Status: 503, Reason: "Too Many Requests"}
//   }
// Then, before forwarding:
//   if p.topoh != nil { p.topoh.HideForForward(msg) }
```

### 6.3 Test Files

**File: `internal/core/pike/pike_test.go`** — 5 tests (under limit passes, over limit blocks, expiry, blocked-until, concurrent access).

**File: `internal/core/topoh/topoh_test.go`** — 6 tests (IP rewrite, tag rewrite, call-id rewrite, round-trip unhide, empty strategy, concurrent access).

---

## 7. Phase 31 — Hash Tables + AVP Store

**Goal:** Implement `htable.c` equivalent — a script-accessible shared key-value store, plus the AVP (attribute-value pair) store that script expressions and DB lookups use.

### 7.1 Files to Create

**File: `internal/core/htable/htable.go`**

```go
package htable

// Table is a named in-memory hash table of string keys → string values,
// with optional per-key expiry. C: `htable.c`
type Table struct {
    mu        sync.RWMutex
    entries   map[string]*entry
    name      string
    defaultTTL time.Duration
}

type entry struct {
    value     string
    expiresAt time.Time // zero = no expiry
}

func NewTable(name string, defaultTTL time.Duration) *Table { ... }

func (t *Table) Set(key, value string) { ... }
func (t *Table) SetExpiring(key, value string, ttl time.Duration) { ... }
func (t *Table) Get(key string) (string, bool) { ... }
func (t *Table) Del(key string) { ... }
func (t *Table) Inc(key string, n int) int { ... }  // useful for counters
func (t *Table) Size() int { ... }

// Manager owns all named tables for the server.
type Manager struct {
    mu    sync.RWMutex
    tables map[string]*Table
}
func NewManager() *Manager { ... }
func (m *Manager) Get(name string) *Table { ... }
func (m *Manager) Create(name string, defaultTTL time.Duration) *Table { ... }
```

**File: `internal/core/avp/store.go`**

```go
package avp

// Store is a per-request AVP store. AVPs live for the duration of one
// request/reply. They are indexed by name (string) and have a typed value.
// C: `struct avp` in avp.c
type Store struct {
    mu   sync.RWMutex
    data map[string][]Value  // same name can have multiple values
}

type Value struct {
    S string  // for $avp(name)
    I int64   // for $avp(name:i)
    IsInt bool
}

func NewStore() *Store { ... }
func (s *Store) Add(name string, v Value) { ... }
func (s *Store) First(name string) (Value, bool) { ... }
func (s *Store) All(name string) []Value { ... }
func (s *Store) Del(name string) { ... }
```

### 7.2 Files to Modify

**Modify: `internal/core/script/executor.go`** — add PV lookups `$ht(name=>key)` and `$avp(name)`.

### 7.3 Test Files

**File: `internal/core/htable/htable_test.go`** — 5 tests.

**File: `internal/core/avp/avp_test.go`** — 4 tests.

---

## 8. Phase 32 — Persistent DB Drivers + Auth DB Lookup

**Goal:** Add SQLite and PostgreSQL drivers to `core/db`, and wire `digest.go` to fetch credentials via a DB query.

### 8.1 Files to Create

**File: `internal/core/db/db_sqlite.go`** — implements `Database` interface using `modernc.org/sqlite` (pure Go, no CGO).

**File: `internal/core/db/db_postgres.go`** — implements `Database` interface using `pgx`.

Both provide the same CRUD API as `db_memory.go`/`db_redis.go`:

```go
func NewSQLiteDB(path string) (Database, error) { ... }
func NewPostgresDB(connString string) (Database, error) { ... }
```

### 8.2 Files to Modify

**Modify: `internal/core/auth/digest.go`** — add DB-backed credential lookup:

```go
type AuthStore interface {
    GetUser(ctx context.Context, username string) (ha1 string, err error)
}

type InMemoryStore map[string]string

func (m InMemoryStore) GetUser(_ context.Context, u string) (string, error) {
    if ha1, ok := m[u]; ok { return ha1, nil }
    return "", errors.New("not found")
}

type DBAuthStore struct {
    db     db.Database
    table  string   // e.g. "subscriber"
    userCol string   // e.g. "username"
    ha1Col  string   // e.g. "ha1"
}
func (d *DBAuthStore) GetUser(ctx context.Context, u string) (string, error) {
    rows, err := d.db.Query(ctx, d.table, db.QueryCondition{Field: d.userCol, Value: u}, 1)
    if err != nil { return "", err }
    if len(rows) == 0 { return "", errors.New("not found") }
    v, ok := rows[0][d.ha1Col]
    if !ok { return "", errors.New("missing column") }
    return v.String(), nil
}
```

### 8.3 Test File

**File: `internal/core/auth/digest_db_test.go`** — 5 tests (valid user, wrong user, wrong password, stale nonce, concurrent lookups).

---

## 9. Phase 33 — Full NAT / Media Rewrite Pipeline

**Goal:** Make `nat.go`'s Contact rewriting coordinate with `rtpengine.go` so SDP bodies are rewritten in lockstep with SIP headers. The resulting behavior matches `nathelper.c` + `rtpengine` in C-Kamailio.

### 9.1 Files to Create

**File: `internal/core/nat/media.go`**

```go
package nat

import (
    "github.com/kamailio/kamailio-go/internal/core/parser"
    "github.com/kamailio/kamailio-go/internal/core/rtpengine"
)

// MediaPipeline applies Contact and SDP rewriting for a NAT'ed call.
type MediaPipeline struct {
    engine *rtpengine.RTPEngineClient
    mu     sync.Mutex
}

func NewMediaPipeline(engine *rtpengine.RTPEngineClient) *MediaPipeline { ... }

// OnInviteOffer runs RTPengine "offer" command with the message's SDP body,
// then replaces the body with the rewritten SDP.
func (m *MediaPipeline) OnInviteOffer(msg *parser.SIPMsg, callID, fromTag string) error { ... }

// OnAnswer handles the 200OK-to-INVITE by calling RTPengine "answer" and
// replacing the body with the rewritten SDP.
func (m *MediaPipeline) OnAnswer(msg *parser.SIPMsg, callID, fromTag, toTag string) error { ... }

// OnBye calls RTPengine "delete" to release the media session.
func (m *MediaPipeline) OnBye(msg *parser.SIPMsg, callID, fromTag, toTag string) error { ... }
```

### 9.2 Files to Modify

**Modify: `internal/core/proxy/proxy.go`** — install media pipeline alongside existing NAT handling.

### 9.3 Test File

**File: `internal/core/nat/media_test.go`** — 5 tests (offer round-trip with mock RTPengine, answer, delete, SDP body size preservation, missing body returns clear error).

---

## 10. Phase 34 — IMS / Presence + MESSAGE msilo

**Goal:** Expand IMS with full route-from-registration pipeline; add msilo for offline MESSAGE store-and-forward; improve Presence with PUBLISH-notify pipeline.

### 10.1 Files to Create

**File: `internal/ims/pcscf/route_from_reg.go`**

```go
// RouteFromRegistration derives the next-hop URI from a user's registration
// record (usrloc AOR). Returns a list of contact URIs (possibly multiple
// if multi-register) suitable for forking.
func RouteFromRegistration(registrar *usrloc.Registrar, domain string, aor string) ([]string, error) { ... }
```

**File: `internal/core/msilo/msilo.go`**

```go
package msilo

// StoredMessage is one offline MESSAGE queued for a user.
type StoredMessage struct {
    ID        string
    To        string
    From      string
    Body      string
    Timestamp time.Time
    Tries     int
}

// Msilo is the message silo store-and-forward queue.
type Msilo struct {
    mu    sync.RWMutex
    queue map[string][]*StoredMessage  // key: recipient AOR
    db    db.Database                  // optional persistent backing store
}

func NewMsilo(db db.Database) *Msilo { ... }
func (m *Msilo) Store(msg *parser.SIPMsg) error { ... }
func (m *Msilo) DeliverFor(user string, max int) []*StoredMessage { ... }
func (m *Msilo) MarkDelivered(id string) { ... }
```

### 10.2 Files to Modify

**Modify: `internal/core/presence/server.go`** — extend PUBLISH handler to auto-fire NOTIFY on state change; integrate with usrloc.

### 10.3 Test Files

**File: `internal/ims/pcscf/route_from_reg_test.go`** — 4 tests.

**File: `internal/core/msilo/msilo_test.go`** — 4 tests.

---

## 11. Phase 35 — Runtime RPC + Pipeline Glue Release

**Goal:** Provide a `kamcmd`-equivalent: a JSON-RPC over HTTP endpoint that lets operators read counters, clear pike blocks, view active dialogs, and hot-reload routing scripts. Then tie everything (script + pike + topoh + htable + acc + media + msilo) together as a coherent pipeline in one demo configuration file.

### 11.1 Files to Create

**File: `internal/core/rpc/rpc.go`**

```go
package rpc

// Server exposes a JSON-RPC 2.0 endpoint. Methods:
//
//   kamailio.stats          → return Metrics snapshot
//   kamailio.dialog.list    → return N active dialogs
//   kamailio.pike.status    → return blocked IPs
//   kamailio.pike.clear(ip) → clear a block
//   kamailio.script.reload(text) → hot-reload routing script
//   kamailio.shutdown       → graceful stop
type Server struct { ... }

func NewServer(core *proxy.ProxyCore, dialogs *dialog.Manager, pike *pike.Pike, acc *acc.AccountingService) *Server { ... }
func (s *Server) ListenAndServe(addr string) error { ... }
func (s *Server) Shutdown(ctx context.Context) error { ... }
```

**File: `configs/demo-full.cfg`** — example routing script text demonstrating every Phase 27-34 feature in one deployment.

### 11.2 Files to Modify

**Modify: `cmd/kamailio/main.go`** — add `run --rpc-addr :7000 --config demo-full.cfg` flag handling.

**Modify: `internal/core/app/bootstrap.go`** — `NewBootstrap` now accepts the full config (script path, pike thresholds, topoh strategy, CDR output path, htable names, db connection string) and wires the entire pipeline into `ProxyCore`.

### 11.3 Test File

**File: `internal/integration/full_pipeline_test.go`** — 5 end-to-end tests (register, invite via script, pike block test, topology hiding round-trip, CDR output).

---

## 12. Test Design Principles (reminder for implementers)

- **Table-driven tests** are the default pattern — see any `_test.go` already in the project.
- **Mock servers, not real infrastructure** — use `httptest.NewServer` for HTTP, and `net.Listen("tcp", "127.0.0.1:0")` for raw protocol mocks (as `rtpengine_integration_test.go` already does).
- **Time-sensitive tests** (flood detection, CDR duration, rate limit) use an injectable `time.Now` or `time.After` hook — **never** use `time.Sleep` with real timing.
- **Every new public type gets a zero-value-safety test** (`nil receiver`, empty map, empty slice) to match the defensive pattern already used throughout `proxy.go`.
- **Integration tests live in `internal/integration/`**; unit tests live alongside source files in the same package.

---

## 13. Delivery Checklist per Phase

Every phase ends with:

- [ ] `go build ./...` — clean build, no new warnings
- [ ] `go test ./...` — all tests pass including pre-existing ones
- [ ] New tests exercise: nominal path, error path, nil-safety, concurrent access
- [ ] Code review: at least one read-through for logic clarity and script-expression hygiene
- [ ] `cmd/kamailio` binary runs the new feature end-to-end with a demo config

---

## 14. Summary of Gaps by Phase

```
Phase  | C-Module Equivalent        | Files Added                | Est. New Tests
-------|---------------------------|---------------------------|---------------
27     | route.c / action.c        | internal/core/script/*.go | 10
28     | t_fork.c / dns_srv.c      | internal/core/fork/*.go, internal/core/dns/srv.go | 10
29     | acc.c / acc_db.c          | internal/core/acc/*.go    | 8
30     | pike.c + topoh.c / topos.c| internal/core/pike/pike.go + internal/core/topoh/topoh.go | 11
31     | htable.c + avpops.c       | internal/core/htable + avp | 9
32     | db_sqlite + db_postgres + auth_db.c | internal/core/db/db_sqlite.go + db_postgres.go, internal/core/auth/digest_db_test.go | 7
33     | nathelper + rtpengine integration | internal/core/nat/media.go | 5
34     | msilo.c + presence_xml / pua | internal/core/msilo/*.go, internal/ims/pcscf/route_from_reg.go | 8
35     | ctl.c / jsonrpcs.c        | internal/core/rpc/*.go, configs/demo-full.cfg | 5
Total  | —                         | ~15 new files, 8 modified   | 73
```

After all 9 phases, kamailio-go becomes a production-ready SIP proxy with a programmable routing script, parallel forking, accounting CDRs, flood protection, topology hiding, hash tables, persistent DB backends, full NAT media rewriting, IMS registration routing, offline message store, and a runtime RPC interface.

---

## 15. Where This Document Lives

Path in repo: [`docs/superpowers/plans/2026-06-21-complete-sip-stack-roadmap.md`](file:///workspace/kamailio-go/docs/superpowers/plans/2026-06-21-complete-sip-stack-roadmap.md)

This is a **living document** — as each Phase is completed, strike through the relevant checklist items and note any deviations in an appendix below.

### Implementation Notes Added During Execution

_(Empty — to be filled in as phases ship.)_

---
