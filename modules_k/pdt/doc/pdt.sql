-- SQL script for PDT module

DROP DATABASE IF EXISTS pdt;

CREATE DATABASE pdt;

USE pdt;

-- create table
CREATE TABLE prefix_domain (
	prefix VARCHAR(32) NOT NULL PRIMARY KEY,
	domain VARCHAR(255) NOT NULL DEFAULT ""
);

