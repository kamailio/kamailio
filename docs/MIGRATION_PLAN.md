# Kamailio C → Go 迁移计划

> 适用于 porting-go 分支 | 3GPP IMS (VoLTE/VoNR) 场景

---

## 1. 架构总览

### 1.1 当前 C 代码库架构

Kamailio（SIP Express Router）是一个面向运营商场景的 SIP 信令服务器，核心架构如下：

```
┌─────────────────────────────────────────────────────────────┐
│                   主进程 main.c                            │
│  ┌──────────┐  初始化 config/alloc/sockets/child procs    │
└──┴──────────┴───────────────────────────────────────────────┘
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
   UDP 子进程    TCP/TLS 子进程   SCTP 子进程
   (多个fork)    (多个fork)      (可选)
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                   SIP 消息处理管线                          │
│  udp_rcv_loop() → parse_msg() → route 脚本 → 转发/事务      │
└─────────────────────────────────────────────────────────────┘
                       │
        ┌──────────────┴──────────────┐
        ▼                             ▼
┌─────────────────┐          ┌────────────────────┐
│ 共享内存 SHM    │          │ 私有内存 PKG       │
│ (进程间共享)    │          │ (进程内/请求级)   │
│ shm_malloc/free │          │ pkg_malloc/free    │
└─────────────────┘          └────────────────────┘
```

**核心架构特征：**

| 特征 | 说明 |
|------|------|
| **多进程模型** | Master 进程 fork N 个子进程，每个子进程独立处理消息 |
| **双内存池** | SHM（进程间共享，如事务表）+ PKG（进程内请求级分配） |
| **显式内存管理** | 全部手动 malloc/free，无 GC |
| **无 GC** | 纯手动内存管理 |
| **无 goroutine** | 每个 UDP 子进程是单线程事件循环 |
| **Socket 共享** | 所有子进程共享监听 socket，通过内核调度 |
| **配置脚本** | flex/bison 自定义脚本语言解析执行 |
| **模块动态加载** | `.so` 动态库 dlopen/dlsym |
| **Zero-copy 解析** | SIP 头字段指针指向原始 buffer，不做字符串复制 |

### 1.2 3GPP IMS 协议背景

IMS（IP Multimedia Subsystem）是 3GPP Release 5 引入的架构，核心要点：

**IMS 核心网元（本迁移重点关注）：**

| 网元 | 功能 | SIP 角色 |
|------|------|---------|
| **P-CSCF** (Proxy-CSCF) | UE 接入 IMS 的第一个接触点，SIP 消息转发、拓扑隐藏、QoS 绑定 | 入站 Proxy |
| **I-CSCF** (Interrogating-CSCF) | 归属网络入口，查询 HSS 分配 S-CSCF，隐藏内部拓扑 | 入口 Proxy |
| **S-CSCF** (Serving-CSCF) | 核心会话控制器，用户注册绑定、事务处理、iFC 业务触发、鉴权授权 | Registrar + Proxy |
| **HSS** (Home Subscriber Server) | 中央用户数据库（IMPI/IMPU、鉴权向量、签约数据） | Diameter 接口 (Cx) |

**IMS 关键流程（SIP 层面）：**

1. **REGISTER 流程**（用户注册）
   - UE → P-CSCF → I-CSCF → S-CSCF（初次注册分配）
   - S-CSCF 与 HSS 交互（MAR/SAR，Diameter）下载鉴权向量
   - 401 + Authentication-Info / WWW-Authenticate（AKA 挑战）
   - UE 计算 RES 后重新 REGISTER → 通过后 200 OK
   - Contact 头绑定用户可达地址

2. **INVITE 流程**（会话建立）
   - UE → P-CSCF → S-CSCF（执行 iFC 触发 AS）→ 对端 S-CSCF → 对端 P-CSCF → 对端 UE
   - SDP Offer/Answer 协商媒体
   - 183/180/181 Provisional + PRACK → 200 OK (INVITE) → ACK

3. **IMS 特有 SIP 头字段**
   - `P-Asserted-Identity` (PAI) - 已断言身份（RFC 3325）
   - `P-Preferred-Identity` (PPI) - 首选身份（RFC 3325）
   - `Privacy` - 隐私指示（RFC 3323）
   - `P-Charging-Vector` / `P-Charging-Function-Addresses` - 计费向量
   - `Path` - 注册路径记录（RFC 3327）
   - `Service-Route` - 业务路由（RFC 3608）
   - `Require: path` - Path 能力协商

