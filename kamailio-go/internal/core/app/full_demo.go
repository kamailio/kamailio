// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Full-stack Kamailio-Go launcher.
 *
 * Wires ProxyCore, Pike, htable.Manager, Msilo, an optional
 * RTPEngine-backed media pipeline, an optional DB auth store and an
 * optional routing script behind a single FullStack struct so the
 * JSON-RPC HTTP endpoint can report aggregate state.
 */

package app

import (
	"fmt"
	"time"

	"github.com/kamailio/kamailio-go/internal/core/acc"
	"github.com/kamailio/kamailio-go/internal/core/auth"
	"github.com/kamailio/kamailio-go/internal/core/db"
	"github.com/kamailio/kamailio-go/internal/core/dialog"
	"github.com/kamailio/kamailio-go/internal/core/htable"
	"github.com/kamailio/kamailio-go/internal/core/msilo"
	"github.com/kamailio/kamailio-go/internal/core/nat"
	"github.com/kamailio/kamailio-go/internal/core/pike"
	"github.com/kamailio/kamailio-go/internal/core/proxy"
	"github.com/kamailio/kamailio-go/internal/core/registrar"
	"github.com/kamailio/kamailio-go/internal/core/rtpengine"
	"github.com/kamailio/kamailio-go/internal/core/rpc"
	"github.com/kamailio/kamailio-go/internal/core/script"
	"github.com/kamailio/kamailio-go/internal/modules/tm"
)

// FullStack represents a fully-wired Kamailio-Go server plus a runtime
// JSON-RPC HTTP endpoint. Subsystems that are not explicitly configured
// remain nil; the RPC endpoint returns graceful errors for them instead
// of crashing.
type FullStack struct {
	Proxy   *proxy.ProxyCore
	Dialogs *dialog.Manager
	Acc     *acc.AccountingService
	Pike    *pike.Pike
	HTables *htable.Manager
	Msilo   *msilo.Msilo
	RPC     *rpc.Server
	Auth    *auth.DBAuthStore

	rpcAddr string
	started bool
}

// FullStackConfig controls how the FullStack is built. Every field is
// optional — sensible defaults are applied when fields are zero.
type FullStackConfig struct {
	// Realm is used as the SIP realm for authentication challenges.
	// Defaults to "kamailio-go.local".
	Realm string

	// RPCEndpoint is the host:port to bind the JSON-RPC HTTP server
	// on. Empty disables the JSON-RPC endpoint.
	RPCEndpoint string

	// PikeLimit is the number of requests per source IP permitted in
	// a PikeWindow. Zero falls back to 20.
	PikeLimit int

	// PikeWindow is the sliding window for Pike rate-limiting. Zero
	// falls back to 5s.
	PikeWindow time.Duration

	// RTPEngineURL is the optional HTTP URL of an RTPEngine control
	// interface. When set the proxy will forward INVITE/BYE
	// requests' SDP to the RTPEngine.
	RTPEngineURL string

	// PublicMediaIP is the public address advertised for RTP. Used
	// only when RTPEngineURL is set.
	PublicMediaIP string

	// AuthDB is the optional DBConn used for credential lookups.
	AuthDB db.DBConn

	// AuthTable / AuthUserCol / AuthPassCol / AuthHA1Col /
	// AuthRealmCol are column name overrides. Empty strings fall
	// back to the Kamailio defaults.
	AuthTable    string
	AuthUserCol  string
	AuthPassCol  string
	AuthHA1Col   string
	AuthRealmCol string

	// Script, if non-nil, is installed so requests are dispatched
	// through it before the default pipeline.
	Script *script.Script
}

// NewFullStack builds a fully-wired server and returns it.
func NewFullStack(cfg FullStackConfig) (*FullStack, error) {
	if cfg.Realm == "" {
		cfg.Realm = "kamailio-go.local"
	}
	fs := &FullStack{Proxy: proxy.NewProxyCore(&proxy.ProxyConfig{Realm: cfg.Realm})}

	if cfg.PikeLimit <= 0 {
		cfg.PikeLimit = 20
	}
	if cfg.PikeWindow <= 0 {
		cfg.PikeWindow = 5 * time.Second
	}
	fs.Pike = pike.New(cfg.PikeLimit, cfg.PikeWindow)
	fs.Proxy.SetPike(fs.Pike)

	fs.HTables = htable.NewManager()
	fs.Proxy.SetHTables(fs.HTables)

	fs.Msilo = msilo.New(nil, "msilo")
	fs.Proxy.SetMsilo(fs.Msilo)

	fs.Dialogs = dialog.NewManager()
	fs.Proxy.SetDialogs(fs.Dialogs)

	fs.Acc = acc.NewAccountingService()
	fs.Proxy.SetAccounting(fs.Acc)

	// Phase 44: wire registrar and transaction manager into ProxyCore.
	tmMgr := tm.NewManager(1024)
	fs.Proxy.SetTM(tmMgr)
	reg := registrar.New(&registrar.Config{
		Realm:          cfg.Realm,
		DefaultExpires: 3600 * time.Second,
		MaxExpires:     86400 * time.Second,
		MinExpires:     60 * time.Second,
	})
	fs.Proxy.SetRegistrar(reg)

	if cfg.RTPEngineURL != "" {
		rtp := rtpengine.NewRTPEngineClient(rtpengine.RTPEngineConfig{ControlURL: cfg.RTPEngineURL})
		pl := nat.NewPipeline(rtp, cfg.PublicMediaIP)
		fs.Proxy.SetMediaPipeline(pl)
	}

	if cfg.AuthDB != nil {
		fs.Auth = auth.NewDBAuthStore(
			cfg.AuthDB,
			cfg.AuthTable,
			cfg.AuthUserCol,
			cfg.AuthPassCol,
			cfg.AuthHA1Col,
			cfg.AuthRealmCol,
		)
		fs.Proxy.SetAuthStore(fs.Auth)
	}

	if cfg.Script != nil {
		fs.Proxy.SetScript(cfg.Script)
	}

	if cfg.RPCEndpoint != "" {
		fs.RPC = rpc.NewExtended(rpc.ServerConfig{
			Core:    fs.Proxy,
			Dialogs: fs.Dialogs,
			Acc:     fs.Acc,
			Pike:    fs.Pike,
			HTables: fs.HTables,
			Msilo:   fs.Msilo,
		})
		addr := cfg.RPCEndpoint
		go func() {
			_ = fs.RPC.ListenAndServe(addr)
		}()
		fs.rpcAddr = addr
	}
	fs.started = true
	return fs, nil
}

// Close stops every subsystem. Safe to call on a nil FullStack.
func (fs *FullStack) Close() error {
	if fs == nil {
		return nil
	}
	if fs.RPC != nil {
		if err := fs.RPC.Shutdown(); err != nil {
			return fmt.Errorf("rpc shutdown: %w", err)
		}
	}
	if fs.Pike != nil {
		fs.Pike.Close()
	}
	fs.started = false
	return nil
}

// RPCAddr returns the JSON-RPC endpoint host:port (empty if the
// endpoint was disabled).
func (fs *FullStack) RPCAddr() string { return fs.rpcAddr }
