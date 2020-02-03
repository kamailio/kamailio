INSERT INTO version (table_name, table_version) values ('ro_session','6');
CREATE TABLE `ro_session` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `hash_entry` int(10) unsigned NOT NULL,
  `hash_id` int(10) unsigned NOT NULL,
  `session_id` varchar(100) NOT NULL,
  `dlg_hash_entry` int(10) unsigned NOT NULL,
  `dlg_hash_id` int(10) unsigned NOT NULL,
  `direction` int(1) unsigned NOT NULL,
  `asserted_identity` varchar(100) NOT NULL,
  `callee` varchar(100) NOT NULL,
  `start_time` datetime DEFAULT NULL,
  `last_event_timestamp` datetime DEFAULT NULL,
  `reserved_secs` int(10) DEFAULT NULL,
  `valid_for` int(10) DEFAULT NULL,
  `state` int(1) DEFAULT NULL,
  `incoming_trunk_id` varchar(20) DEFAULT NULL,
  `outgoing_trunk_id` varchar(20) DEFAULT NULL,
  `rating_group` int(11) DEFAULT NULL,
  `service_identifier` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `hash_idx` (`hash_entry`,`hash_id`)
);