**3GPP 规范参考：**

- TS 23.002: Network Architecture
- TS 23.228: IP Multimedia Subsystem (IMS)
- TS 24.229: IMS Call control; Session Initiation Protocol (SIP)
- TS 24.229 Annex A: SIP and SDP usage in IMS
- RFC 3261: SIP Core Protocol
- RFC 3325: Private Extensions to the SIP (PAI/PPI)
- RFC 3327: Path Header
- RFC 4474/8826: Identity Assertion

**SIP 消息示例（IMS REGISTER）：**

```
REGISTER sip:ims.mnc001.mcc460.gprs SIP/2.0
Via: SIP/2.0/UDP [2001:db8::1]:5060;branch=z9hG4bK...
Max-Forwards: 70
From: <sip:460000123456789@ims.mnc001.mcc460.gprs>;tag=...
To: <sip:460000123456789@ims.mnc001.mcc460.gprs>
Call-ID: ...
CSeq: 1 REGISTER
Contact: <sip:460000123456789@[2001:db8::1]:5060;ob>;+sip.instance="<urn:uuid:...>"
Expires: 600000
Require: path, outbound
Supported: path, outbound, gruu
P-Access-Network-Info: 3GPP-UTRAN-TDD;utran-cell-id-3gpp=4600001234ABCD
P-Visited-Network-ID: "vplmn.ims.mnc001.mcc460.gprs"
Content-Length: 0
```

---

## 2. Go 迁移架构设计

### 2.1 设计原则

1. **分阶段迁移**：先骨架后核心，先解析后路由，先单进程后并发
2. **保持结构对齐**：Go 包结构与 C 目录结构保持对应关系
3. **零 Go goroutine 滥用**：C 的单进程事件循环 → Go 的显式事件循环
4. **Go 原生内存模型**：不模拟 SHM/PKG，用 Go 内存模型替代
5. **SIP 协议精确性优先**：逐字符解析、头字段处理与 C 原代码行为一致
6. **可测试性**：每个模块独立可测试，纯函数优先

### 2.2 包结构映射

| C 路径 | Go 包 | 功能 | 优先级 |
|--------|-------|------|--------|
| `src/core/str.{c,h}` | `core/str` | 定长字符串结构 | P0 |
| `src/core/parser/` | `core/parser` | SIP 头字段解析 | P0 |
| `src/core/msg_parser.{c,h}` | `core/msg` | 消息结构 & 总解析 | P0 |
| `src/core/ip_addr.{c,h}` | `core/netutil` | IP/端口/协议处理 | P0 |
| `src/core/hash_func.{c,h}` | `core/hash` | 哈希算法（Call-Id/branch） | P0 |
| `src/core/udp_server.{c,h}` | `core/transport` | UDP 收发 + 事件循环 | P0 |
| `src/core/tcp_main.{c,h}` | `core/transport` | TCP/TLS 收发 | P1 |
| `src/core/forward.{c,h}` | `core/forward` | 请求转发（stateless） | P1 |
| `src/core/route.{c,h}` | `core/route` | 路由脚本执行引擎 | P1 |
| `src/core/cfg/` | `core/cfg` | 配置管理 | P1 |
| `src/core/mem/` | **省略** | Go GC 替代 SHM/PKG | - |
| `src/core/dprint.{c,h}` | `core/log` | 日志系统 | P0 |
| `src/core/sr_module.{c,h}` | `core/module` | 模块系统（Go interface） | P2 |
| `src/core/timer.{c,h}` | `core/timer` | 定时器（事务超时等） | P1 |
| `src/core/receive.{c,h}` | `core/receive` | 消息接收路由 |
| `src/modules/tm/` | `modules/tm` | Transaction Management | P2 |

### 2.3 Go 进程模型设计

```
┌────────────────────────────────────────────────────────────┐
│                      Go 主 goroutine                      │
│                    config load + setup                    │
└────────────────────────────────────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
┌───────────────┐ ┌──────────────────┐ ┌────────────────┐
│ UDP Listener  │ │ TCP Listener     │ │ SCTP Listener  │
│ (goroutine)   │ │ (goroutine)      │ (goroutine/可选) │
└───────────────┘ └──────────────────┘ └────────────────┘
        │                   │                   │
        ▼                   ▼                   ▼
┌────────────────────────────────────────────────────────────┐
│                       Worker Pool                          │
│   chan sip_msg_t → worker goroutine → parse → route → send  │
└────────────────────────────────────────────────────────────┘
```

### 2.4 核心数据结构迁移方案

