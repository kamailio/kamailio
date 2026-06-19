// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Redis database backend - matching C db_redis
 *
 * Provides a Redis-backed database implementation using key-value
 * storage with hash tables for SIP data.
 *
 * Data model:
 *   - Each Kamailio table maps to a Redis hash
 *   - Row keys are stored as hash fields
 *   - Column values are stored as hash values
 */

package db

import (
	"fmt"
	"strings"
	"time"
)

// RedisConfig holds Redis connection configuration.
type RedisConfig struct {
	Host     string
	Port     int
	Password string
	DB       int
	Timeout  time.Duration
}

// DefaultRedisConfig returns default Redis configuration.
func DefaultRedisConfig() *RedisConfig {
	return &RedisConfig{
		Host:    "127.0.0.1",
		Port:    6379,
		DB:      0,
		Timeout: 5 * time.Second,
	}
}

// RedisConn represents a Redis database connection.
// This is a stub implementation - a full implementation would use
// github.com/redis/go-redis/v9 or similar.
type RedisConn struct {
	config *RedisConfig
	tables map[string][]*DBRow // in-memory fallback for testing
}

// RedisDriver is the Redis database driver.
type RedisDriver struct{}

// Name returns the driver name.
func (d *RedisDriver) Name() string {
	return "redis"
}

// Open creates a new Redis connection.
func (d *RedisDriver) Open(url string) (DBConn, error) {
	cfg := DefaultRedisConfig()
	// Parse URL: redis://[password@]host:port/db
	if strings.HasPrefix(url, "redis://") {
		rest := url[8:]
		if idx := strings.Index(rest, "@"); idx >= 0 {
			cfg.Password = rest[:idx]
			rest = rest[idx+1:]
		}
		parts := strings.SplitN(rest, "/", 2)
		addrParts := strings.SplitN(parts[0], ":", 2)
		cfg.Host = addrParts[0]
		if len(addrParts) > 1 {
			fmt.Sscanf(addrParts[1], "%d", &cfg.Port)
		}
		if len(parts) > 1 {
			fmt.Sscanf(parts[1], "%d", &cfg.DB)
		}
	}

	return &RedisConn{
		config: cfg,
		tables: make(map[string][]*DBRow),
	}, nil
}

// Query executes a SELECT.
func (c *RedisConn) Query(table string, keys []DBKey, where []DBCondition, orderBy string, limit, offset int) (*DBResult, error) {
	rows, ok := c.tables[table]
	if !ok {
		return &DBResult{Rows: nil, Keys: keys}, nil
	}

	var filtered []*DBRow
	for _, row := range rows {
		if matchRow(row, where) {
			filtered = append(filtered, row)
		}
	}

	if limit > 0 && limit < len(filtered) {
		filtered = filtered[:limit]
	}

	return &DBResult{Rows: filtered, Keys: keys}, nil
}

// Insert inserts a row.
func (c *RedisConn) Insert(table string, keys []DBKey, values []DBValue) error {
	c.tables[table] = append(c.tables[table], &DBRow{Keys: keys, Values: values})
	return nil
}

// Update updates matching rows.
func (c *RedisConn) Update(table string, keys []DBKey, values []DBValue, where []DBCondition) (int64, error) {
	var count int64
	for _, row := range c.tables[table] {
		if matchRow(row, where) {
			for i, k := range keys {
				for j, rk := range row.Keys {
					if rk.Name == k.Name && i < len(values) {
						row.Values[j] = values[i]
					}
				}
			}
			count++
		}
	}
	return count, nil
}

// Delete deletes matching rows.
func (c *RedisConn) Delete(table string, where []DBCondition) (int64, error) {
	var remaining []*DBRow
	var count int64
	for _, row := range c.tables[table] {
		if matchRow(row, where) {
			count++
		} else {
			remaining = append(remaining, row)
		}
	}
	c.tables[table] = remaining
	return count, nil
}

// Replace inserts or updates.
func (c *RedisConn) Replace(table string, keys []DBKey, values []DBValue) error {
	for _, row := range c.tables[table] {
		if len(row.Values) > 0 && len(values) > 0 && row.Values[0].String() == values[0].String() {
			row.Values = values
			return nil
		}
	}
	c.tables[table] = append(c.tables[table], &DBRow{Keys: keys, Values: values})
	return nil
}

// Raw executes a raw query.
func (c *RedisConn) Raw(query string, args ...interface{}) (*DBResult, error) {
	return nil, fmt.Errorf("raw queries not yet supported by redis backend")
}

// Close closes the connection.
func (c *RedisConn) Close() error {
	return nil
}

// Ping checks connection.
func (c *RedisConn) Ping() error {
	return nil // stub: always succeeds
}

func init() {
	RegisterDriver(&RedisDriver{})
}
