# TOPOS REGISTER/PUBLISH Topology Hiding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port REGISTER/PUBLISH topology hiding from `topos_mod` into `topos` with renamed modparams `enable_reg_pub` and `reg_pub_multi_contact`, plus automated SIPp tests and an IMS manual checklist.

**Architecture:** Surgical cherry-pick of REGISTER/PUBLISH logic from `src/modules/topos_mod/` into `src/modules/topos/`, preserving the current contact-building refactor. Gated by `enable_reg_pub`; multi-Contact policy via `reg_pub_multi_contact`. Storage backends unchanged (shared `tps_storage.c` API).

**Tech Stack:** C (Kamailio module), SQLite/HTable storage, SIPp unit tests, DocBook XML docs.

**Spec:** [`docs/superpowers/specs/2026-06-07-topos-reg-pub-design.md`](../specs/2026-06-07-topos-reg-pub-design.md)

**Reference implementation:** `src/modules/topos_mod/` (local, untracked — use for diff only, do not commit)

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/modules/topos/tps_storage.h` | Modify | Add `expires_valid` to `tps_data_t` |
| `src/modules/topos/topos_mod.c` | Modify | New modparams + validation |
| `src/modules/topos/tps_msg.c` | Modify | Skip gate, Expires parse, failed-reply cleanup |
| `src/modules/topos/tps_storage.c` | Modify | link_msg, dialog lifecycle for REGISTER/PUBLISH |
| `src/modules/topos/doc/topos_admin.xml` | Modify | Document new parameters |
| `src/modules/topos/README` | Modify | Regenerate from docbook |
| `test/unit/topos_reg_pub.cfg` | Create | Kamailio proxy config (SQLite + topos) |
| `test/unit/topos_reg_pub_uas.xml` | Create | SIPp UAS: log Call-ID, reply 200 |
| `test/unit/topos_reg_pub_uac.xml` | Create | SIPp UAC: REGISTER + re-REGISTER + Expires=0 |
| `test/unit/topos_reg_pub_publish_uac.xml` | Create | SIPp UAC: PUBLISH + de-publish |
| `test/unit/topos_reg_pub.sh` | Create | Primary automated test (SQLite) |
| `test/unit/topos_reg_pub_htable.cfg` | Create | HTable storage variant |
| `test/unit/topos_reg_pub_htable.sh` | Create | HTable smoke test |
| `docs/superpowers/specs/2026-06-07-topos-reg-pub-ims-checklist.md` | Create | Manual IMS verification steps |

---

### Task 1: Automated test scaffolding (expect FAIL before implementation)

**Files:**
- Create: `test/unit/topos_reg_pub.cfg`
- Create: `test/unit/topos_reg_pub_uas.xml`
- Create: `test/unit/topos_reg_pub_uac.xml`
- Create: `test/unit/topos_reg_pub_publish_uac.xml`
- Create: `test/unit/topos_reg_pub.sh`

- [ ] **Step 1: Create Kamailio proxy config**

Create `test/unit/topos_reg_pub.cfg`:

```
#!define UAS_PORT 5070
#!define DB_FILE "/tmp/topos_reg_pub_test.db"

debug=2
memdbg=5
memlog=5
log_stderror=yes
fork=no
children=1

listen=udp:127.0.0.1:5060

loadmodule "tm/tm.so"
loadmodule "sl/sl.so"
loadmodule "rr/rr.so"
loadmodule "pv/pv.so"
loadmodule "xlog/xlog.so"
loadmodule "db_sqlite/db_sqlite.so"
loadmodule "topoh/topoh.so"
loadmodule "topos/topos.so"

modparam("topos", "db_url", "sqlite:///" + DB_FILE)
modparam("topos", "mask_callid", 1)
modparam("topos", "enable_reg_pub", 1)
modparam("topos", "reg_pub_multi_contact", 0)
modparam("topos", "version_table", 0)