#### `str` → Go `string` + `Str` 辅助类型

```go
package core

// Str 保留原始指针语义，用于 zero-copy 解析场景
// C: struct _str { char *s; int len; }
type Str struct {
    S   []byte // 指向原始 buffer 的切片（不复制）
    Len int    // 显式长度（与 C 语义一致）
}

// String 返回 Go string（做复制，安全使用）
func (s Str) String() string {
    if s.Len == 0 || s.S == nil {
        return ""
    }
    return string(s.S[:s.Len])
}

// Equal 对应 C 的 STR_EQ 宏
func (s Str) Equal(other Str) bool {
    if s.Len != other.Len {
        return false
    }
    return bytes.Equal(s.S[:s.Len], other.S[:other.Len])
}
```

**设计决策：** 对于 SIP 头字段解析，保留 `[]byte` slice 引用原始 buffer 以保持 zero-copy 性能；对外暴露 API 使用 `string`。

#### `sip_msg_t` → Go `SIPMsg`

```go
package core

// SIPMsg 对应 C 的 struct sip_msg
type SIPMsg struct {
    ID          uint32      // message id, unique per process
    PID         int         // process id (Go: goroutine id 或 0)
    ReceivedAt  time.Time   // struct timeval tval → time.Time

    FirstLine   MsgStart    // 首行: request/reply
    Via1        *ViaBody    // first via
    Via2        *ViaBody    // second via
    Headers     []HdrField  // all parsed headers
    LastHeader  *HdrField   // pointer to last parsed header
    ParsedFlag  uint64      // 已解析头字段类型 bitmap

    // 快速引用（与 C 字段一一对应）
    HdrVia1        *HdrField
    HdrVia2        *HdrField
    CallID         *HdrField
    To             *HdrField
    CSeq           *HdrField
    From           *HdrField
    Contact        *HdrField
    MaxForwards    *HdrField
    Route          *HdrField
    RecordRoute    *HdrField
    ContentType    *HdrField
    ContentLength  *HdrField
    Authorization  *HdrField
    Expires        *HdrField
    ProxyAuth      *HdrField
    Supported      *HdrField
    Require        *HdrField
    ProxyRequire   *HdrField
    Allow          *HdrField
    Event          *HdrField
    PAI            *HdrField  // P-Asserted-Identity (IMS 关键)
    PPI            *HdrField  // P-Preferred-Identity (IMS 关键)
    Privacy        *HdrField  // Privacy (IMS 关键)
    Identity       *HdrField  // Identity (RFC 8826)
    IdentityInfo   *HdrField  // Identity-Info
    Diversion      *HdrField  // Diverted calling info
    // ... 其他头字段引用

    Body        *MsgBody     // SDP/Body 解析

    Buf         []byte       // scratch pad: 原始/修改后的消息 buffer
    Len         int          // 原始消息长度
    BufSize     int          // buffer 容量

    // 路由相关
    NewURI      Str          // 新 R-URI
    DstURI      Str          // 转发目的 URI
    ParsedURI   *SipURI      // 缓存的解析 URI（与 C parsed_uri_ok 对应）

    MsgFlags    MsgFlags     // 内部标志位（与 C msg_flags_t 对应）
    Flags       uint32       // 配置脚本标志
}
```

#### `sip_uri_t` → Go `SIPURI`

```go
package core

// SIPURI 对应 C 的 struct sip_uri
// 支持 sip: / sips: / tel: URI 方案
type SIPURI struct {
    User      string // username
    Passwd    string // password
    Host      string // host name / IP
    Port      uint16 // port number (0 if absent)
    Type      URIType // SIP_URI_T / SIPS_URI_T / TEL_URI_T

    // 原始参数切片（zero-copy 指向 Buf）
    Transport     Str
    TTL           Str
    UserParam     Str
    MAddr         Str
    Method        Str
    LR            Str // lr 参数（rfc3261 路由标志）
    GR            Str // gruu

    // 已解析的参数字典
    Params map[string]string // generic params (key=value pairs)
}
```

#### 头字段 `hdr_field_t` → Go `HdrField`

```go
package core

// HdrField 对应 C 的 struct hdr_field
// 代表单个 SIP 头字段，parsed 指向类型特定的解析结果
type HdrField struct {
    Name     Str        // 头字段名称 (e.g. "Via")
    Body     Str        // 头字段值 (逗号分隔的主体部分)
    Type     HdrType    // HDR_VIA / HDR_TO / HDR_FROM ...
    Parsed   interface{} // *ViaBody / *ToBody / *FromBody / *ContactBody ...
    Next     *HdrField  // 同类型下一个头字段（C: hdr_field.next）
}
```

