INSERT INTO version (table_name, table_version) values ('dialog_in', 7), ('dialog_out', 7), ('dialog_vars', 7);

CREATE TABLE `dialog_in` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `hash_entry` int(10) unsigned NOT NULL,
  `hash_id` int(10) unsigned NOT NULL,
  `did` varchar(45) NOT NULL,
  `callid` varchar(255) NOT NULL,
  `from_uri` varchar(128) NOT NULL,
  `from_tag` varchar(64) NOT NULL,
  `caller_original_cseq` varchar(20) NOT NULL,
  `req_uri` varchar(128) NOT NULL,
  `caller_route_set` varchar(512) DEFAULT NULL,
  `caller_contact` varchar(128) NOT NULL,
  `caller_sock` varchar(64) NOT NULL,
  `state` int(10) unsigned NOT NULL,
  `start_time` int(10) unsigned NOT NULL,
  `timeout` int(10) unsigned NOT NULL DEFAULT '0',
  `sflags` int(10) unsigned NOT NULL DEFAULT '0',
  `toroute_name` varchar(32) DEFAULT NULL,
  `toroute_index` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `hash_idx` (`hash_entry`,`hash_id`)
);

CREATE TABLE `dialog_out` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `hash_entry` int(11) NOT NULL,
  `hash_id` int(11) NOT NULL,
  `did` varchar(45) NOT NULL,
  `to_uri` varchar(128) NOT NULL,
  `to_tag` varchar(64) NOT NULL,
  `caller_cseq` varchar(20) NOT NULL,
  `callee_cseq` varchar(20) NOT NULL,
  `callee_contact` varchar(128) NOT NULL,
  `callee_route_set` varchar(512) DEFAULT NULL,
  `callee_sock` varchar(64) NOT NULL,
  PRIMARY KEY (`id`)
);

CREATE TABLE `dialog_vars` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `hash_entry` int(10) unsigned NOT NULL,
  `hash_id` int(10) unsigned NOT NULL,
  `dialog_key` varchar(128) NOT NULL,
  `dialog_value` varchar(512) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `hash_idx` (`hash_entry`,`hash_id`)
);



