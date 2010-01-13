drop table if exists rls_subscription;
CREATE TABLE rls_subscription (
  id			varchar(48) NOT NULL,
  doc_version	int,
  dialog		blob,
  expires		datetime NOT NULL,
  status		int,
  contact		varchar(128),
  uri			varchar(128),
  package		varchar(128),
  w_uri			varchar(128),
  PRIMARY KEY  (id)
) TYPE=MyISAM;

drop table if exists rls_vs;
CREATE TABLE rls_vs (
  id			varchar(48) NOT NULL,
  rls_id		varchar(48) NOT NULL,
  uri			varchar(128),
  PRIMARY KEY  (id)
) TYPE=MyISAM;

drop table if exists rls_vs_names;
CREATE TABLE rls_vs_names (
  id			varchar(48) NOT NULL,
  name			varchar(64),
  lang			varchar(64)
) TYPE=MyISAM;
