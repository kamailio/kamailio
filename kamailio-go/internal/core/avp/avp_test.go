// SPDX-License-Identifier: GPL-2.0-or-later

package avp

import (
	"testing"

	"github.com/kamailio/kamailio-go/internal/core/str"
)

func TestNewStringAVP(t *testing.T) {
	avp := NewStringAVP("foo", "bar")
	if avp == nil {
		t.Fatal("NewStringAVP returned nil")
	}
	if avp.Type != AVPTypeStr {
		t.Errorf("expected type %v, got %v", AVPTypeStr, avp.Type)
	}
	if avp.Name.String() != "foo" {
		t.Errorf("expected name 'foo', got %s", avp.Name.String())
	}
}

func TestNewIntAVP(t *testing.T) {
	avp := NewIntAVP("count", 42)
	if avp == nil {
		t.Fatal("NewIntAVP returned nil")
	}
	if avp.Type != AVPTypeInt {
		t.Errorf("expected type %v, got %v", AVPTypeInt, avp.Type)
	}
	if iv, ok := avp.Value.(*IntValue); !ok || iv.Val != 42 {
		t.Errorf("expected value 42, got %v", avp.Value)
	}
}

func TestNewFloatAVP(t *testing.T) {
	avp := NewFloatAVP("ratio", 3.14)
	if avp == nil {
		t.Fatal("NewFloatAVP returned nil")
	}
	if avp.Type != AVPTypeFloat {
		t.Errorf("expected type %v, got %v", AVPTypeFloat, avp.Type)
	}
}

func TestAvpTable_Add(t *testing.T) {
	table := NewAvpTable()
	avp := NewStringAVP("test", "value")
	table.Add(avp)

	if table.Count() != 1 {
		t.Errorf("expected count 1, got %d", table.Count())
	}
}

func TestAvpTable_GetByName(t *testing.T) {
	table := NewAvpTable()
	table.AddString("name1", "val1")
	table.AddString("name2", "val2")
	table.AddString("name1", "val3")

	avps := table.GetByName("name1")
	if len(avps) != 2 {
		t.Errorf("expected 2 AVPs for name1, got %d", len(avps))
	}

	avps = table.GetByName("nonexistent")
	if len(avps) != 0 {
		t.Errorf("expected 0 AVPs for nonexistent, got %d", len(avps))
	}
}

func TestAvpTable_GetFirst(t *testing.T) {
	table := NewAvpTable()
	table.AddString("test", "first")
	table.AddString("test", "second")

	first := table.GetFirst("test")
	if first == nil {
		t.Fatal("GetFirst returned nil")
	}
	if sv, ok := first.Value.(*StringValue); !ok {
		t.Errorf("expected StringValue, got %T", first.Value)
	} else if sv.Val.String() != "first" {
		t.Errorf("expected 'first', got %s", sv.Val.String())
	}

	none := table.GetFirst("nonexistent")
	if none != nil {
		t.Errorf("expected nil for nonexistent, got %v", none)
	}
}

func TestAvpTable_Delete(t *testing.T) {
	table := NewAvpTable()
	table.AddString("test", "value")
	table.Delete("test")

	if table.Count() != 0 {
		t.Errorf("expected count 0 after delete, got %d", table.Count())
	}
}

func TestAvpTable_Clear(t *testing.T) {
	table := NewAvpTable()
	table.AddString("a", "1")
	table.AddString("b", "2")
	table.Clear()

	if table.Count() != 0 {
		t.Errorf("expected count 0 after clear, got %d", table.Count())
	}
}

func TestAvpTable_AddInt(t *testing.T) {
	table := NewAvpTable()
	table.AddInt("count", 100)

	avps := table.GetByName("count")
	if len(avps) != 1 {
		t.Errorf("expected 1 AVP, got %d", len(avps))
	}
	if iv, ok := avps[0].Value.(*IntValue); !ok || iv.Val != 100 {
		t.Errorf("expected value 100, got %v", avps[0].Value)
	}
}

func TestGetString(t *testing.T) {
	avp := NewStringAVP("test", "hello")
	val, ok := GetString(avp)
	if !ok {
		t.Error("GetString returned false")
	}
	if val != "hello" {
		t.Errorf("expected 'hello', got %s", val)
	}

	// nil AVP
	_, ok = GetString(nil)
	if ok {
		t.Error("GetString should return false for nil")
	}

	// wrong type
	intAvp := NewIntAVP("test", 42)
	_, ok = GetString(intAvp)
	if ok {
		t.Error("GetString should return false for IntValue")
	}
}

func TestGetInt(t *testing.T) {
	avp := NewIntAVP("test", 123)
	val, ok := GetInt(avp)
	if !ok {
		t.Error("GetInt returned false")
	}
	if val != 123 {
		t.Errorf("expected 123, got %d", val)
	}

	// nil AVP
	_, ok = GetInt(nil)
	if ok {
		t.Error("GetInt should return false for nil")
	}
}

func TestGetFloat(t *testing.T) {
	avp := NewFloatAVP("test", 2.718)
	val, ok := GetFloat(avp)
	if !ok {
		t.Error("GetFloat returned false")
	}
	if val != 2.718 {
		t.Errorf("expected 2.718, got %f", val)
	}

	// nil AVP
	_, ok = GetFloat(nil)
	if ok {
		t.Error("GetFloat should return false for nil")
	}
}

func TestContextAVPs(t *testing.T) {
	table := ContextAVPs()
	if table == nil {
		t.Fatal("ContextAVPs returned nil")
	}
	if table.Count() != 0 {
		t.Errorf("expected empty table, got count %d", table.Count())
	}
}

func TestGlobalTable(t *testing.T) {
	// Save and restore global state
	original := GlobalTable
	defer func() {
		GlobalTable = original
	}()

	GlobalTable = NewAvpTable()
	GlobalTable.AddString("global", "test")

	avps := GlobalTable.GetByName("global")
	if len(avps) != 1 {
		t.Errorf("expected 1 global AVP, got %d", len(avps))
	}
}

func TestStringValue_String(t *testing.T) {
	sv := &StringValue{Val: str.Mk("test")}
	if sv.String() != "test" {
		t.Errorf("expected 'test', got %s", sv.String())
	}
}

func TestIntValue_String(t *testing.T) {
	iv := &IntValue{Val: 42}
	if iv.String() != "42" {
		t.Errorf("expected '42', got %s", iv.String())
	}
}

func TestFloatValue_String(t *testing.T) {
	fv := &FloatValue{Val: 1.5}
	if fv.String() != "1.500000" {
		t.Errorf("expected '1.500000', got %s", fv.String())
	}
}

func TestAvpTable_AddByID(t *testing.T) {
	table := NewAvpTable()
	avp := &AVP{
		Name:  str.Mk("id-test"),
		Value: &IntValue{Val: 99},
		Type:  AVPTypeInt,
		ID:    12345,
	}
	table.Add(avp)

	avps := table.GetByID(12345)
	if len(avps) != 1 {
		t.Errorf("expected 1 AVP by ID, got %d", len(avps))
	}
}
