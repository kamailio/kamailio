// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Kamailio-Go
 *
 * Core logging package - structured logging with zap
 */

package log

import (
	"time"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
)

// String creates a string zap.Field
func String(key, val string) zap.Field {
	return zap.String(key, val)
}

// Int creates an int zap.Field
func Int(key string, val int) zap.Field {
	return zap.Int(key, val)
}

// Uint16 creates a uint16 zap.Field
func Uint16(key string, val uint16) zap.Field {
	return zap.Uint16(key, val)
}

// Time creates a time zap.Field
func Time(key string, val time.Time) zap.Field {
	return zap.Time(key, val)
}

// Bool creates a bool zap.Field
func Bool(key string, val bool) zap.Field {
	return zap.Bool(key, val)
}

var (
	// global logger instance
	logger *zap.Logger
)

// Config holds logging configuration
type Config struct {
	Level      string // debug, info, warn, error
	Encoding   string // json, console
	EncoderConfig zapcore.EncoderConfig
}

// Init initializes the global logger with the given configuration
func Init(cfg *Config) error {
	var level zapcore.Level
	if err := level.UnmarshalText([]byte(cfg.Level)); err != nil {
		level = zapcore.InfoLevel
	}

	if cfg.EncoderConfig.EncodeLevel == nil {
		cfg.EncoderConfig.EncodeLevel = zapcore.LowercaseLevelEncoder
	}
	if cfg.EncoderConfig.EncodeTime == nil {
		cfg.EncoderConfig.EncodeTime = zapcore.ISO8601TimeEncoder
	}
	if cfg.EncoderConfig.EncodeDuration == nil {
		cfg.EncoderConfig.EncodeDuration = zapcore.StringDurationEncoder
	}

	encoderConfig := cfg.EncoderConfig

	var encoder zapcore.Encoder
	if cfg.Encoding == "console" {
		encoder = zapcore.NewConsoleEncoder(encoderConfig)
	} else {
		encoder = zapcore.NewJSONEncoder(encoderConfig)
	}

	core := zapcore.NewCore(
		encoder,
		zapcore.AddSync(zapcore.AddSync(&zapWriteSyncer{})),
		level,
	)

	logger = zap.New(core, zap.AddCaller(), zap.AddCallerSkip(1))
	return nil
}

// InitDefault initializes with default production settings
func InitDefault() error {
	cfg := &Config{
		Level:    "info",
		Encoding: "json",
	}
	return Init(cfg)
}

// Get returns the global logger
func Get() *zap.Logger {
	if logger == nil {
		// fallback to nop logger if not initialized
		return zap.NewNop()
	}
	return logger
}

// Debug logs a message at DebugLevel
func Debug(msg string, fields ...zap.Field) {
	Get().Debug(msg, fields...)
}

// Info logs a message at InfoLevel
func Info(msg string, fields ...zap.Field) {
	Get().Info(msg, fields...)
}

// Warn logs a message at WarnLevel
func Warn(msg string, fields ...zap.Field) {
	Get().Warn(msg, fields...)
}

// Error logs a message at ErrorLevel
func Error(msg string, fields ...zap.Field) {
	Get().Error(msg, fields...)
}

// ErrField creates an error zap.Field
func ErrField(err error) zap.Field {
	return zap.Error(err)
}

// Fatal logs a message at FatalLevel and then calls os.Exit(1)
func Fatal(msg string, fields ...zap.Field) {
	Get().Fatal(msg, fields...)
}

// Sync flushes any buffered log entries
func Sync() {
	if logger != nil {
		logger.Sync()
	}
}

// zapWriteSyncer implements zap.Syncer for standard WriteSyncer interface
type zapWriteSyncer struct{}

func (z *zapWriteSyncer) Write(p []byte) (n int, err error) {
	// Use standard library for output
	// In production, this would be replaced with actual file/stdout writer
	return len(p), nil
}

func (z *zapWriteSyncer) Sync() error {
	return nil
}