request_route {
	if (is_method("REGISTER|PUBLISH|INVITE|SUBSCRIBE")) {
		record_route();
	}
	$du = "sip:127.0.0.1:" + UAS_PORT;
	if (!t_relay()) {
		sl_reply_error();
	}
}
```

- [ ] **Step 2: Create SIPp UAS scenario**

Create `test/unit/topos_reg_pub_uas.xml`:

```xml
<?xml version="1.0" encoding="ISO-8859-1" ?>
<!DOCTYPE scenario SYSTEM "sipp.dtd">
<scenario name="topos_reg_pub_uas">
  <recv request="REGISTER" crlf="true">
    <action>
      <ereg regexp="Call-ID:[ ]*([^\r\n]+)" search_in="msg" check_it="true" assign_to="_,uas_callid"/>
      <log message="UAS_CALLID=[$uas_callid]"/>
    </action>
  </recv>
  <send>
    <![CDATA[
SIP/2.0 200 OK
[last_Via:]
[last_From:]
[last_To:];tag=[pid]SIPpTag01[call_number]
[last_Call-ID:]
[last_CSeq:]
Contact: <sip:[field0]@[local_ip]:[local_port]>;expires=3600
Content-Length: 0

]]>
  </send>
  <recv request="REGISTER" crlf="true"/>
  <send>
    <![CDATA[
SIP/2.0 200 OK
[last_Via:]
[last_From:]
[last_To:];tag=[pid]SIPpTag01[call_number]
[last_Call-ID:]
[last_CSeq:]
Contact: <sip:[field0]@[local_ip]:[local_port]>;expires=0
Content-Length: 0

]]>
  </send>
</scenario>
```

- [ ] **Step 3: Create SIPp UAC REGISTER scenario**

Create `test/unit/topos_reg_pub_uac.xml`:

```xml
<?xml version="1.0" encoding="ISO-8859-1" ?>
<!DOCTYPE scenario SYSTEM "sipp.dtd">
<scenario name="topos_reg_pub_uac">
  <send retrans="500">
    <![CDATA[
REGISTER sip:127.0.0.1 SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
Max-Forwards: 70
To: <sip:[field0]@127.0.0.1>
From: <sip:[field0]@127.0.0.1>;tag=[call_number]
Call-ID: [call_id]
CSeq: 1 REGISTER
Contact: <sip:[field0]@[local_ip]:[local_port]>;expires=3600
Content-Length: 0

]]>
  </send>
  <recv response="200"/>
  <pause milliseconds="200"/>
  <send retrans="500">
    <![CDATA[
REGISTER sip:127.0.0.1 SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
Max-Forwards: 70
To: <sip:[field0]@127.0.0.1>[peer_tag_param]
From: <sip:[field0]@127.0.0.1>;tag=[call_number]
Call-ID: [call_id]
CSeq: 2 REGISTER
Contact: <sip:[field0]@[local_ip]:[local_port]>;expires=0
Content-Length: 0

]]>
  </send>
  <recv response="200"/>
</scenario>
```

- [ ] **Step 4: Create SIPp UAC PUBLISH scenario**

Create `test/unit/topos_reg_pub_publish_uac.xml`:

```xml
<?xml version="1.0" encoding="ISO-8859-1" ?>
<!DOCTYPE scenario SYSTEM "sipp.dtd">
<scenario name="topos_reg_pub_publish_uac">
  <send retrans="500">
    <![CDATA[
PUBLISH sip:[field0]@127.0.0.1 SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
Max-Forwards: 70
To: <sip:[field0]@127.0.0.1>
From: <sip:[field0]@127.0.0.1>;tag=[call_number]
Call-ID: [call_id]
CSeq: 1 PUBLISH
Contact: <sip:[field0]@[local_ip]:[local_port]>
Event: presence
Expires: 60
Content-Type: application/pidf+xml
Content-Length: 0

]]>
  </send>
  <recv response="200"/>
</scenario>
```

- [ ] **Step 5: Create test shell script**

Create `test/unit/topos_reg_pub.sh`:

```bash
#!/bin/bash
# topos REGISTER/PUBLISH topology hiding with SQLite storage

. include/common
. include/require.sh

SRV=5060
UAS=5070
UAC=5080
DB_FILE="/tmp/topos_reg_pub_test.db"
LOG_FILE="${RUN_DIR}/topos_reg_pub.log"
CFG=topos_reg_pub.cfg

if ! (check_sipp && check_kamailio \
		&& check_module "db_sqlite" && check_module "topos" \
		&& check_module "topoh"); then
	exit 0
fi

cleanup() {
	kill_kamailio 2>/dev/null
	killall -9 sipp 2>/dev/null
	rm -f "$DB_FILE" "$LOG_FILE" "${CFG}.bak"
}
trap cleanup EXIT

