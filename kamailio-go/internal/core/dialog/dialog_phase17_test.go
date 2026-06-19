// SPDX-License-Identifier: GPL-2.0-or-later
package dialog

import (
	"testing"
	"time"
)

func TestProfile_AddAndCount(t *testing.T) {
	p := NewProfile("test", ProfileTypeCount)
	p.Add("dlg-1", 0)
	p.Add("dlg-2", 0)
	if p.Count() != 2 {
		t.Errorf("expected 2, got %d", p.Count())
	}
}

func TestProfile_RemoveAndCount(t *testing.T) {
	p := NewProfile("test", ProfileTypeCount)
	p.Add("dlg-1", 0)
	p.Add("dlg-2", 0)
	p.Remove("dlg-1")
	if p.Count() != 1 {
		t.Errorf("expected 1, got %d", p.Count())
	}
}

func TestProfile_Value(t *testing.T) {
	p := NewProfile("test", ProfileTypeValue)
	p.Add("dlg-1", 100)
	p.Add("dlg-2", 200)
	if p.Value() != 300 {
		t.Errorf("expected 300, got %d", p.Value())
	}
	p.Remove("dlg-1")
	if p.Value() != 200 {
		t.Errorf("expected 200 after remove, got %d", p.Value())
	}
}

func TestProfile_SetValue(t *testing.T) {
	p := NewProfile("test", ProfileTypeValue)
	p.Add("dlg-1", 100)
	p.SetValue("dlg-1", 500)
	if p.Value() != 500 {
		t.Errorf("expected 500, got %d", p.Value())
	}
}

func TestProfileManager_DefineAndGet(t *testing.T) {
	pm := NewProfileManager()
	_, err := pm.DefineProfile("concurrent", ProfileTypeCount)
	if err != nil {
		t.Fatalf("DefineProfile: %v", err)
	}
	p := pm.GetProfile("concurrent")
	if p == nil {
		t.Error("expected profile")
	}
	if p.Type != ProfileTypeCount {
		t.Error("expected count type")
	}
}

func TestProfileManager_Duplicate(t *testing.T) {
	pm := NewProfileManager()
	pm.DefineProfile("test", ProfileTypeCount)
	_, err := pm.DefineProfile("test", ProfileTypeCount)
	if err == nil {
		t.Error("expected error for duplicate profile")
	}
}

func TestProfileManager_AddToProfile(t *testing.T) {
	pm := NewProfileManager()
	pm.DefineProfile("calls", ProfileTypeCount)
	pm.AddToProfile("calls", "dlg-1", 0)
	pm.AddToProfile("calls", "dlg-2", 0)
	count, _ := pm.ProfileCount("calls")
	if count != 2 {
		t.Errorf("expected 2, got %d", count)
	}
}

func TestCallbackManager_RegisterAndFire(t *testing.T) {
	cm := NewCallbackManager()

	var fired bool
	var receivedType DialogEventType

	cm.Register(DialogCreated, func(event *DialogEvent) {
		fired = true
		receivedType = event.Type
	})

	cm.FireCreated(nil)
	if !fired {
		t.Error("expected callback to fire")
	}
	if receivedType != DialogCreated {
		t.Error("expected DialogCreated event type")
	}
}

func TestCallbackManager_MultipleCallbacks(t *testing.T) {
	cm := NewCallbackManager()

	var count int
	cm.Register(DialogTerminated, func(event *DialogEvent) {
		count++
	})
	cm.Register(DialogTerminated, func(event *DialogEvent) {
		count++
	})

	cm.FireTerminated(nil)
	if count != 2 {
		t.Errorf("expected 2 callbacks, got %d", count)
	}
}

func TestCallbackManager_Unregister(t *testing.T) {
	cm := NewCallbackManager()
	var fired bool
	cm.Register(DialogExpired, func(event *DialogEvent) {
		fired = true
	})
	cm.Unregister(DialogExpired)
	cm.FireExpired(nil)
	if fired {
		t.Error("expected callback not to fire after unregister")
	}
}

func TestTimerManager_StartAndStop(t *testing.T) {
	tm := NewTimerManager(nil, nil)
	tm.StartLifetimeTimer("dlg-1", 5*time.Second)
	if !tm.HasTimer("dlg-1") {
		t.Error("expected timer to be active")
	}
	tm.StopTimer("dlg-1")
	if tm.HasTimer("dlg-1") {
		t.Error("expected timer to be stopped")
	}
}

func TestTimerManager_Expire(t *testing.T) {
	var expiredID string
	tm := NewTimerManager(nil, func(id string) {
		expiredID = id
	})
	tm.StartLifetimeTimer("dlg-expire", 50*time.Millisecond)
	time.Sleep(100 * time.Millisecond)
	if expiredID != "dlg-expire" {
		t.Errorf("expected dlg-expire, got %q", expiredID)
	}
}

func TestTimerManager_Refresh(t *testing.T) {
	var expiredID string
	tm := NewTimerManager(nil, func(id string) {
		expiredID = id
	})
	tm.StartLifetimeTimer("dlg-refresh", 100*time.Millisecond)
	time.Sleep(30 * time.Millisecond)
	tm.RefreshTimer("dlg-refresh")
	time.Sleep(50 * time.Millisecond)
	if expiredID != "" {
		t.Error("expected timer not to expire after refresh")
	}
}

func TestTimerConfig_Default(t *testing.T) {
	cfg := DefaultTimerConfig()
	if cfg.DefaultLifetime != 7200*time.Second {
		t.Errorf("DefaultLifetime = %v, want 7200s", cfg.DefaultLifetime)
	}
	if cfg.InactiveLifetime != 180*time.Second {
		t.Errorf("InactiveLifetime = %v, want 180s", cfg.InactiveLifetime)
	}
}
