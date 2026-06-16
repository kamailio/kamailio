// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * AVP (Attribute-Value Pair) system - matching C usr_avp.c
 */

package avp

import (
	"fmt"
	"sync"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

// AVPType represents the type of an AVP value
type AVPType int

const (
	AVPTypeStr AVPType = iota
	AVPTypeInt
	AVPTypeFloat
	AVPTypeDouble
)

// AVP represents an attribute-value pair
type AVP struct {
	Name  str.Str
	Value AVPValue
	Type  AVPType
	Flags uint16
	ID    int64
}

// AVPValue represents the value of an AVP
type AVPValue interface {
	String() string
}

// StringValue represents a string AVP value
type StringValue struct {
	Val str.Str
}

func (s *StringValue) String() string {
	return s.Val.String()
}

// IntValue represents an integer AVP value
type IntValue struct {
	Val int64
}

func (i *IntValue) String() string {
	return fmt.Sprintf("%d", i.Val)
}

// FloatValue represents a float AVP value
type FloatValue struct {
	Val float64
}

func (f *FloatValue) String() string {
	return fmt.Sprintf("%f", f.Val)
}

// AvpTable represents a collection of AVPs
type AvpTable struct {
	byName map[string][]*AVP
	byID   map[int64][]*AVP
	mu     sync.RWMutex
}

// NewAvpTable creates a new AVP table
func NewAvpTable() *AvpTable {
	return &AvpTable{
		byName: make(map[string][]*AVP),
		byID:   make(map[int64][]*AVP),
	}
}

// Add adds an AVP to the table
func (t *AvpTable) Add(avp *AVP) {
	t.mu.Lock()
	defer t.mu.Unlock()

	if avp.Name.Len > 0 {
		key := avp.Name.String()
		t.byName[key] = append(t.byName[key], avp)
	}
	if avp.ID != 0 {
		t.byID[avp.ID] = append(t.byID[avp.ID], avp)
	}
}

// GetByName returns AVPs by name
func (t *AvpTable) GetByName(name string) []*AVP {
	t.mu.RLock()
	defer t.mu.RUnlock()
	return t.byName[name]
}

// GetByID returns AVPs by ID
func (t *AvpTable) GetByID(id int64) []*AVP {
	t.mu.RLock()
	defer t.mu.RUnlock()
	return t.byID[id]
}

// GetFirst returns the first AVP matching the name
func (t *AvpTable) GetFirst(name string) *AVP {
	avps := t.GetByName(name)
	if len(avps) > 0 {
		return avps[0]
	}
	return nil
}

// Delete removes AVPs by name
func (t *AvpTable) Delete(name string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	delete(t.byName, name)
}

// Clear removes all AVPs
func (t *AvpTable) Clear() {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.byName = make(map[string][]*AVP)
	t.byID = make(map[int64][]*AVP)
}

// Count returns the total number of AVPs
func (t *AvpTable) Count() int {
	t.mu.RLock()
	defer t.mu.RUnlock()
	count := 0
	for _, avps := range t.byName {
		count += len(avps)
	}
	return count
}

// NewStringAVP creates a string AVP
func NewStringAVP(name string, value string) *AVP {
	return &AVP{
		Name:  str.Mk(name),
		Value: &StringValue{Val: str.Mk(value)},
		Type:  AVPTypeStr,
	}
}

// NewIntAVP creates an integer AVP
func NewIntAVP(name string, value int64) *AVP {
	return &AVP{
		Name:  str.Mk(name),
		Value: &IntValue{Val: value},
		Type:  AVPTypeInt,
	}
}

// NewFloatAVP creates a float AVP
func NewFloatAVP(name string, value float64) *AVP {
	return &AVP{
		Name:  str.Mk(name),
		Value: &FloatValue{Val: value},
		Type:  AVPTypeFloat,
	}
}

// AddString adds a string AVP
func (t *AvpTable) AddString(name, value string) {
	t.Add(NewStringAVP(name, value))
}

// AddInt adds an integer AVP
func (t *AvpTable) AddInt(name string, value int64) {
	t.Add(NewIntAVP(name, value))
}

// GetString returns a string value
func GetString(avp *AVP) (string, bool) {
	if avp == nil {
		return "", false
	}
	if sv, ok := avp.Value.(*StringValue); ok {
		return sv.Val.String(), true
	}
	return "", false
}

// GetInt returns an integer value
func GetInt(avp *AVP) (int64, bool) {
	if avp == nil {
		return 0, false
	}
	if iv, ok := avp.Value.(*IntValue); ok {
		return iv.Val, true
	}
	return 0, false
}

// GetFloat returns a float value
func GetFloat(avp *AVP) (float64, bool) {
	if avp == nil {
		return 0, false
	}
	if fv, ok := avp.Value.(*FloatValue); ok {
		return fv.Val, true
	}
	return 0, false
}

// Global AVP table (for request context)
var GlobalTable = NewAvpTable()

// ContextAVPs returns a new AVP table
func ContextAVPs() *AvpTable {
	return NewAvpTable()
}