setup_db() {
	rm -f "$DB_FILE"
	sqlite3 "$DB_FILE" <<'EOSQL'
CREATE TABLE version (
    id INTEGER PRIMARY KEY NOT NULL,
    table_name VARCHAR(32) NOT NULL,
    table_version INTEGER DEFAULT 0 NOT NULL
);
CREATE TABLE topos_d (
    id INTEGER PRIMARY KEY NOT NULL,
    rectime DATETIME NOT NULL,
    x_context VARCHAR(64) DEFAULT '' NOT NULL,
    s_method VARCHAR(64) DEFAULT '' NOT NULL,
    s_cseq VARCHAR(64) DEFAULT '' NOT NULL,
    a_callid VARCHAR(255) DEFAULT '' NOT NULL,
    a_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    b_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    a_contact VARCHAR(512) DEFAULT '' NOT NULL,
    b_contact VARCHAR(512) DEFAULT '' NOT NULL,
    as_contact VARCHAR(512) DEFAULT '' NOT NULL,
    bs_contact VARCHAR(512) DEFAULT '' NOT NULL,
    a_tag VARCHAR(255) DEFAULT '' NOT NULL,
    b_tag VARCHAR(255) DEFAULT '' NOT NULL,
    a_rr TEXT, b_rr TEXT, s_rr TEXT,
    iflags INTEGER DEFAULT 0 NOT NULL,
    a_uri VARCHAR(255) DEFAULT '' NOT NULL,
    b_uri VARCHAR(255) DEFAULT '' NOT NULL,
    r_uri VARCHAR(255) DEFAULT '' NOT NULL,
    a_srcaddr VARCHAR(128) DEFAULT '' NOT NULL,
    b_srcaddr VARCHAR(128) DEFAULT '' NOT NULL,
    a_socket VARCHAR(128) DEFAULT '' NOT NULL,
    b_socket VARCHAR(128) DEFAULT '' NOT NULL
);
CREATE TABLE topos_t (
    id INTEGER PRIMARY KEY NOT NULL,
    rectime DATETIME NOT NULL,
    x_context VARCHAR(64) DEFAULT '' NOT NULL,
    s_method VARCHAR(64) DEFAULT '' NOT NULL,
    s_cseq VARCHAR(64) DEFAULT '' NOT NULL,
    a_callid VARCHAR(255) DEFAULT '' NOT NULL,
    a_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    b_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    direction INTEGER DEFAULT 0 NOT NULL,
    x_via TEXT,
    x_vbranch VARCHAR(255) DEFAULT '' NOT NULL,
    x_rr TEXT, y_rr TEXT, s_rr TEXT,
    x_uri VARCHAR(255) DEFAULT '' NOT NULL,
    a_contact VARCHAR(512) DEFAULT '' NOT NULL,
    b_contact VARCHAR(512) DEFAULT '' NOT NULL,
    as_contact VARCHAR(512) DEFAULT '' NOT NULL,
    bs_contact VARCHAR(512) DEFAULT '' NOT NULL,
    x_tag VARCHAR(255) DEFAULT '' NOT NULL,
    a_tag VARCHAR(255) DEFAULT '' NOT NULL,
    b_tag VARCHAR(255) DEFAULT '' NOT NULL,
    a_srcaddr VARCHAR(255) DEFAULT '' NOT NULL,
    b_srcaddr VARCHAR(255) DEFAULT '' NOT NULL,
    a_socket VARCHAR(128) DEFAULT '' NOT NULL,
    b_socket VARCHAR(128) DEFAULT '' NOT NULL
);
INSERT INTO version (table_name, table_version) VALUES ('topos_d', 2);
INSERT INTO version (table_name, table_version) VALUES ('topos_t', 2);
EOSQL
}

count_topos_d() {
	sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM topos_d WHERE iflags != 0;"
}

start_kamailio() {
	$BIN -L "$MOD_DIR" -Y "$RUN_DIR" -P "$PIDFILE" -w . \
		-f "$CFG" -E -e > "$LOG_FILE" 2>&1
	sleep 1
	test -f "$PIDFILE"
}

setup_db
start_kamailio || exit 1

