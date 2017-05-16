package prelude

import (
	"fmt"
	"io"
	"os"
	"runtime"
	"sync"
)

// LoggerTag represents components you want to see the log for.
type LoggerTag int

const (
	// None represents nothing to log
	None LoggerTag = 0
	// All represents logging all messages
	All = ^0
	// Parsing tag logs messages for lexer and parser
	Parsing = 1 << iota
	// Sema tag logs messages for semantics checks
	Sema
)

var tagTable = [...]string{
	Parsing: "Parsing",
	Sema:    "Sema",
}

type logger struct {
	enabledTags LoggerTag
	tag         LoggerTag
	out         io.Writer
	mu          sync.Mutex
}

var globalLogger = logger{
	enabledTags: 0,
	tag:         0,
	out:         os.Stderr,
}

// EnableLog enables log for flags defined by tag. Tags can be combined with '|'.
func EnableLog(tag LoggerTag) {
	globalLogger.mu.Lock()
	globalLogger.enabledTags |= tag
	globalLogger.mu.Unlock()
}

// IsLogEnabled returns whether given tag is enabled or not.j
func IsLogEnabled(tag LoggerTag) bool {
	return globalLogger.enabledTags&tag != 0
}

// DisableLog stops logging for flags defined by tag. Tags can be combined with '|'.
func DisableLog(tag LoggerTag) {
	globalLogger.mu.Lock()
	globalLogger.enabledTags &^= tag
	globalLogger.mu.Unlock()
}

// NowLogging declares what is now logging with tag.
func NowLogging(tag LoggerTag) {
	globalLogger.mu.Lock()
	globalLogger.tag = tag
	globalLogger.mu.Unlock()
}

// SetLogWriter sets a target to write log. Default is os.Stderr
func SetLogWriter(w io.Writer) {
	globalLogger.mu.Lock()
	globalLogger.out = w
	globalLogger.mu.Unlock()
}

func output(text string, calldepth int) {
	globalLogger.mu.Lock()
	defer globalLogger.mu.Unlock()
	if globalLogger.tag&globalLogger.enabledTags == 0 {
		return
	}

	// Calling runtime.Caller is heavy
	globalLogger.mu.Unlock()

	_, file, line, ok := runtime.Caller(calldepth)
	if !ok {
		file = "???"
		line = 0
	} else {
		for i := len(file) - 1; i > 0; i-- {
			if file[i] == '/' {
				file = file[i+1:]
				break
			}
		}
	}

	globalLogger.mu.Lock()

	header := fmt.Sprintf("[%s] %s:%d: ", tagTable[globalLogger.tag], file, line)
	io.WriteString(globalLogger.out, header+text)
}

// Log prints one line log. All arguments will be printed by being separated with whitespace
func Log(args ...interface{}) {
	output(fmt.Sprintln(args...), 2)
}

// Logf prints one line formatted log. New line will be added at the end automatically.
func Logf(format string, args ...interface{}) {
	output(fmt.Sprintf(format, args...)+"\n", 2)
}
