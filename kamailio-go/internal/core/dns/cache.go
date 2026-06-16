// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * DNS cache - matching C dns_cache.h
 *
 * Provides caching for DNS records with TTL support.
 */

package dns

import (
	"sync"
	"time"
)

// CacheEntry represents a cached DNS entry
type CacheEntry struct {
	Addrs     []*Addr
	ExpiresAt time.Time
}

// IsExpired returns true if the entry has expired
func (e *CacheEntry) IsExpired() bool {
	return time.Now().After(e.ExpiresAt)
}

// Cache represents a DNS cache
type Cache struct {
	entries map[string]*CacheEntry
	ttl     time.Duration
	mu      sync.RWMutex
}

// NewCache creates a new DNS cache with the specified TTL
func NewCache(ttl time.Duration) *Cache {
	c := &Cache{
		entries: make(map[string]*CacheEntry),
		ttl:     ttl,
	}

	// Start cleanup goroutine
	go c.cleanup()

	return c
}

// Get retrieves addresses from the cache
func (c *Cache) Get(key string) []*Addr {
	c.mu.RLock()
	defer c.mu.RUnlock()

	entry, ok := c.entries[key]
	if !ok || entry.IsExpired() {
		return nil
	}

	return entry.Addrs
}

// Set stores addresses in the cache
func (c *Cache) Set(key string, addrs []*Addr) {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.entries[key] = &CacheEntry{
		Addrs:     addrs,
		ExpiresAt: time.Now().Add(c.ttl),
	}
}

// SetWithTTL stores addresses with a custom TTL
func (c *Cache) SetWithTTL(key string, addrs []*Addr, ttl time.Duration) {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.entries[key] = &CacheEntry{
		Addrs:     addrs,
		ExpiresAt: time.Now().Add(ttl),
	}
}

// Delete removes an entry from the cache
func (c *Cache) Delete(key string) {
	c.mu.Lock()
	defer c.mu.Unlock()

	delete(c.entries, key)
}

// Clear removes all entries from the cache
func (c *Cache) Clear() {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.entries = make(map[string]*CacheEntry)
}

// Size returns the number of entries in the cache
func (c *Cache) Size() int {
	c.mu.RLock()
	defer c.mu.RUnlock()

	return len(c.entries)
}

// cleanup periodically removes expired entries
func (c *Cache) cleanup() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		c.mu.Lock()
		for key, entry := range c.entries {
			if entry.IsExpired() {
				delete(c.entries, key)
			}
		}
		c.mu.Unlock()
	}
}

// CacheStats represents cache statistics
type CacheStats struct {
	TotalEntries int
	ExpiredEntries int
}

// Stats returns cache statistics
func (c *Cache) Stats() CacheStats {
	c.mu.RLock()
	defer c.mu.RUnlock()

	stats := CacheStats{
		TotalEntries: len(c.entries),
	}

	for _, entry := range c.entries {
		if entry.IsExpired() {
			stats.ExpiredEntries++
		}
	}

	return stats
}

// Global default cache
var defaultCache = NewCache(5 * time.Minute)

// GetDefaultCache returns the default DNS cache
func GetDefaultCache() *Cache {
	return defaultCache
}
