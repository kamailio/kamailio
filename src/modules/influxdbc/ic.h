/*
 * Influx C (ic) client for data capture header file
 * Developer: Nigel Griffiths.
 * (C) Copyright 2021 Nigel Griffiths
 */
void ic_influx_database(char *host, long port, char *db);
void ic_influx_userpw(char *user, char *pw);
void ic_tags(char *tags);

void ic_measure(char *section);
void ic_measureend();

void ic_sub(char *sub_name);
void ic_subend();

void ic_long(char *name, long long value);
void ic_double(char *name, double value);
void ic_string(char *name, char *value);

void ic_push();
void ic_debug(int level);
