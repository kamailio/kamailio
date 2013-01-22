/*
 * 
 *   Postgress SQL Schema for Sipcapture
 *   Author: Ovind Kolbu
 *
*/

CREATE TABLE sip_capture (
        id SERIAL NOT NULL,
        date TIMESTAMP WITHOUT TIME ZONE DEFAULT '1900-01-01 00:00:01' NOT NULL,
        micro_ts BIGINT NOT NULL DEFAULT '0',
        method VARCHAR(50) NOT NULL DEFAULT '',
        reply_reason VARCHAR(100) NOT NULL,
        ruri VARCHAR(200) NOT NULL DEFAULT '',
        ruri_user VARCHAR(100) NOT NULL DEFAULT '',
        from_user VARCHAR(100) NOT NULL DEFAULT '',
        from_tag VARCHAR(64) NOT NULL DEFAULT '',
        to_user VARCHAR(100) NOT NULL DEFAULT '',
        to_tag VARCHAR(64) NOT NULL,
        pid_user VARCHAR(100) NOT NULL DEFAULT '',
        contact_user VARCHAR(120) NOT NULL,
        auth_user VARCHAR(120) NOT NULL,
        callid VARCHAR(100) NOT NULL DEFAULT '',
        callid_aleg VARCHAR(100) NOT NULL DEFAULT '',
        via_1 VARCHAR(256) NOT NULL,
        via_1_branch VARCHAR(80) NOT NULL,
        cseq VARCHAR(25) NOT NULL,
        diversion VARCHAR(256), /* MySQL: NOT NULL */
        reason VARCHAR(200) NOT NULL,
        content_type VARCHAR(256) NOT NULL,
        auth VARCHAR(256) NOT NULL,
        user_agent VARCHAR(256) NOT NULL,
        source_ip VARCHAR(60) NOT NULL DEFAULT '',
        source_port INTEGER NOT NULL,
        destination_ip VARCHAR(60) NOT NULL DEFAULT '',
        destination_port INTEGER NOT NULL,
        contact_ip VARCHAR(60) NOT NULL,
        contact_port INTEGER NOT NULL,
        originator_ip VARCHAR(60) NOT NULL DEFAULT '',
        originator_port INTEGER NOT NULL,
        proto INTEGER NOT NULL,
        family INTEGER NOT NULL,
        rtp_stat VARCHAR(256) NOT NULL,
        type INTEGER NOT NULL,
        node VARCHAR(125) NOT NULL,
        msg VARCHAR(1500) NOT NULL,
        PRIMARY KEY (id,date)
);

CREATE INDEX sip_capture_ruri_user_idx ON sip_capture (ruri_user);
CREATE INDEX sip_capture_from_user_idx ON sip_capture (from_user);
CREATE INDEX sip_capture_to_user_idx ON sip_capture (to_user);
CREATE INDEX sip_capture_pid_user_idx ON sip_capture (pid_user);
CREATE INDEX sip_capture_auth_user_idx ON sip_capture (auth_user);
CREATE INDEX sip_capture_callid_aleg_idx ON sip_capture (callid_aleg);
CREATE INDEX sip_capture_date_idx ON sip_capture (date);
CREATE INDEX sip_capture_callid_idx ON sip_capture (callid);