---

## 3. 迁移阶段与里程碑

### 阶段 0：基础设施（预计 2 周）

**目标：建立 Go 项目骨架、构建系统、基础工具**

| 任务 | 交付物 |
|------|--------|
| 初始化 Go module | `go.mod` (module: `github.com/kamailio/kamailio-go`) |
| 目录结构约定 | `internal/core/`, `internal/core/parser/`, `internal/modules/` |
| Makefile / CI | `make build`, `make test`, GitHub Actions |
| 日志系统 | `internal/core/log/` - 结构化日志（zap） |
| 基础字符串工具 | `internal/core/str/` - `Str` 类型 + 工具函数 |
| 哈希工具 | `internal/core/hash/` - 与 C `hash_func.c` 对齐 | |

### 阶段 1：SIP 协议解析层（预计 4 周）

**目标：完整解析 SIP 请求/响应，通过所有现有测试用例**

**子任务：**

1. **首行解析**（`parse_fline.c`）: `parseFirstLine([]byte) (*MsgStart, error)`
   - Request: `METHOD URI SIP/2.0` → method enum + URI string
   - Response: `SIP/2.0 STATUS_CODE REASON-PHRASE`
   - Methods: INVITE, ACK, BYE, CANCEL, REGISTER, OPTIONS, PRACK, SUBSCRIBE, NOTIFY, UPDATE, REFER, MESSAGE, INFO, PUBLISH

2. **头字段解析核心**（`parse_hname2.c`）: 头字段类型识别
   - 快速识别: "Via" / "From" / "To" / "Call-ID" / "CSeq" / "Contact" 等标准头
   - IMS 扩展头: "P-Asserted-Identity" / "P-Preferred-Identity" / "Privacy" / "Path" / "P-Charging-Vector" / "P-Access-Network-Info"
   - 逐字符扫描: 与 C 原代码行为一致，大小写敏感处理

3. **URI 解析**（`parse_uri.c`）: `parseURI([]byte) (*SIPURI, error)`
   - `sip:user@host:port;params?headers`
   - `sips:user@host:port;params`
   - `tel:+8613800138000;phone-context=+86`
   - 参数解析: `transport=udp`, `user=phone`, `lr`, `gr`, `ob` 等

4. **各头字段专用解析**:
   - Via: `parse_via.c` → `ViaBody{sentBy, branch, received, rport, params}`
   - From/To: `parse_to.c` → `ToBody{displayName, URI, tag, params}`
   - Contact: `parse_contact.c` → `ContactBody{displayName, URI, expires, q-value, instance, reg-id}`
   - CSeq: `parse_cseq.c` → `CSeqBody{method, number}`
   - Authorization/Proxy-Authenticate: `digest/` → AKAv1-MD5 支持
   - PAI/PPI: `parse_ppi_pai.c` → 身份断言 URI
   - Supported/Require: `parse_supported.c` → feature tokens（path, outbound, gruu, path）
   - Record-Route/Route: `parse_rr.c` → URI list

5. **Body/SDP 解析**（`src/core/parser/sdp/`）: 可选，先框架

**测试策略：**
- 使用 Kamailio 现有测试用例的 SIP 消息样本
- 增加 IMS 特定消息样本（REGISTER 带 P-Access-Network-Info、INVITE 带 PAI）
- 测试 C 与 Go 解析结果逐字段对比

### 阶段 2：传输层（预计 3 周）

**目标：UDP/TCP 接收发送，基本事件循环**

| 组件 | C 文件 | Go 实现 |
|------|--------|--------|
| UDP 收发 | `udp_server.c` | `internal/core/transport/udp.go` |
| TCP 收发 | `tcp_main.c` | `internal/core/transport/tcp.go` |
| TLS/DTLS | `tls_init.c` | `crypto/tls` + `github.com/pion/dtls` |
| Socket 信息 | `socket_info.c` | `SocketInfo{proto, address, port}` |
| 目的地址信息 | `ip_addr.c` | `DestInfo{proto, dstAddr, sendSocket}` |

**关键设计点：**

