#
# DATABASE definition
#

DROP DATABASE IF EXISTS sip_jab;

CREATE DATABASE sip_jab;

USE sip_jab;

# jabber users
CREATE TABLE jusers(
	juid INT NOT NULL AUTO_INCREMENT,
	jab_id VARCHAR(128) NOT NULL,
	jab_passwd VARCHAR(50),
	sip_id VARCHAR(128) NOT NULL,
	type INT NOT NULL DEFAULT 0,
	PRIMARY KEY(juid),
	KEY(jab_id),
	KEY(sip_id)
);

# icq users
CREATE TABLE icq(
	juid INT NOT NULL,
	icq_id INT NOT NULL,
	icq_passwd VARCHAR(50),
	icq_nick VARCHAR(50),
	type INT NOT NULL DEFAULT 0,
	PRIMARY KEY(juid), # --- REFERENCES jusers(juid) ON UPDATE CASCADE ON DELETE CASCADE,
	KEY(icq_id)
);

# msn users
CREATE TABLE msn(
	juid INT NOT NULL,
	msn_id VARCHAR(128) NOT NULL,
	msn_passwd VARCHAR(50),
	msn_nick VARCHAR(50),
	type INT NOT NULL DEFAULT 0,
	PRIMARY KEY(juid), # --- REFERENCES jusers(juid) ON UPDATE CASCADE ON DELETE CASCADE,
	KEY(msn_id)
);

# aim users
CREATE TABLE aim(
	juid INT NOT NULL,
	aim_id VARCHAR(128) NOT NULL,
	aim_passwd VARCHAR(50),
	aim_nick VARCHAR(50),
	type INT NOT NULL DEFAULT 0,
	PRIMARY KEY(juid), # --- REFERENCES jusers(juid) ON UPDATE CASCADE ON DELETE CASCADE,
	KEY(aim_id)
);

# yahoo users
CREATE TABLE yahoo(
	juid INT NOT NULL,
	yahoo_id VARCHAR(128) NOT NULL,
	yahoo_passwd VARCHAR(50),
	yahoo_nick VARCHAR(50),
	type INT NOT NULL DEFAULT 0,
	PRIMARY KEY(juid), # --- REFERENCES jusers(juid) ON UPDATE CASCADE ON DELETE CASCADE,
	KEY(yahoo_id)
);
