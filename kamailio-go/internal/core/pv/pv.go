// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Pseudo Variables (PV) system - matching C pvapi.c / pvar.c
 *
 * PVs are dynamic variables accessed via $name notation in the
 * Kamailio routing script. They provide access to SIP message fields,
 * transaction state, dialog properties, and more.
 *
 * This module implements:
 *   - PV type definitions and registration
 *   - PV value resolution from SIP messages
 *   - PV value setting (for writable PVs)
 *   - Transformation operations ({s.substr}, {s.len}, etc.)
 */

package pv

import (
	"crypto/md5"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"sync"
)

// PVType represents the type of a pseudo variable.
type PVType int

const (
	// PVTypeReadOnly is a read-only PV.
	PVTypeReadOnly PVType = iota
	// PVTypeReadWrite is a writable PV.
	PVTypeReadWrite
	// PVTypeScript is a script variable ($var(name)).
	PVTypeScript
	// PVTypeAVP is an Attribute-Value Pair ($avp(name)).
	PVTypeAVP
	// PVTypeHeader is a header variable ($hdr(name)).
	PVTypeHeader
)

// PVClass represents the PV class (prefix).
type PVClass int

const (
	PVClassNone PVClass = iota
	PVClassMsg    // message-related: $ru, $rd, $ci, $rm, $rs
	PVClassFrom   // From header: $fU, $fD, $fP, $fn, $ft
	PVClassTo     // To header: $tU, $tD, $tP, $tn, $tt
	PVClassCSeq   // CSeq: $cs, $cseq
	PVClassCallID // Call-ID: $ci
	PVClassVia    // Via: $vi, $vs
	PVClassMsgBody // Message body: $mb
	PVClassRURI    // Request URI: $ru, $rd, $rp, $rU, $rP
	PVClassDU      // Destination URI: $du, $dd, $dp
	PVClassVar     // Script variable: $var(name)
	PVClassAVP     // AVP: $avp(name)
	PVClassDlg     // Dialog: $dlg(...)
	PVClassBranch  // Branch: $br
	PVClassTimer   // TM timer: $T_branch_idx, $T_reply_code
)

// PVSpec represents a parsed PV specification.
type PVSpec struct {
	Name  string
	Class PVClass
	Type  PVType
	Index int    // array index (for indexed PVs)
	Key   string // key string (for keyed PVs like $var(name))
}

// PVValue represents a PV value (string + integer).
type PVValue struct {
	Str string
	Int int
	OK  bool // true if the value was resolved
}

// PVGetter is a function that retrieves a PV value from a message context.
type PVGetter func(ctx interface{}) PVValue

// PVSetter is a function that sets a PV value.
type PVSetter func(ctx interface{}, val PVValue) error

// PVEntry represents a registered PV.
type PVEntry struct {
	Spec PVSpec
	Get  PVGetter
	Set  PVSetter // nil for read-only PVs
}

// Registry manages all registered pseudo variables.
type Registry struct {
	mu      sync.RWMutex
	entries map[string]*PVEntry
}

// Global registry instance.
var globalRegistry = &Registry{
	entries: make(map[string]*PVEntry),
}

// GlobalRegistry returns the global PV registry.
func GlobalRegistry() *Registry {
	return globalRegistry
}

// Register registers a new PV with the global registry.
func Register(name string, pvClass PVClass, pvType PVType, getter PVGetter, setter PVSetter) error {
	return globalRegistry.Register(name, pvClass, pvType, getter, setter)
}

// Register adds a PV to the registry.
func (r *Registry) Register(name string, pvClass PVClass, pvType PVType, getter PVGetter, setter PVSetter) error {
	r.mu.Lock()
	defer r.mu.Unlock()

	if _, exists := r.entries[name]; exists {
		return fmt.Errorf("PV %q already registered", name)
	}

	r.entries[name] = &PVEntry{
		Spec: PVSpec{
			Name:  name,
			Class: pvClass,
			Type:  pvType,
		},
		Get: getter,
		Set: setter,
	}
	return nil
}

// Lookup finds a PV by name.
func (r *Registry) Lookup(name string) *PVEntry {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.entries[name]
}

// Get resolves a PV value by name from the given context.
func (r *Registry) Get(name string, ctx interface{}) PVValue {
	entry := r.Lookup(name)
	if entry == nil || entry.Get == nil {
		return PVValue{OK: false}
	}
	return entry.Get(ctx)
}

// Set sets a PV value by name.
func (r *Registry) Set(name string, ctx interface{}, val PVValue) error {
	entry := r.Lookup(name)
	if entry == nil || entry.Set == nil {
		return fmt.Errorf("PV %q not found or read-only", name)
	}
	return entry.Set(ctx, val)
}

// AllNames returns all registered PV names.
func (r *Registry) AllNames() []string {
	r.mu.RLock()
	defer r.mu.RUnlock()
	names := make([]string, 0, len(r.entries))
	for name := range r.entries {
		names = append(names, name)
	}
	return names
}