```go
package transport

type Listener interface {
    ListenAndServe(handler MessageHandler) error
    Shutdown(ctx context.Context) error
}

type UDPListener struct {
    Addr    *net.UDPAddr
    BufSize int  // 与 C MAX_RECV_BUFFER_SIZE 对齐
}

func (u *UDPListener) ListenAndServe(handler MessageHandler) error {
    conn, err := net.ListenUDP("udp", u.Addr)
    if err != nil {
        return err
    }
    defer conn.Close()

    buf := make([]byte, u.BufSize)
    for {
        n, remoteAddr, err := conn.ReadFromUDP(buf)
        if err != nil {
            if isTemporary(err) {
                continue
            }
            return err
        }
        // NOTE: 为避免 goroutine 过多，使用 worker pool
        handler.HandleMessage(buf[:n], remoteAddr, ProtoUDP)
    }
}
```

### 阶段 3：消息转发与路由（预计 4 周）

**目标：实现 stateless 转发、基本路由逻辑**

1. **Stateless 转发**（`forward.c`）: `forwardRequest(msg, dst) error`
   - Via 头添加/移除/重写（top via + branch）
   - Record-Route 头处理
   - Max-Forwards 递减
   - 目的地址 DNS 解析（`resolve.c` → Go `net.Lookup*`）
   - Socket 选择（send socket selection）

2. **消息重写**（`msg_translator.c`）:
   - Via 添加/修改
   - Contact 重写（nat 场景）
   - Record-Route 添加
   - Content-Length 更新

3. **路由引擎骨架**（`route.c`）:
   - Go 原生 DSL（非脚本语言）
   - `type RouteFunc func(*SIPMsg) RouteAction`
   - `RouteAction { Drop, Relay, Reply, Route(name) }`
   - 与现有 C 脚本语义对应

4. **事务管理 (TM) 基础**（`modules/tm/`）:
   - 事务表（Go map，替代 C 的 shared memory hash table）
   - INVITE 事务状态机（Calling/Proceeding/Completed/Terminated）
   - 非 INVITE 事务状态机（Trying/Proceeding/Completed/Terminated）
   - 重传处理（UDP 场景）

### 阶段 4：IMS 特性层（预计 3 周）

**目标：支持 P-CSCF/I-CSCF/S-CSCF 行为**

**关键 IMS 能力：**

| 功能 | C 实现位置 | 对应 3GPP 规范 |
|------|------------|---------------|
| PAI/PPI 处理 | 现有头字段解析 | RFC 3325 / TS 24.229 |
| Privacy 处理 | Privacy 头字段解析 | RFC 3323 |
| Path 头处理 | Path 头字段 + Route 组装 | RFC 3327 |
| Service-Route 处理 | | RFC 3608 |
| 注册绑定 | S-CSCF 逻辑 | TS 24.229 5.2 |
| 业务触发 (iFC) | 需要新增 | TS 24.229 Annex B |

**IMS REGISTER 流程伪代码：**

```
UE → P-CSCF → I-CSCF → S-CSCF
          │
          ├─ 添加 P-Visited-Network-ID
          ├─ 添加 Path (记录自身，用于后续 INVITE 路由)
          ├─ 验证 P-Access-Network-Info
          │
S-CSCF 处理 REGISTER:
  ├─ 查询 HSS (MAR/SAR via Diameter Cx) 获取鉴权向量
  ├─ 生成 401 WWW-Authenticate: Digest AKA challenge
  ├─ 接收带 Authorization 头的 REGISTER
  ├─ 验证 MAC/SQN
  ├─ 绑定 Contact → 注册位置
  ├─ 生成 Service-Route (指向下一跳 S-CSCF)
  └─ 返回 200 OK
```

**Go 实现要点：**

- `internal/ims/auth/`: AKA (Authentication and Key Agreement) - `AKAv1-MD5` digest
- `internal/ims/scscf/`: Serving-CSCF 逻辑（注册、会话路由、iFC 触发）
- `internal/ims/pcscf/`: Proxy-CSCF 逻辑（UE 接入点、拓扑隐藏）
- `internal/ims/icscf/`: Interrogating-CSCF 逻辑（HSS 查询、S-CSCF 分配）

### 阶段 5：配置与脚本系统（预计 3 周）

**目标：可配置、可扩展的路由规则系统**

**方案选择：**

| 方案 | 优点 | 缺点 | 推荐 |
|------|------|------|------|
| **Go Native DSL** | 编译时检查、高性能、零依赖 | 需写 Go 代码 | ⭐⭐⭐ |
| **YAML/JSON 配置** | 简单、标准格式 | 表达力有限 | ⭐⭐ |
| **Lua 嵌入** | 灵活、动态可改 | 依赖外部库、性能损耗 | ⭐ |

