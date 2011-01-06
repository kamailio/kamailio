drop table location;
CREATE TABLE location (
username VARCHAR(64) NOT NULL,
aor VARCHAR(255) NOT NULL,
contact VARCHAR(255) NOT NULL,
server_id INT NOT NULL DEFAULT '0',
received VARCHAR(255),
expires DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
q FLOAT NOT NULL DEFAULT '1.0',
callid VARCHAR(255),
cseq INT UNSIGNED,
flags INT UNSIGNED NOT NULL DEFAULT '0',
cflags INT UNSIGNED NOT NULL DEFAULT '0',
user_agent VARCHAR(64),
instance VARCHAR(255),
UNIQUE KEY location_key (username, contact),
KEY location_contact (contact),
KEY location_expires (expires)
);

