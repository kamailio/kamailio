/*
 * Copyright (C) 2019 Vicente Hernando (Sonoc https://www.sonoc.io)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/** \file
 * \brief Kafka :: Apache Kafka functions via librdkafka
 * \ingroup kfk
 *
 * - \ref kfk.c
 * - Module: \ref kfk
 */

#ifndef _KFK_H
#define _KFK_H

/**
 * \brief Initialize kafka functionality.
 *
 * \param brokers brokers to add.
 * \return 0 on success.
 */
int kfk_init(char *brokers);

/**
 * \brief Close kafka related functionality.
 */
void kfk_close();

/**
 * \brief Parse general configuration properties for Kafka.
 */
int kfk_conf_parse(char *spec);

/**
 * \brief Parse topic properties for Kafka.
 */
int kfk_topic_parse(char *spec);

/**
 * \brief send a message to a topic.
 *
 * \return 0 on success.
 */
int kfk_message_send(str *topic, str *message);

/**
 * \brief Initialize statistics.
 *
 * \return 0 on success.
 */
int kfk_stats_init();

/**
 * \brief Close statistics.
 */
void kfk_stats_close();

/**
 * \brief Get total statistics.
 *
 * \param msg_total return total number of messages by reference.
 * \param msg_error return total number of errors by reference.
 *
 * \return 0 on success.
 */
int kfk_stats_get(uint64_t *msg_total, uint64_t *msg_error);

/**
 * \brief Get statistics for a specified topic.
 *
 * \param s_topic string with topic name.
 * \param msg_total return total number of messages by reference.
 * \param msg_error return total number of errors by reference.
 *
 * \return 0 on success.
 */
int kfk_stats_topic_get(str *s_topic, uint64_t *msg_total, uint64_t *msg_error);

#endif /* KFK_H */