sipp -sf topos_reg_pub_uas.xml -i 127.0.0.1 -p "$UAS" -m 1 -trace_err \
	> "${RUN_DIR}/topos_reg_pub_uas.log" 2>&1 &
UAS_PID=$!
sleep 1

sipp -sf topos_reg_pub_uac.xml -i 127.0.0.1 -p "$UAC" -m 1 \
	127.0.0.1:"$SRV" -trace_err > "${RUN_DIR}/topos_reg_pub_uac.log" 2>&1
ret=$?
wait "$UAS_PID" 2>/dev/null

if [ "$ret" -ne 0 ]; then
	exit 1
fi

# Call-ID must differ from UAC original when mask_callid=1
UAS_CALLID=$(grep -m1 '^UAS_CALLID=' "${RUN_DIR}/topos_reg_pub_uas.log" | cut -d= -f2-)
UAC_CALLID=$(grep -m1 '^Call-ID:' "${RUN_DIR}/topos_reg_pub_uac.log" | awk '{print $2}')
if [ -z "$UAS_CALLID" ] || [ "$UAS_CALLID" = "$UAC_CALLID" ]; then
	exit 1
fi

# Dialog row created then cleared after Expires=0
DLG_AFTER=$(count_topos_d)
if [ "$DLG_AFTER" != "0" ]; then
	exit 1
fi

# PUBLISH smoke
sipp -sf topos_reg_pub_uas.xml -i 127.0.0.1 -p "$UAS" -m 1 -trace_err \
	> /dev/null 2>&1 &
sleep 1
sipp -sf topos_reg_pub_publish_uac.xml -i 127.0.0.1 -p "$UAC" -m 1 \
	127.0.0.1:"$SRV" -trace_err > /dev/null 2>&1
ret=$?

exit "$ret"
```

- [ ] **Step 6: Make script executable and run (expect FAIL)**

```bash
chmod +x test/unit/topos_reg_pub.sh
cd test/unit && make run UNIT=topos_reg_pub.sh
```

Expected: FAIL — `enable_reg_pub` modparam unknown, REGISTER skipped, Call-ID not masked, or script errors before assertions pass.

- [ ] **Step 7: Commit test scaffolding**

```bash
git add test/unit/topos_reg_pub.cfg test/unit/topos_reg_pub_uas.xml \
	test/unit/topos_reg_pub_uac.xml test/unit/topos_reg_pub_publish_uac.xml \
	test/unit/topos_reg_pub.sh
git commit -m "test: add topos REGISTER/PUBLISH topology hiding unit tests"
```

---

### Task 2: Add `expires_valid` to storage struct

**Files:**
- Modify: `src/modules/topos/tps_storage.h:90-92`

- [ ] **Step 1: Add field to `tps_data_t`**

In `src/modules/topos/tps_storage.h`, after `uint32_t s_method_id;`:

```c
	uint32_t s_method_id;
	unsigned char expires_valid; /*!< Expires header was parsed into expires */
	int32_t expires;
```

Remove the existing standalone `int32_t expires;` line that follows `s_method_id` (merge as shown above).

- [ ] **Step 2: Build topos module**

```bash
make -C src/modules/topos
```

Expected: PASS (no errors)

- [ ] **Step 3: Commit**

```bash
git add src/modules/topos/tps_storage.h
git commit -m "topos: add expires_valid to tps_data_t for reg/pub cleanup"
```

---

### Task 3: Module parameters

**Files:**
- Modify: `src/modules/topos/topos_mod.c:87-92,174-176,278-284`

- [ ] **Step 1: Add globals after `_tps_param_mask_callid`**

In `src/modules/topos/topos_mod.c`:

```c
int _tps_param_mask_callid = 0;
int _tps_enable_reg_pub = 0;
/** 0 = first Contact only (+ NOTICE if multi); 1 = reject multi-Contact REGISTER/PUBLISH */
int _tps_reg_pub_multi_contact = 0;
```

- [ ] **Step 2: Export modparams in `params[]` after `mask_callid`**

```c
	{"mask_callid", PARAM_INT, &_tps_param_mask_callid},
	{"enable_reg_pub", PARAM_INT, &_tps_enable_reg_pub},
	{"reg_pub_multi_contact", PARAM_INT, &_tps_reg_pub_multi_contact},
