create table dbaliases (
  alias_username varchar(64) not null default '',
  alias_domain varchar(128) not null default '',
  username varchar(64) not null default '',
  domain varchar(128) not null default '',
  unique key alias_key(alias_username, alias_domain)
);
