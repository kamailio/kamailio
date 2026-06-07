# TOPOS REGISTER/PUBLISH Topology Hiding — Design Spec

**Date:** 2026-06-07  
**Status:** Approved  
**Scope:** Port REGISTER/PUBLISH topology hiding from `src/modules/topos_mod` into `src/modules/topos`

## Summary

Port the REGISTER/PUBLISH topology hiding feature from the older `topos_mod` module into the current `topos` module. When enabled, REGISTER and PUBLISH follow the same TOPOS encoding, storage, and Call-ID masking rules as INVITE/SUBSCRIBE. Parameters are renamed for upstream integration.

## Decisions

| Decision | Choice |
|----------|--------|
| Enable switch | `enable_reg_pub` (renamed from `enable_register_publish`) |
| Multi-Contact policy | `reg_pub_multi_contact` (renamed from `register_multi_contact`) |
| Deployment targets | IMS P-CSCF + generic SIP proxy |
| Verification | Automated SIPp unit tests + manual IMS checklist |
| Test storage backends | SQLite (primary) + HTable (smoke test) |
| Port strategy | Surgical cherry-pick from `topos_mod` into current `topos` |

## Parameters

### `enable_reg_pub` (int, default 0)

When set to 0 (default), REGISTER and PUBLISH are not processed by TOPOS (legacy behavior). When set to 1, REGISTER and PUBLISH use the same topology stripping, storage updates, and Call-ID masking rules as INVITE (when `mask_callid` is enabled and `topoh` is loaded).

The `methods_noinitial` check runs afterward in `tps_skip_msg()`: if REGISTER or PUBLISH matches that bitmask on an initial request, the message can still be skipped even when this parameter is 1.

Optionally add REGISTER or PUBLISH to `methods_update_time` so in-dialog requests refresh `rectime` in `topos_d`, as for SUBSCRIBE.

### `reg_pub_multi_contact` (int, default 0)

Evaluated only when `enable_reg_pub=1` and TOPOS parses multiple Contact entries for REGISTER or PUBLISH.

- **0 (default):** Use the first Contact URI for TOPOS storage; emit `LM_NOTICE` naming registrar/usrloc for full bindings.
- **1:** Reject linking for multi-Contact REGISTER/PUBLISH (`LM_ERR`); strict policy when multi-Contact must not pass TOPOS storage.

Validation in `mod_init()`: value must be 0 or 1.

### Interaction with existing parameters

- `mask_callid` — applies when `topoh` is loaded (same as INVITE)
- `methods_noinitial` — evaluated after enable check; can skip even when `enable_reg_pub=1`
- `methods_update_time` — optionally include REGISTER/PUBLISH for rectime refresh (documented, not hardcoded)

Default behavior with `enable_reg_pub=0` is unchanged — fully backward compatible.

## Architecture

When `enable_reg_pub=1`, REGISTER and PUBLISH follow the TOPOS pipeline:

```
UA ──REGISTER/PUBLISH──► Kamailio (topos + topoh)
                              │
                    tps_skip_msg() → process (not skip)
                              │
                    tps_pack_message() → mask Call-ID (via topoh)
                              │
                    tps_storage_record() → topos_t / topos_d
                              │
                    Forward stripped message upstream
                              │
                    On reply: restore headers, unmask Call-ID
                              │
                    On Expires=0 or >299 reply → end dialog storage
```

Storage backends (`topos_redis`, `topos_htable`) require no method-specific changes — they use the shared `tps_storage.c` API.

## Code Changes

Surgical port from `topos_mod`. Do **not** replace contact-building refactor in current `topos` (`tps_contact_host_len`, buffer sizing, Refer-To handling).

### `topos/topos_mod.c`

- Add globals: `_tps_enable_reg_pub`, `_tps_reg_pub_multi_contact`
- Export modparams: `enable_reg_pub`, `reg_pub_multi_contact`
- Add `mod_init()` validation for `reg_pub_multi_contact` (0 or 1)

### `topos/tps_storage.h`

- Add `unsigned char expires_valid` to `tps_data_t` — distinguishes "no Expires header" from "Expires: 0" for deregister/de-publish cleanup

### `topos/tps_msg.c`

| Function | Change |
|----------|--------|
| `tps_skip_msg()` | Skip REGISTER/PUBLISH only when `_tps_enable_reg_pub == 0` |
| `tps_pack_message()` | Parse Expires for REGISTER/PUBLISH/SUBSCRIBE; set `expires_valid` |
| `tps_reply_received()` | On >299 replies, end dialog for REGISTER/PUBLISH (extend bitmask) |

### `topos/tps_storage.c`

| Function / area | Change |
|-----------------|--------|
| `tps_storage_link_msg()` | Set `td->s_method_id` early; treat REGISTER/PUBLISH as contact-required methods |
| Multi-Contact handling | REGISTER/PUBLISH: first URI + NOTICE (mode 0) or reject (mode 1) |
| Expires parsing | REGISTER/PUBLISH/SUBSCRIBE: set `expires` + `expires_valid` |
| `tps_storage_load_branch()` | Dialog method lookup: add `"REGISTER"` / `"PUBLISH"` strings |
| `tps_storage_update_dialog()` | Include REGISTER/PUBLISH in dialog update path |
| `tps_storage_update_branch()` | Include REGISTER/PUBLISH in branch update path |
| `tps_storage_end_dialog()` | End on Expires=0 for REGISTER/PUBLISH when `expires_valid==1` |

