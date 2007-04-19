
use openser;

CREATE TABLE `xcap_xml` (
  `id` int(10) NOT NULL auto_increment,
  `username` varchar(66) NOT NULL,
  `domain` varchar(128) NOT NULL,
  `xcap` text NOT NULL,
  `doc_type` int(11) NOT NULL,
  UNIQUE KEY udd_xcap (`username`,`domain`,`doc_type`),
  PRIMARY KEY (id)
) ENGINE=MyISAM;