**推荐 Go Native DSL 示例**（类似 Caddyfile v2 风格）：

```go
// config/routes.go
func initRoutes() route.Router {
    return route.NewRouter().
        // REGISTER 路由 - 走 IMS 注册逻辑
        Match(route.IsMethod("REGISTER"), route.IMSRegister()).
        // INVITE 路由 - 按 Request-URI 路由
        Match(route.IsMethod("INVITE"), route.ToUserDBLookup()).
        // 默认转发
        Default(route.Relay())
}
```

---

## 4. 关键技术挑战与解决方案

### 4.1 内存模型：SHM/PKG → Go GC

**挑战**：C 代码手动区分 SHM（进程间共享）与 PKG（进程内私有），在 Go 中不需要也不应该模拟。

**解决方案**：
- **PKG 内存** → Go 的 `make/new` + GC 自动回收。请求级分配天然被 GC 管理
- **SHM 内存** →
  - 场景 A（事务表等高频数据）：使用 Go 的 `sync.Map` 或 `map + sync.RWMutex`
  - 场景 B（配置共享）：Go 全局变量 + `sync.Once` 或 `atomic.Value`
  - 场景 C（大规模共享状态）：使用 `github.com/allegro/bigcache` 或类似内存缓存库
- **零 copy 解析** → Go slice 天然支持对底层 `[]byte` 的切片引用，无需模拟 C 的指针操作

### 4.2 并发模型：多进程 fork → Go goroutine

**挑战**：C 使用 fork 多进程实现多核利用，Go 使用 goroutine。

**解决方案**：

```go
// worker pool 模式
type WorkerPool struct {
    jobs    chan Job
    results chan Result
    workers int
}

func (wp *WorkerPool) Start() {
    for i := 0; i < wp.workers; i++ {
        go func(id int) {
            for job := range wp.jobs {
                job.Process(id)
            }
        }(i)
    }
}
```

**关键约束**：
- 每个 SIP 消息的处理是串行的（保持 C 原代码单线程事件循环语义）
- 不同消息/事务并行处理
- 共享数据结构必须线程安全（`sync.Map`, `atomic`, `sync.RWMutex`）

### 4.3 Zero-Copy 解析安全

**挑战**：C 代码中头字段指针直接指向原始 buffer，生命周期依赖 buffer 不被释放。

**解决方案**：
- **Phase A**：保持与 C 相同的指针语义，`Str.S` 为 `[]byte` 指向底层 buffer
- **Phase B**：确保消息处理完成前 buffer 不被 GC 回收（通过持有引用）
- **安全检查**：`go vet` + `go build -race` 检测并发问题
- **可选优化**：对需要跨请求保留的字段，显式 `string()` 复制

### 4.4 头字段顺序与重复

**挑战**：SIP 中某些头字段允许多个实例（Via, Record-Route, Route, Contact），顺序有语义。

**解决方案**：
- 使用 Go `[]HdrField` slice 保存所有头字段
- 提供 `GetFirstHeader(type)` / `GetAllHeaders(type)` / `GetHeaderByName(name)` 方法
- 保留原始 `[]HdrField` 的顺序（与 C 中 `headers` 链表一致）

### 4.5 SIP 协议精确性

**挑战**：SIP 有大量边缘情况和 RFC 兼容性要求，Go 实现必须与 C 行为完全一致。

**测试策略**：
- **单元测试**：每个解析函数独立测试
- **对比测试**：用相同输入跑 C 和 Go，逐字段比较输出
- **SIPp 集成测试**：用 SIPp 生成真实流量进行端到端测试
- **模糊测试 (Fuzzing)**：`go test -fuzz` 对解析函数做模糊测试

### 4.6 性能目标

| 指标 | C 参考 | Go 目标 | 备注 |
|------|--------|---------|------|
| UDP 消息/秒 | ~200K | ~150K+ | 单物理核 |
| 注册处理/秒 | ~50K | ~40K+ | 含 HSS 交互 |
| 内存占用（空载） | ~10MB | ~30MB | Go runtime overhead |
| P99 延迟 | <5ms | <10ms | 可接受范围 |

---

## 5. 测试策略

### 5.1 测试金字塔

```
        /\             E2E 测试（SIPp）
       /  \            集成测试（多组件）
      /____\           单元测试（逐函数）
```

### 5.2 测试框架

