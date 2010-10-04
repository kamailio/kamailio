#!/usr/bin/php -q
<?php

// This script upgrades lcr tables from sr 3.0 to sr 3.1

$db_host = "localhost";
$db_name = "ser";
$db_user = "rwuser";
$db_pass = "password";

function db_connect($db_name) {
  global $db_host, $db_user, $db_pass, $db_conn;
  $db_conn = @mysql_connect($db_host, $db_user, $db_pass)
	  or die("Failed to connect to database '$db_name': " .
		 mysql_error() . "!" );
  if (!mysql_select_db($db_name)) {
    mysql_close($db_conn);
    die("Unknown database: '$db_name'!");
  }
}

function db_disconnect() {
  global $db_conn;
  mysql_close($db_conn) or
    die ("Failed to disconnect from database!");
}
  
function create_lcr_rule_table() {

  // create 'ser/lcr_rule' table

  global $db_name;

  db_connect($db_name);

  $result = mysql_query("SELECT table_version FROM version WHERE table_name='lcr_rule'")
    or die("Failed to select from 'ser/version' table!\n");
  if (mysql_num_rows($result) == 1) {
    db_disconnect();
    echo "Nothing to do for 'ser/lrc_rules' table\n";
    return;
  }

  mysql_query("CREATE TABLE lcr_rule (id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL, lcr_id SMALLINT UNSIGNED NOT NULL, prefix VARCHAR(16) DEFAULT NULL, from_uri VARCHAR(64) DEFAULT NULL, stopper TINYINT UNSIGNED NOT NULL DEFAULT 0, enabled TINYINT UNSIGNED NOT NULL DEFAULT 1, CONSTRAINT lcr_id_prefix_from_uri_idx UNIQUE (lcr_id, prefix, from_uri))")
    or die("Failed to create 'ser/lcr_rule' table!\n");

  mysql_query("INSERT INTO version SET table_name='lcr_rule', table_version=1")
    or die ("Failed to insert into 'ser/version'\n");

  db_disconnect();

  echo "Table 'ser/lcr_rule' created\n";
}
  
function create_lcr_rule_target_table() {

  // create 'ser/lcr_rule_target' table

  global $db_name;
  
  db_connect($db_name);

  $result = mysql_query("SELECT table_version FROM version WHERE table_name='lcr_rule_target'")
    or die("Failed to select from 'ser/version' table!\n");
  if (mysql_num_rows($result) == 1) {
    db_disconnect();
    echo "Nothing to do for 'ser/lrc_rule_targets' table\n";
    return;
  }

  mysql_query("CREATE TABLE lcr_rule_target (id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL, lcr_id INT UNSIGNED NOT NULL, rule_id INT UNSIGNED NOT NULL, gw_id INT UNSIGNED NOT NULL, priority TINYINT UNSIGNED NOT NULL DEFAULT 1, weight TINYINT UNSIGNED NOT NULL DEFAULT 1, CONSTRAINT rule_id_gw_id_idx UNIQUE (rule_id, gw_id), INDEX lcr_id_idx (lcr_id))")
    or die("Failed to create 'ser/lcr_rule_target' table!\n");

  mysql_query("INSERT INTO version SET table_name='lcr_rule_target', table_version=1")
    or die ("Failed to insert into 'ser/version'\n");

  db_disconnect();

  echo "Table 'ser/lcr_rule_target' created\n";
}
  
function create_lcr_gw_table() {

  // create 'ser/lcr_gw' table

  global $db_name;
  
  db_connect($db_name);

  $result = mysql_query("SELECT table_version FROM version WHERE table_name='lcr_gw'")
    or die("Failed to select from 'ser/version' table!\n");
  if (mysql_num_rows($result) == 1) {
    db_disconnect();
    echo "Nothing to do for 'ser/lrc_gws' table\n";
    return;
  }

  mysql_query("CREATE TABLE lcr_gw (id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL, lcr_id SMALLINT UNSIGNED NOT NULL, gw_name VARCHAR(128) DEFAULT NULL, uri_scheme TINYINT DEFAULT NULL, ip_addr VARCHAR(15) DEFAULT NULL, port SMALLINT DEFAULT NULL, transport TINYINT DEFAULT NULL, params VARCHAR(64) DEFAULT NULL, hostname VARCHAR(64) DEFAULT NULL, strip TINYINT UNSIGNED DEFAULT NULL, tag VARCHAR(16) DEFAULT NULL, flags INT UNSIGNED NOT NULL DEFAULT 0, defunct INT UNSIGNED DEFAULT NULL, CONSTRAINT lcr_id_ip_addr_port_hostname_idx UNIQUE (lcr_id, ip_addr, port, hostname))")
    or die("Failed to create 'ser/lcr_gw' table!\n");

  mysql_query("INSERT INTO version SET table_name='lcr_gw', table_version=1")
    or die ("Failed to insert into 'ser/version'\n");

  db_disconnect();

  echo "Table 'ser/lcr_gw' created\n";
}

