-- SQL script for PDT module

DROP DATABASE IF EXISTS pdt;

CREATE DATABASE pdt;

USE pdt;

-- create table
CREATE TABLE pd_multidomain (
	sdomain VARCHAR(255) NOT NULL,
	prefix VARCHAR(32) NOT NULL,
	domain VARCHAR(255) NOT NULL DEFAULT "",
	PRIMARY KEY (sdomain, prefix)
);

