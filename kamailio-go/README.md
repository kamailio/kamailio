# Kamailio-Go

Open source SIP server implementation in Go, targeting 3GPP IMS (VoLTE/VoNR) deployments.

## Project Status

**Phase: Planning / M1 - Project Skeleton**

See [docs/MIGRATION_PLAN.md](docs/MIGRATION_PLAN.md) for full migration plan.

## Quick Start

```bash
make dev     # build
make test    # run tests
make bench   # benchmarks
```

## Architecture

- `cmd/kamailio` - Main entry point
- `internal/core/` - Core SIP server functionality
  - `parser/` - SIP message parsing
  - `transport/` - UDP/TCP/TLS transport
  - `str/` - String utilities
  - `hash/` - Hash functions
- `internal/ims/` - IMS (P-CSCF/I-CSCF/S-CSCF) functionality
- `internal/modules/tm/` - Transaction Management

## Building

```bash
go build -o bin/kamailio ./cmd/kamailio
```

## License

GPL-2.0 (matching Kamailio original project)
