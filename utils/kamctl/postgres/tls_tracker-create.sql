CREATE TABLE tcp_connection(cid SERIAL NOT NULL,
		ssl_conn_id INTEGER DEFAULT '0', start_timestamp TIMESTAMP DEFAULT NULL,
		finish_timestamp TIMESTAMP DEFAULT NULL,
		local_ip VARCHAR(64) DEFAULT NULL, local_port INTEGER DEFAULT '0',
		remote_ip VARCHAR(64) DEFAULT NULL, remote_port INTEGER DEFAULT '0',
		CONSTRAINT tc_idx UNIQUE(cid));

CREATE TABLE session_key(cid INTEGER NOT NULL,
		key_generation_timestamp TIMESTAMP DEFAULT NULL,
		key_expiration_timestamp TIMESTAMP DEFAULT NULL,
		session_key VARCHAR(1024) DEFAULT NULL);

INSERT INTO version(table_name, table_version) values('tcp_connection', '1');
INSERT INTO version(table_name, table_version) values('session_key', '1');