| 层级 | 工具 | 位置 |
|------|------|------|
| 单元测试 | Go `testing` + `testify` | `*_test.go` 与源码同目录 |
| 对比测试 | Go `testing` + C 输出捕获 | `test/compare/` |
| 集成测试 | Go `testing` + mock transport | `test/integration/` |
| E2E 测试 | SIPp + shell scripts | `test/e2e/` |
| 模糊测试 | Go `testing.F` | `test/fuzz/` |

### 5.3 IMS 场景测试用例

| 场景 | 输入 | 预期输出 |
|------|------|---------|
| IMS 初始注册 | REGISTER 无 Authorization | 401 + WWW-Authenticate AKA |
| IMS 完整注册 | REGISTER 带 Authorization (AKA response) | 200 OK + Path + Service-Route |
| IMS MO 呼叫 | INVITE 从 UE 发起 | 路由到 S-CSCF → 对端 |
| IMS MT 呼叫 | INVITE 从网络侧到达 | 路由到 UE P-CSCF |
| 紧急呼叫 | INVITE Request-URI: tel:110 | 特殊路由逻辑 |
| PAI 隐私 | INVITE 带 PAI + Privacy: id | 转发时 strip PAI |

---

## 6. 配置与部署

### 6.1 配置文件

`config.yaml`:

```yaml
server:
  name: kamailio-go
  log_level: info
  workers: 8   # 对应 C 的 children_no

listeners:
  - protocol: udp
    address: "[::]:5060"
  - protocol: tcp
    address: "[::]:5060"
  - protocol: tls
    address: "[::]:5061"
    tls_cert: /etc/kamailio/tls/cert.pem
    tls_key: /etc/kamailio/tls/key.pem

ims:
  enabled: true
  home_domain: ims.mnc001.mcc460.gprs
  # HSS Diameter Cx 接口配置
  hss:
    host: hss.local
    port: 3868
    realm: ims.mnc001.mcc460.gprs

routes:
  register: ims_register
  invite: ims_route
  default: relay
```

### 6.2 容器化

```dockerfile
# Dockerfile
FROM golang:1.22-alpine AS builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o /bin/kamailio-go ./cmd/kamailio

FROM alpine:3.19
RUN apk add --no-cache ca-certificates
COPY --from=builder /bin/kamailio-go /usr/bin/kamailio-go
EXPOSE 5060/udp 5060/tcp 5061/tcp
ENTRYPOINT ["/usr/bin/kamailio-go"]
```

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| SIP 解析行为与 C 不一致 | 中 | 高 | 逐字段对比测试 + fuzzing |
| goroutine 调度导致乱序 | 低 | 高 | 使用有序 channel + 事务 ID 哈希 |
| Go GC 导致延迟抖动 | 中 | 中 | GOGC=off + 手动内存池 + 基准测试 |
| IMS 协议细节遗漏 | 中 | 高 | 对照 TS 24.229 逐条 checklist |
| TCP/TLS 性能低于 C | 中 | 中 | 使用 `bufio` + 连接池，基准测试调优 |
| 模块生态缺失 | 高 | 中 | 优先实现核心模块，提供清晰的扩展接口 |

---

## 8. 里程碑与时间线

| 里程碑 | 时间 | 交付物 |
|--------|------|--------|
| M1: 项目骨架 | W1-W2 | go.mod, Makefile, 日志, CI |
| M2: SIP 解析层 | W3-W6 | 通过所有解析测试用例 |
| M3: UDP + 基本转发 | W7-W9 | `sip:localhost:5060` 回显服务器 |
| M4: 事务管理 | W10-W12 | 支持 INVITE/BYE 事务 |
| M5: IMS 注册支持 | W13-W15 | 完整的 REGISTER + 401 + 200 OK 流程 |
| M6: IMS 会话支持 | W16-W18 | 支持 INVITE 路由、PAI/Privacy 处理 |
| M7: 生产可用 | W19-W24 | 性能达标、压力测试通过 |

**总计：~24 周（6 个月）到生产可用状态**

---

## 9. 目录结构建议

