-- SQL script for MSILO module

DROP DATABASE IF EXISTS msilo;

CREATE DATABASE msilo;

USE msilo;

-- create table
CREATE TABLE silo(
	mid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
	src_addr VARCHAR(128),
	dst_addr VARCHAR(128),
	inc_time INTEGER NOT NULL DEFAULT 0,
	exp_time INTEGER NOT NULL,
	ctype VARCHAR(32) NOT NULL DEFAULT "text/plain",
	body BLOB NOT NULL
);