// ParsePVName parses a PV name string (e.g., "ru", "var(x)", "avp(name)").
// Returns the base name and optional key/index.
func ParsePVName(raw string) (name string, key string) {
	raw = strings.TrimSpace(raw)
	if idx := strings.Index(raw, "("); idx >= 0 {
		name = raw[:idx]
		if endIdx := strings.Index(raw[idx:], ")"); endIdx >= 0 {
			key = raw[idx+1 : idx+endIdx]
		}
	} else {
		name = raw
	}
	return name, key
}

// ApplyTransformation applies a transformation to a PV value.
// Supports: {s.len}, {s.int}, {s.substr.start.end}, {s.md5}, {s.sha256},
// {s.trim}, {s.tolower}, {s.toupper}, {s.encode.hexa}, {s.decode.hexa}
func ApplyTransformation(value string, transform string) (string, error) {
	transform = strings.TrimSpace(transform)
	if !strings.HasPrefix(transform, "s.") {
		return "", fmt.Errorf("unsupported transformation: %s", transform)
	}

	op := strings.TrimPrefix(transform, "s.")

	switch {
	case op == "len":
		return strconv.Itoa(len(value)), nil

	case op == "int":
		n, err := strconv.Atoi(value)
		if err != nil {
			return "0", nil
		}
		return strconv.Itoa(n), nil

	case op == "trim":
		return strings.TrimSpace(value), nil

	case op == "tolower":
		return strings.ToLower(value), nil

	case op == "toupper":
		return strings.ToUpper(value), nil

	case strings.HasPrefix(op, "substr."):
		// s.substr.start.end or s.substr.start.len
		parts := strings.SplitN(op[7:], ".", 2)
		if len(parts) != 2 {
			return "", fmt.Errorf("invalid substr: %s", op)
		}
		start, err := strconv.Atoi(parts[0])
		if err != nil {
			return "", fmt.Errorf("invalid substr start: %s", parts[0])
		}
		end, err := strconv.Atoi(parts[1])
		if err != nil {
			return "", fmt.Errorf("invalid substr end: %s", parts[1])
		}
		if start < 0 {
			start = 0
		}
		if end > len(value) {
			end = len(value)
		}
		if start >= end {
			return "", nil
		}
		return value[start:end], nil

	case op == "md5":
		return fmt.Sprintf("%x", md5Sum(value)), nil

	case op == "sha256":
		return fmt.Sprintf("%x", sha256Sum(value)), nil

	case op == "encode.hexa":
		return fmt.Sprintf("%x", []byte(value)), nil

	case op == "decode.hexa":
		decoded, err := hexDecode(value)
		if err != nil {
			return "", err
		}
		return string(decoded), nil

	default:
		return "", fmt.Errorf("unsupported transformation: s.%s", op)
	}
}

// ParseTransformations extracts transformation operations from a PV expression.
// e.g., "$ru{s.tolower}" -> ("ru", ["s.tolower"])
func ParseTransformations(expr string) (pvName string, transforms []string) {
	expr = strings.TrimSpace(expr)
	if !strings.HasPrefix(expr, "$") {
		return expr, nil
	}
	expr = expr[1:]

	// Extract transformations in {}
	re := regexp.MustCompile(`\{([^}]*)\}`)
	matches := re.FindAllStringSubmatch(expr, -1)
	for _, m := range matches {
		transforms = append(transforms, m[1])
	}

	// Remove transformations from PV name
	pvName = re.ReplaceAllString(expr, "")
	pvName = strings.TrimSpace(pvName)

	return pvName, transforms
}

// Resolve resolves a full PV expression with transformations.
// e.g., "$ru{s.tolower}{s.substr.0.3}"
func Resolve(expr string, ctx interface{}) (string, error) {
	pvName, transforms := ParseTransformations(expr)

	// Get base PV value
	baseName, key := ParsePVName(pvName)
	lookupName := baseName
	if key != "" {
		lookupName = baseName + "(" + key + ")"
	}

	val := GlobalRegistry().Get(lookupName, ctx)
	if !val.OK {
		return "", fmt.Errorf("PV %q not resolved", lookupName)
	}

	result := val.Str

	// Apply transformations
	for _, tf := range transforms {
		transformed, err := ApplyTransformation(result, tf)
		if err != nil {
			return "", err
		}
		result = transformed
	}

	return result, nil
}

// --- Hash and encoding helpers ---

// md5Sum computes the MD5 hash of the given string.
func md5Sum(data string) [md5.Size]byte {
	return md5.Sum([]byte(data))
}

// sha256Sum computes the SHA-256 hash of the given string.
func sha256Sum(data string) [sha256.Size]byte {
	return sha256.Sum256([]byte(data))
}

// hexDecode decodes a hex-encoded string.
func hexDecode(s string) ([]byte, error) {
	return hex.DecodeString(s)
}
