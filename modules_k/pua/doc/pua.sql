
use openser;

CREATE TABLE pua (
  id int(10) unsigned NOT NULL auto_increment,
  pres_uri varchar(128) NOT NULL,
  pres_id varchar(128) NOT NULL,
  event int(11) NOT NULL,  
  expires int(11) NOT NULL,
  flag int(11) NOT NULL,
  etag varchar(128) NOT NULL,
  tuple_id varchar(128) NOT NULL,
  watcher_uri varchar(128) NOT NULL,
  call_id varchar(128) NOT NULL,
  to_tag varchar(128) NOT NULL,
  from_tag varchar(128) NOT NULL,
  cseq int(11) NOT NULL,
  record_route text NULL,
  version int(11) NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM;

