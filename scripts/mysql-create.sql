create database ser;
use ser;

create table location (
    user varchar(255)    not null,
    contact varchar(255) not null,
    expire datetime,
    q float(10,2),
    last_modified timestamp(14),
    key(user)
);