```
kamailio-go/
├── cmd/
│   └── kamailio/          # 主程序入口
│       └── main.go
├── internal/
│   ├── core/
│   │   ├── str/           # 字符串类型
│   │   ├── parser/        # SIP 解析器
│   │   │   ├── parse_uri.go
│   │   │   ├── parse_via.go
│   │   │   ├── parse_contact.go
│   │   │   ├── parse_from.go
│   │   │   ├── parse_to.go
│   │   │   ├── parse_ppi_pai.go
│   │   │   └── digest/    # Digest/AKA 鉴权
│   │   ├── msg.go         # sip_msg_t 对应结构
│   │   ├── msg_parser.go  # 消息整体解析
│   │   ├── msg_translator.go  # 消息重写
│   │   ├── transport/     # UDP/TCP/TLS
│   │   │   ├── udp.go
│   │   │   ├── tcp.go
│   │   │   ├── tls.go
│   │   │   └── socket_info.go
│   │   ├── forward.go     # stateless 转发
│   │   ├── hash/          # 哈希函数
│   │   ├── route/         # 路由引擎
│   │   ├── config/        # 配置管理
│   │   ├── timer/         # 定时器
│   │   ├── dns/           # DNS 解析
│   │   └── log/           # 日志
│   ├── ims/
│   │   ├── pcscf/         # P-CSCF 逻辑
│   │   ├── icscf/         # I-CSCF 逻辑
│   │   ├── scscf/         # S-CSCF 逻辑
│   │   ├── auth/          # AKA/AKAv2 鉴权
│   │   ├── hss/           # HSS Diameter Cx 接口 (client)
│   │   └── headers.go     # IMS 特定头字段处理
│   └── modules/
│       └── tm/            # Transaction Management (事务管理)
├── pkg/
│   └── sip/               # 对外公开的 SIP 工具
├── test/
│   ├── data/              # SIP 消息样本
│   ├── integration/       # 集成测试
│   ├── e2e/               # SIPp E2E 测试
│   └── compare/           # C vs Go 对比测试
├── config/
│   └── config.example.yaml
├── docs/
│   ├── PORTING.md         # 本文件（移植指南）
│   └── IMS.md             # IMS 协议说明
├── go.mod
├── go.sum
├── Makefile
└── README.md
```

---

## 10. IMS 特定迁移细节清单

### 10.1 P-CSCF 功能映射

| 功能 | C 实现 | Go 实现位置 |
|------|--------|------------|
| UE 接入点监听 | `udp_server.c` + `tcp_main.c` | `transport/` |
| P-Visited-Network-ID 添加 | `modules/pua/pua.c`（参考）| `ims/pcscf/pvid.go` |
| P-Access-Network-Info 透传/验证 | 头字段解析 + 路由脚本 | `ims/pcscf/pani.go` |
| 拓扑隐藏（Topology Hiding） | 配置/脚本层面 | `ims/pcscf/thig.go` |
| QoS 资源授权（Rx 接口） | 外部 PCRF 交互 | `ims/pcscf/rx.go` |
| SIP over TLS/DTLS | `core/tls/` | `transport/tls.go` |

### 10.2 S-CSCF 功能映射

| 功能 | C 实现 | Go 实现位置 |
|------|--------|------------|
| REGISTER 处理 & 绑定 | 脚本级 + registrar 模块 | `ims/scscf/register.go` |
| S-CSCF 分配 (via I-CSCF) | - | `ims/scscf/assign.go` |
| INVITE 路由 | 脚本级 + tm | `ims/scscf/session.go` |
| 业务触发 (iFC) | 需开发 | `ims/scscf/ifc.go` |
| 与 AS 交互（3rd party SCSCF AS） | - | `ims/scscf/as.go` |

### 10.3 I-CSCF 功能映射

| 功能 | C 实现 | Go 实现位置 |
|------|--------|------------|
| 入口/出口点 | `forward.c` | `ims/icscf/` |
| HSS 查询 (LIR) | Diameter Cx | `ims/icscf/lir.go` |
| S-CSCF 选择 (LIA) | Diameter Cx | `ims/icscf/scscf_select.go` |

---

## 11. 编译与运行

```bash
# 开发构建
make dev      # go build -o bin/kamailio ./cmd/kamailio
make test     # go test ./...
make bench    # go test -bench=. ./...
make fuzz     # go test -fuzz=FuzzParseURI ./internal/core/parser

# 运行
./bin/kamailio -config config/config.yaml
./bin/kamailio -pcscf        # 启动 P-CSCF 角色
./bin/kamailio -scscf        # 启动 S-CSCF 角色
```

---

## 12. 下一步行动项

- [ ] **决策点 1**: 路由脚本系统选择（Go DSL vs YAML vs Lua）
- [ ] **决策点 2**: HSS Diameter Cx 接口实现方案（从零 vs 复用现有库）
- [ ] **决策点 3**: 是否保留与原 C Kamailio 的配置兼容性
- [ ] **启动 M1**: 初始化项目骨架，建立 CI/CD
- [ ] **招聘/培训**: 组建 Go 开发团队 + 3GPP IMS 协议培训