function update_lcr_tables() {

  // update 3.1 lcr tables based on 3.0 lcr tables

  global $db_name;

  db_connect($db_name);

  $result = mysql_query("SELECT table_version FROM version WHERE table_name='lcr'")
    or die("Could not select from 'ser/version' table\n");
  if (mysql_num_rows($result) == 0) {
    echo "Nothing to do for 'ser' lcr tables\n";
    return;
  }

  $lcrs = array();
  $result = mysql_query("SELECT * FROM lcr")
    or die ("Failed to select from 'ser/lcr' table\n");
  while ($result && $row = mysql_fetch_assoc($result)) {
    $lcrs[] = $row;
  }
  db_disconnect();
  foreach ($lcrs as $lcr) {
    $lcr_id = $lcr['lcr_id'];
    $prefix = $lcr['prefix'];
    $from_uri = $lcr['from_uri'];
    if ($from_uri) {
      $from_select_uri = "='" . $from_uri . "'";
      $from_insert_uri = "'" . $from_uri . "'";
    } else {
      $from_select_uri = " IS NULL";
      $from_insert_uri = "NULL";
    }
    $grp_id = $lcr['grp_id'];
    $priority = $lcr['priority'];
    db_connect($db_name);
    $result = mysql_query("SELECT id FROM lcr_rule WHERE lcr_id=$lcr_id AND prefix='$prefix' AND from_uri$from_select_uri")
      or die ("Failed to select from 'ser/lcr_rule' table\n");
    if (mysql_num_rows($result) == 0) {
      mysql_query("INSERT INTO lcr_rule SET lcr_id = $lcr_id, prefix='$prefix', from_uri=$from_insert_uri")
	or die ("Failed to insert into 'ser/lcr_rule' table\n");
      $rule_id = mysql_insert_id();
    } else {
      $row = mysql_fetch_assoc($result);
      $rule_id = $row['id'];
    }
    $result = mysql_query("SELECT * FROM gw WHERE lcr_id=$lcr_id AND grp_id=$grp_id")
      or die("Failed to select from 'ser/gw' table\n");
    $gws = array();
    while ($result && $row = mysql_fetch_assoc($result)) {
      $gws[] = $row;
    }
    foreach ($gws as $gw) {
      $gw_name = $gw['gw_name'];
      if ($gw_name) {
	$gw_name = "'" . $gw_name . "'";
      } else {
	$gw_name = "NULL";
      }
      $ip_addr = $gw['ip_addr'];
      if ($ip_addr) {
	$select_ip_addr = "='" . $ip_addr . "'";
	$ip_addr = "'" . $ip_addr . "'";
      } else {
	$select_ip_addr = " IS NULL";
	$ip_addr = "NULL";
      }
      $hostname = $gw['hostname'];
      if ($hostname) {
	$select_hostname = "='" . $hostname . "'";
	$hostname = "'" . $hostname . "'";
      } else {
	$select_hostname = " IS NULL";
	$hostname = "NULL";
      }
      $port = $gw['port'];
      if (!$port) {
	$select_port = " IS NULL";
	$port = "NULL";
      } else {
	$select_port = "=$port";
      }
      $params = $gw['params'];
      if ($params) {
	$params = "'" . $params . "'";
      } else {
	$params = "NULL";
      }
      $uri_scheme = $gw['uri_scheme'];
      if (!$uri_scheme) {
	$uri_scheme = "NULL";
      }
      $transport = $gw['transport'];
      if (!$transport) {
	$transport = "NULL";
      }
      $strip = $gw['strip'];
      $tag = $gw['tag'];
      if ($tag) {
	$tag = "'" . $tag . "'";
      } else {
	$tag = "NULL";
      }
      $weight = $gw['weight'];
      if (!$weight) $weight = 1;
      $flags = $gw['flags'];
      $defunct = $gw['defunct'];
      if (!$defunct) {
	$defunct = "NULL";
      }
      $result = mysql_query("SELECT id FROM lcr_gw WHERE (ip_addr IS NOT NULL AND lcr_id=$lcr_id AND ip_addr$select_ip_addr AND port$select_port) OR (lcr_id=$lcr_id AND ip_addr IS NULL AND hostname$select_hostname AND port$select_port)")
	or die ("Failed to select from 'ser/lcr_gw' table\n");
      if (mysql_num_rows($result) == 0) {
	mysql_query("INSERT INTO lcr_gw SET lcr_id=$lcr_id, gw_name=$gw_name, uri_scheme=$uri_scheme, ip_addr=$ip_addr, port=$port, transport=$transport, params=$params, hostname=$hostname, strip=$strip, tag=$tag, flags=$flags, defunct=$defunct")
	  or die ("Failed to insert into 'ser/lcr_gw' table\n");
	$gw_id = mysql_insert_id();
      } else {
	$row = mysql_fetch_assoc($result);
	$gw_id = $row['id'];
      }
      mysql_query("INSERT INTO lcr_rule_target SET lcr_id=$lcr_id, rule_id=$rule_id, gw_id=$gw_id, priority=$priority, weight=$weight")
	or die ("Failed to insert into 'ser/lcr_rule_target' table\n");
    }
  }

  mysql_query("DELETE FROM version WHERE table_name='lcr' OR table_name='gw'")
    or die ("Failed to delete from 'ser/version' table\n");

  db_disconnect();

  echo "lcr tables updated\n";
}

echo "Starting upgrade ...\n";

create_lcr_rule_table();

create_lcr_rule_target_table();

create_lcr_gw_table();

update_lcr_tables();

echo "Upgrade done.\n";

?>
