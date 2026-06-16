all: build

# Go parameters
GOCMD=go
GOBUILD=$(GOCMD) build
GOTEST=$(GOCMD) test
GOMOD=$(GOCMD) mod
GOFMT=$(GOCMD) fmt
GOVET=$(GOCMD) vet

# Binary output
BINARY_NAME=kamailio
BINARY_DIR=bin
BINARY_PATH=$(BINARY_DIR)/$(BINARY_NAME)

# Build flags
LDFLAGS=-ldflags "-s -w"

# Directories
CMD_DIR=cmd/kamailio
PKG_DIR=internal

.PHONY: all build dev test bench clean fmt vet tidy

# Default target
all: build

# Development build (with debug symbols)
dev:
	@mkdir -p $(BINARY_DIR)
	$(GOBUILD) -o $(BINARY_PATH) $(CMD_DIR)

# Production build
build:
	@mkdir -p $(BINARY_DIR)
	CGO_ENABLED=0 $(GOBUILD) $(LDFLAGS) -o $(BINARY_PATH) $(CMD_DIR)

# Run tests
test:
	$(GOTEST) -v -race $(PKG_DIR)/...

# Run benchmarks
bench:
	$(GOTEST) -bench=. -benchmem $(PKG_DIR)/...

# Format code
fmt:
	$(GOFMT) ./...

# Vet code
vet:
	$(GOVET) ./...

# Tidy dependencies
tidy:
	$(GOMOD) tidy

# Clean build artifacts
clean:
	rm -rf $(BINARY_DIR)

# Run a single package tests
test-one:
	$(GOTEST) -v -race $(PKG_DIR)/$(PKG)

# Build for multiple platforms
build-all:
	GOOS=linux GOARCH=amd64 $(GOBUILD) -o $(BINARY_PATH)-linux-amd64 $(CMD_DIR)
	GOOS=linux GOARCH=arm64 $(GOBUILD) -o $(BINARY_PATH)-linux-arm64 $(CMD_DIR)
