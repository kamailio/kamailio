create database ser;
use ser;

create table location (
    user varchar(50)    not null,
    contact varchar(255) not null,
    expires datetime,
    q float(10,2),
    callid varchar(255),
    cseq integer,
    last_modified timestamp(14),
    key(user,contact)
);

