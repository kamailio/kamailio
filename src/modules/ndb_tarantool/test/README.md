# ndb_tarantool Module Tests

This directory contains the in-tree regression and functional test suite for the `ndb_tarantool` module, adhering to Kamailio's module testing specification (`test/README.md`).

## Prerequisites

1. Running **Tarantool 3.x** instance with user `rtpe_user` and password `rtpe_secret_password` (or custom credentials matching `test.cfg`):
   ```bash
   # Quick start with Docker:
   docker run -d --name tnt_test -p 3301:3301 tarantool/tarantool:3
   ```
2. Python 3 (standard library only, for sending local SIP datagrams).

## Running Tests

### 1. Syntax Check Only
Validates the configuration syntax without executing:
```bash
make test
```

### 2. Full Test Execution
Runs syntax validation, all 19 native CFG & advanced tests, and all 4 KEMI Lua routing tests:
```bash
make all
# or directly:
./run-tests.sh
```

## What Is Verified (23 Tests Total)

### Native CFG & Advanced Regression Suite (19 Tests)
1. **Simple Expression Evaluation:** Basic arithmetic and scalar return (`[42]`).
2. **String Returns:** String return values from Lua expressions.
3. **Server Version Detection:** Querying `box.info.version` dynamically.
4. **Stored Procedure Calls:** Executing stored procedures (`ttl_get_stats`).
5. **Parameter Passing:** JSON array parameter encoding into MsgPack tuples (`[10, 20]` -> `30`).
6. **Session Upsert:** Real-time VoIP session state upsert (`rtpe_call_upsert`).
7. **Session Query:** Fetching active session records by Call-ID (`rtpe_call_get`).
8. **Named Cluster Calls:** Multi-server targeting with `tarantool_call_srv("cluster1", ...)`.
9. **Error Handling:** Graceful rejection of non-existent procedures (returns false / -1).
10. **Multi-byte UTF-8:** Cyrillic and unicode preservation through IProto and Kamailio PVs.
11. **Escaping:** Quotes, backslashes, and special character sanitization.
12. **Strict RFC-8259 JSON:** Outputting valid maps, arrays, booleans (`true`/`false`), and `null`.
13. **Named Cluster Eval:** Multi-server evaluation with `tarantool_eval_srv("cluster1", ...)`.
14. **Forced Timeout & Recovery:** Command timeout (`cmd_timeout=400ms`) and instant auto-recovery.
15. **Bad Authentication Rejection:** Rejection on invalid credentials (`bad_auth` returns -1).
16. **Big Payload Stress:** 64 KB payload received and verified.
17. **Destination Storage:** Direct assignment of query results into `$avp(...)`.
18. **Atomic VoIP Billing:** Real telecom call authorization (`billing_authorize_call`).
19. **Pipeline Integration:** Parsing Tarantool JSON output directly with Kamailio `jansson` module.

### KEMI Framework Suite (4 Tests)
20. `KSR.tarantool.eval` — Evaluating expressions from Lua.
21. `KSR.tarantool.call` — Executing procedures from Lua.
22. `KSR.tarantool.eval_srv` — Evaluating on named servers from Lua.
23. `KSR.tarantool.call_srv` — Executing procedures on named servers from Lua.