### Documentation

- `topos/doc/topos_admin.xml` — new parameter sections (adapted from `topos_mod`, renamed)
- `topos/README` — update matching sections

### Out of scope

- Contact URI building refactor in current `topos`
- Refer-To unmasking, event routes, `methods_update_time` defaults
- Backward-compat aliases for old param names (`enable_register_publish`, `register_multi_contact`)
- Changes to `topos_redis` or `topos_htable` modules

## Data Flow

### Initial REGISTER/PUBLISH (downstream → upstream)

1. SIP message enters Kamailio (msg-outgoing / msg-sending event)
2. `tps_skip_msg()` — skip only if `enable_reg_pub=0` or `methods_noinitial` match
3. `tps_request_sent()` path
4. `tps_pack_message()` — extract headers; parse Expires
5. `topoh` masks Call-ID (if `mask_callid=1`)
6. `tps_storage_record()` — fill contact, link msg, insert branch + dialog
7. Strip RR/Contact/Via; reinsert masked Via; forward upstream

### In-dialog refresh (upstream → downstream)

1. `tps_request_received()` with dialog != 0
2. `tps_unmask_callid()` — restore original Call-ID
3. `tps_storage_load_dialog()` — lookup by masked call-id + tags
4. `tps_dlg_detect_direction()` — match from-tag
5. Restore Route, Contact, Via; rewrite R-URI
6. `tps_storage_update_branch()` / `tps_storage_update_dialog()`

### Deregister / de-publish (Expires: 0)

1. Same in-dialog path as refresh
2. `tps_storage_end_dialog()` when `expires_valid==1` AND `expires==0`
3. Remove topos_t / topos_d rows

### Failed reply (>299)

1. `tps_reply_received()` on 4xx/5xx/6xx
2. `tps_storage_end_dialog()` for REGISTER/PUBLISH

### Call-ID masking round-trip

- Outbound: UA Call-ID → topoh masks → stored in `topos_d.a_callid`
- Inbound: masked Call-ID → `tps_unmask_callid()` → restored for UA

## Error Handling & Edge Cases

| Scenario | Behavior |
|----------|----------|
| `enable_reg_pub=0` | REGISTER/PUBLISH skipped (legacy) |
| Missing CSeq | Warn and skip in `tps_skip_msg()` |
| Missing Contact on initial REGISTER/PUBLISH | Error in `tps_storage_link_msg()` |
| Missing Contact on 1xx/4xx+ reply | Allowed |
| Multi-Contact, `reg_pub_multi_contact=0` | First URI stored; LM_NOTICE |
| Multi-Contact, `reg_pub_multi_contact=1` | Link fails; LM_ERR; no TOPOS row |
| Multi-Contact on INVITE/SUBSCRIBE | Unchanged — rejected |
| No Expires header | `expires_valid=0`; no cleanup |
| Expires: 0 with valid header | End dialog storage |
| `methods_noinitial` includes REGISTER | Skipped even with `enable_reg_pub=1` |
| Failed reply >299 | Dialog storage ended |

**IMS note:** TOPOS stores topology for one Contact URI. Full multi-binding state remains in registrar/usrloc (or ims_registrar_pcscf/ims_usrloc_pcscf).

## Testing & Verification

### Automated: `test/unit/topos_reg_pub.sh` (SQLite, primary)

Two-hop proxy: SIPp UAC → Kamailio (topos + topoh) → SIPp UAS

| Scenario | Assert |
|----------|--------|
| REGISTER, `enable_reg_pub=0` | Call-ID unchanged upstream |
| REGISTER, `enable_reg_pub=1`, `mask_callid=1` | Masked Call-ID upstream; re-REGISTER unmasks |
| REGISTER Expires=0 | Dialog row removed |
| REGISTER 403 reply | Dialog storage cleaned |
| Multi-Contact, `reg_pub_multi_contact=0` | 200 OK; first Contact used |
| Multi-Contact, `reg_pub_multi_contact=1` | TOPOS linking fails |
| Initial PUBLISH | Call-ID masked; 200 OK |
| Re-PUBLISH in-dialog | Call-ID unmasked |
| PUBLISH Expires=0 | Dialog storage ended |

Storage: `db_sqlite` + `topos_t` / `topos_d` tables.

### Automated: `test/unit/topos_reg_pub_htable.sh` (smoke)

Same REGISTER happy-path with `storage=htable` and `topos_htable` module.

### Manual IMS checklist

1. P-CSCF REGISTER with multiple Contact bindings — NOTICE logged; registrar holds all bindings
2. `methods_update_time` includes REGISTER — rectime refreshed on re-REGISTER
3. PUBLISH through presence server — Event/ETag preserved after TOPOS round-trip
4. `methods_noinitial=REGISTER` — skip confirmed with `enable_reg_pub=1`

### Build verification

- Compile `topos` module without warnings
- Run: `make -C test/unit run UNIT=topos_reg_pub.sh`

Test dependencies: SIPp, `db_sqlite`, `topos_htable` (skip gracefully if missing via `check_sipp()` pattern).
