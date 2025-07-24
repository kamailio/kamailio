CREATE TABLE tcp_connection(cid INT AUTO_INCREMENT NOT NULL,
		ssl_conn_id INT DEFAULT '0', start_timestamp DATETIME DEFAULT NULL,
		finish_timestamp DATETIME DEFAULT NULL,
		local_ip VARCHAR(64) DEFAULT NULL, local_port SMALLINT UNSIGNED,
		remote_ip VARCHAR(64) DEFAULT NULL, remote_port SMALLINT UNSIGNED,
		UNIQUE KEY tc_idx(cid));

CREATE TABLE session_key(cid INT NOT NULL,
		key_generation_timestamp DATETIME DEFAULT NULL,
		key_expiration_timestamp DATETIME DEFAULT NULL,
		session_key VARCHAR(1024) DEFAULT NULL);

INSERT INTO version(table_name, table_version) values('tcp_connection', '1');
INSERT INTO version(table_name, table_version) values('session_key', '1');
