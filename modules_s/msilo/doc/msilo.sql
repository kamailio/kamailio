
-- SQL script for MSILO module

DROP DATABASE IF EXISTS msilo;

-- create a database for storage
CREATE DATABASE msilo;

USE msilo;

-- create the table
CREATE TABLE silo(
      -- unique ID per message
    mid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
      -- src address - From URI
    src_addr VARCHAR(255) NOT NULL DEFAULT "",
      -- dst address - To URI
    dst_addr VARCHAR(255) NOT NULL DEFAULT "",
      -- r-uri == username@domain (for compatibility with old version)
    r_uri VARCHAR(255) NOT NULL DEFAULT "",
      -- username
    username VARCHAR(64) NOT NULL DEFAULT "",
      -- domain
    domain VARCHAR(128) NOT NULL DEFAULT "",
      -- incoming time
    inc_time INTEGER NOT NULL DEFAULT 0,
      -- expiration time
    exp_time INTEGER NOT NULL DEFAULT 0,
      -- content type
    ctype VARCHAR(32) NOT NULL DEFAULT "text/plain",
      -- body of the message
    body BLOB NOT NULL DEFAULT ""
);

