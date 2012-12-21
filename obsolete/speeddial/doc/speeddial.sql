CREATE TABLE speed_dial (
  username varchar(64) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  sd_username varchar(64) NOT NULL default '',
  sd_domain varchar(128) NOT NULL default '',
  new_uri varchar(192) NOT NULL default '',
  description varchar(64) NOT NULL default '',
  PRIMARY KEY (username, domain, sd_domain, sd_username)
);