```

- [ ] **Step 3: Add validation in `mod_init()` after `methods_update_time` parsing**

```c
	if(_tps_reg_pub_multi_contact < 0 || _tps_reg_pub_multi_contact > 1) {
		LM_ERR("invalid reg_pub_multi_contact %d (use 0=first or 1=reject)\n",
				_tps_reg_pub_multi_contact);
		return -1;
	}
```

- [ ] **Step 4: Build and commit**

```bash
make -C src/modules/topos
git add src/modules/topos/topos_mod.c
git commit -m "topos: add enable_reg_pub and reg_pub_multi_contact modparams"
```

---

### Task 4: Message-layer changes (`tps_msg.c`)

**Files:**
- Modify: `src/modules/topos/tps_msg.c:51-52,410-418,726-732,1244-1246`

- [ ] **Step 1: Add extern declarations**

After `extern int _tps_param_mask_callid;`:

```c
extern int _tps_enable_reg_pub;
```

- [ ] **Step 2: Update `tps_skip_msg()`**

Replace lines 417-418:

```c
	if(_tps_enable_reg_pub == 0) {
		if((get_cseq(msg)->method_id) & (METHOD_REGISTER | METHOD_PUBLISH))
			return 1;
	}
```

- [ ] **Step 3: Update `tps_pack_message()` before `return 0`**

After the `_tps_context_param` block (before `return 0;`):

```c
	if(ptsd->s_method_id == METHOD_SUBSCRIBE || ptsd->s_method_id == METHOD_PUBLISH
			|| ptsd->s_method_id == METHOD_REGISTER) {
		ptsd->expires_valid = 0;
		if(parse_headers(msg, HDR_EOH_F, 0) != -1 && msg->expires
				&& msg->expires->body.len > 0
				&& (msg->expires->parsed
						|| (parse_expires(msg->expires) >= 0))) {
			ptsd->expires = ((exp_body_t *)msg->expires->parsed)->val;
			ptsd->expires_valid = 1;
		}
	}
