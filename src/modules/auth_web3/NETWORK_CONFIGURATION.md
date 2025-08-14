# Multi-Network ENS Configuration Guide

## Overview

The Web3 Authentication now supports **dual-network authentication**, allowing ENS contracts and Oasis authentication contracts to operate on different blockchain networks.

## Configuration Parameters

### Main Network (Oasis)
- `authentication_rpc_url`: RPC endpoint for Oasis authentication contract
- `authentication_contract_address`: Oasis authentication contract address

### ENS Network (Ethereum/Sepolia)
- `ens_rpc_url`: **NEW** - RPC endpoint for ENS queries (optional)
- `ens_registry_address`: ENS Registry contract address
- `ens_name_wrapper_address`: ENS Name Wrapper contract address

## Network Flow

### Scenario 1: Same Network (Fallback Mode)
```
ens_rpc_url = NULL (not configured)
```
- ✅ ENS queries → `authentication_rpc_url`
- ✅ Oasis queries → `authentication_rpc_url`
- Use case: Both contracts on same network

### Scenario 2: Different Networks (Multi-Network Mode)
```
authentication_rpc_url = "https://testnet.sapphire.oasis.dev"
ens_rpc_url = "https://eth.drpc.org"
```
- ✅ ENS queries → `ens_rpc_url` (Mainnet)
- ✅ Oasis queries → `authentication_rpc_url` (Oasis Sapphire)
- Use case: ENS on Ethereum, Oasis on Oasis network

## Common Network Configurations

### Production Setup
```bash
# Oasis Mainnet + Ethereum Mainnet
authentication_rpc_url = "https://sapphire.oasis.io"
ens_rpc_url = "https://eth.drpc.org"
ens_registry_address = "0x00000000000C2E074eC69A0dFb2997BA6C7d2e1e"
ens_name_wrapper_address = "0xD4416b13d2b3a9aBae7AcD5D6C2BbDBE25686401"
```

### Testing Setup
```bash
# Oasis Testnet + Ethereum Sepolia
authentication_rpc_url = "https://testnet.sapphire.oasis.dev"
ens_rpc_url = "https://ethereum-sepolia-rpc.publicnode.com"
ens_registry_address = "0x00000000000C2E074eC69A0dFb2997BA6C7d2e1e"
ens_name_wrapper_address = "0x0635513f179D50A207757E05759CbD106d7dFcE8"
```

### Development Setup
```bash
# Both on same testnet
authentication_rpc_url = "https://ethereum-sepolia-rpc.publicnode.com"
# ens_rpc_url not set - will use authentication_rpc_url
```

## Kamailio Configuration Example

```kamailio
loadmodule "auth_web3.so"

# Oasis authentication network
modparam("auth_web3", "authentication_rpc_url", "https://testnet.sapphire.oasis.dev")
modparam("auth_web3", "authentication_contract_address", "0xYourOasisContract")

# ENS network (Sepolia testnet)
modparam("auth_web3", "ens_rpc_url", "https://ethereum-sepolia-rpc.publicnode.com")
modparam("auth_web3", "ens_registry_address", "0x00000000000C2E074eC69A0dFb2997BA6C7d2e1e")
modparam("auth_web3", "ens_name_wrapper_address", "0x0635513f179D50A207757E05759CbD106d7dFcE8")

modparam("auth_web3", "contract_debug_mode", 1)
```

## Testing Your Configuration

Use the provided test program to verify network configuration:

```bash
# Compile test program
make -f Makefile.test

# Test with your configuration
./test_ens_validation jonathan 123123 jonathan123.eth

# Check debug output for network usage:
# "ENS call using RPC: https://ethereum-sepolia-rpc.publicnode.com"
# "Oasis call using main RPC: https://testnet.sapphire.oasis.dev"
```

## Benefits

1. **Network Separation**: Keep ENS queries on Ethereum while using Oasis for authentication
2. **Cost Optimization**: Use cheaper testnets for ENS during development
3. **Performance**: Separate network load between ENS and authentication calls
4. **Flexibility**: Easy migration between networks
5. **Backward Compatibility**: Existing single-network setups continue to work
6. **No API Keys Required**: PublicNode provides free, reliable RPC access

## Troubleshooting

### Common Issues

1. **Network Connectivity**: Ensure PublicNode RPC is accessible from your server
2. **Network Mismatch**: Verify contract addresses match the configured network
3. **Fallback Behavior**: If `ens_rpc_url` is empty, ENS uses `authentication_rpc_url`
4. **Rate Limiting**: PublicNode has fair usage limits for free tier

### Debug Information

Enable debug mode to see which RPC is used for each call:
```
modparam("auth_web3", "contract_debug_mode", 1)
```

Look for log messages:
- `ENS call using RPC: [url] (ENS-specific: yes/no)`
- `Oasis call using main RPC: [url]`

### Why PublicNode?

- ✅ **Free**: No API keys required
- ✅ **Fast**: Low latency, high availability
- ✅ **Privacy-focused**: No tracking or data collection
- ✅ **Reliable**: Professional infrastructure
- ✅ **Multiple networks**: Supports many blockchain networks