```

- [ ] **Step 4: Update failed-reply dialog cleanup in `tps_reply_received()`**

Replace the method bitmask at lines 1244-1246:

```c
	if(msg->first_line.u.reply.statuscode > 299
			&& (get_cseq(msg)->method_id
					& (METHOD_INVITE | METHOD_SUBSCRIBE | METHOD_REGISTER
							| METHOD_PUBLISH))) {
```

- [ ] **Step 5: Build, run test (partial pass expected), commit**

```bash
make -C src/modules/topos
cd test/unit && make run UNIT=topos_reg_pub.sh
git add src/modules/topos/tps_msg.c
git commit -m "topos: enable REGISTER/PUBLISH skip gate and expires parsing"
```

Expected after this task: test may still FAIL on storage/link_msg paths.

---

### Task 5: Storage-layer changes (`tps_storage.c`)

**Files:**
- Modify: `src/modules/topos/tps_storage.c:67-68,628-684,1282-1289,1771-1773,1929-1931,1963-1965`

- [ ] **Step 1: Add extern for multi-contact param**

After `extern int _tps_methods_update_time;`:

```c
extern int _tps_reg_pub_multi_contact;
```

- [ ] **Step 2: Update `tps_storage_link_msg()` — set method id and contact rules**

After `td->s_cseq = get_cseq(msg)->number;` add:

```c
	td->s_method_id = get_cseq(msg)->method_id;
```

Replace contact-required check (lines 633-634):

```c
		if((td->s_method_id != METHOD_INVITE)
				&& (td->s_method_id != METHOD_SUBSCRIBE)
				&& (td->s_method_id != METHOD_REGISTER)
				&& (td->s_method_id != METHOD_PUBLISH)) {
			/* no mandatory contact unless dialog-creating / reg / publish - done */
```

Replace multi-contact rejection block (lines 652-657):

```c
	if(parse_contact(msg->contact) < 0
			|| ((contact_body_t *)msg->contact->parsed)->contacts == NULL) {
		LM_ERR("bad Contact header\n");
		return -1;
	}
	if(((contact_body_t *)msg->contact->parsed)->contacts->next != NULL) {
		if((td->s_method_id == METHOD_REGISTER)
				|| (td->s_method_id == METHOD_PUBLISH)) {
			if(_tps_reg_pub_multi_contact == 1) {
				LM_ERR("topos: multi-Contact body rejected "
					   "(reg_pub_multi_contact=1), method %u\n",
						td->s_method_id);
				return -1;
			}
			LM_NOTICE("topos: multi-Contact body — using first URI only for "
					  "topology storage (method %u); full bindings belong to "
					  "registrar/usrloc (e.g. ims_registrar_pcscf / "
					  "ims_usrloc_pcscf)\n",
					td->s_method_id);
		} else {
			LM_ERR("bad Contact header (multiple contacts)\n");
			return -1;
		}
	}
```

Replace Expires block (lines 678-684):

```c
	if(td->s_method_id == METHOD_SUBSCRIBE || td->s_method_id == METHOD_PUBLISH
			|| td->s_method_id == METHOD_REGISTER) {
		td->expires_valid = 0;
		if(msg->expires && (msg->expires->body.len > 0)
				&& (msg->expires->parsed
						|| (parse_expires(msg->expires) >= 0))) {
			td->expires = ((exp_body_t *)msg->expires->parsed)->val;
			td->expires_valid = 1;
		}
	}
```

- [ ] **Step 3: Update `tps_db_load_branch()` method strings**

After the SUBSCRIBE block (line 1284):

```c
	} else if(get_cseq(msg)->method_id == METHOD_REGISTER) {
		sMethodDlg.s = "REGISTER";
		sMethodDlg.len = 8;
	} else if(get_cseq(msg)->method_id == METHOD_PUBLISH) {
		sMethodDlg.s = "PUBLISH";
		sMethodDlg.len = 7;
```

- [ ] **Step 4: Update `tps_storage_update_branch()` method guard**

Replace lines 1771-1773:

```c
	if((md->s_method_id != METHOD_INVITE)
			&& (md->s_method_id != METHOD_SUBSCRIBE)
			&& (md->s_method_id != METHOD_REGISTER)
			&& (md->s_method_id != METHOD_PUBLISH)) {
```

- [ ] **Step 5: Update `tps_storage_update_dialog()` method guard**

Replace lines 1929-1931 (same pattern as Step 4).

- [ ] **Step 6: Update `tps_db_end_dialog()` Expires=0 guard**

Replace lines 1963-1965:

```c
	if((md->s_method_id != METHOD_BYE)
			&& !((md->s_method_id == METHOD_SUBSCRIBE) && (md->expires_valid)
					&& (md->expires == 0))
			&& !((md->s_method_id == METHOD_REGISTER) && (md->expires_valid)
					&& (md->expires == 0))
			&& !((md->s_method_id == METHOD_PUBLISH) && (md->expires_valid)
					&& (md->expires == 0))) {
```

- [ ] **Step 7: Build, run primary test, commit**

```bash
make -C src/modules/topos
cd test/unit && make run UNIT=topos_reg_pub.sh
```

Expected: PASS

```bash
git add src/modules/topos/tps_storage.c
git commit -m "topos: REGISTER/PUBLISH storage, multi-Contact, and dialog lifecycle"
```

---

### Task 6: Documentation

**Files:**
- Modify: `src/modules/topos/doc/topos_admin.xml:35-38,170+`
- Modify: `src/modules/topos/README`

- [ ] **Step 1: Update overview paragraph in `topos_admin.xml`**

Replace lines 36-37:

```xml
		By default, REGISTER and PUBLISH requests are skipped from processing
		by this module. When modparam <varname>enable_reg_pub</varname> is
		set to 1, REGISTER and PUBLISH follow the same TOPOS encoding as
		INVITE-based dialogs (with <varname>mask_callid</varname> and topoh
		as for INVITE).
```

- [ ] **Step 2: Add parameter sections after `mask_callid` section**

Insert two new `<section>` blocks (adapt from `src/modules/topos_mod/doc/topos_admin.xml` sections `topos.p.enable_register_publish` and `topos.p.register_multi_contact`, renaming to `enable_reg_pub` and `reg_pub_multi_contact`).

- [ ] **Step 3: Regenerate README**

```bash
make -C src/modules/topos/doc
```

Expected: `src/modules/topos/README` updated

- [ ] **Step 4: Commit**

```bash
git add src/modules/topos/doc/topos_admin.xml src/modules/topos/README
git commit -m "topos: document enable_reg_pub and reg_pub_multi_contact"
```

---

### Task 7: HTable smoke test

**Files:**
- Create: `test/unit/topos_reg_pub_htable.cfg`
- Create: `test/unit/topos_reg_pub_htable.sh`

- [ ] **Step 1: Create HTable cfg**

Create `test/unit/topos_reg_pub_htable.cfg` — copy `topos_reg_pub.cfg` but replace storage lines:

```
loadmodule "htable/htable.so"
loadmodule "topos_htable/topos_htable.so"

modparam("topos", "storage", "htable")
```

Remove `db_sqlite` loadmodule and `db_url` modparam.

- [ ] **Step 2: Create smoke test script**

Create `test/unit/topos_reg_pub_htable.sh` — copy `topos_reg_pub.sh`, change `CFG=topos_reg_pub_htable.cfg`, remove `setup_db` and `count_topos_d` assertions (keep Call-ID masking check only), add `check_module "topos_htable" && check_module "htable"`.

- [ ] **Step 3: Run and commit**

```bash
chmod +x test/unit/topos_reg_pub_htable.sh
cd test/unit && make run UNIT=topos_reg_pub_htable.sh
git add test/unit/topos_reg_pub_htable.cfg test/unit/topos_reg_pub_htable.sh
git commit -m "test: add topos REGISTER/PUBLISH htable storage smoke test"
```

Expected: PASS

---

### Task 8: Manual IMS checklist

**Files:**
- Create: `docs/superpowers/specs/2026-06-07-topos-reg-pub-ims-checklist.md`

- [ ] **Step 1: Write checklist document**

```markdown
# TOPOS REGISTER/PUBLISH — Manual IMS Checklist

1. **Multi-Contact REGISTER (P-CSCF)**
   - Config: `enable_reg_pub=1`, `reg_pub_multi_contact=0`
   - Send REGISTER with 2+ Contact URIs
   - Expect: LM_NOTICE in logs; all bindings in registrar/usrloc

2. **rectime refresh**
   - Config: `methods_update_time=SUBSCRIBE,REGISTER`
   - Send re-REGISTER; query `topos_d.rectime` updates

3. **PUBLISH through presence server**
   - Config: `enable_reg_pub=1`, `mask_callid=1`
   - PUBLISH with Event/SIP-ETag; verify headers preserved after round-trip

4. **methods_noinitial opt-out**
   - Config: `enable_reg_pub=1`, `methods_noinitial=REGISTER`
   - Initial REGISTER skipped by TOPOS (Call-ID not masked)
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/specs/2026-06-07-topos-reg-pub-ims-checklist.md
git commit -m "docs: add manual IMS checklist for topos reg/pub"
```

---

### Task 9: Final verification

- [ ] **Step 1: Full module rebuild**

```bash
make -C src/modules/topos clean && make -C src/modules/topos
```

Expected: no warnings/errors

- [ ] **Step 2: Run both unit tests**

```bash
cd test/unit && make run UNIT=topos_reg_pub.sh
cd test/unit && make run UNIT=topos_reg_pub_htable.sh
```

Expected: both report `ok`

- [ ] **Step 3: Verify default backward compatibility**

Temporarily set `enable_reg_pub=0` in `topos_reg_pub.cfg`, rerun test — Call-ID assertion should FAIL (confirms legacy skip still works). Restore `enable_reg_pub=1`.

---

## Spec Coverage Self-Review

| Spec requirement | Task |
|------------------|------|
| `enable_reg_pub` modparam | Task 3, 4 |
| `reg_pub_multi_contact` modparam | Task 3, 5 |
| `expires_valid` in struct | Task 2 |
| `tps_skip_msg` conditional skip | Task 4 |
| `tps_pack_message` Expires | Task 4 |
| Failed reply cleanup | Task 4 |
| `tps_storage_link_msg` changes | Task 5 |
| Dialog load/update/end for REGISTER/PUBLISH | Task 5 |
| Documentation | Task 6 |
| SQLite automated tests | Task 1, 5, 9 |
| HTable smoke test | Task 7 |
| Manual IMS checklist | Task 8 |
| Preserve contact-building refactor | Approach A throughout — no changes to `tps_storage_fill_contact()` |
| No storage backend module changes | Confirmed out of scope |

No placeholders remain. Type/identifier consistency: `_tps_enable_reg_pub`, `_tps_reg_pub_multi_contact`, `expires_valid` used uniformly across tasks.
