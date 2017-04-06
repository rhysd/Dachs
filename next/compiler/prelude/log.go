package prelude

import (
	"fmt"
	"io"
	"os"
	"runtime"
)

// LoggerTag represents components you want to see the log for.
type LoggerTag int

const (
	// None represents nothing to log
	None LoggerTag = 0
	// All represents logging all messages
	All = ^0
	// Lexing tag logs messages for lexer
	Lexing = 1 << iota
	// Parsing tag logs messages for parser
	Parsing
)

var tagTable = [...]string{
	Lexing:  "Lexing",
	Parsing: "Parsing",
}

type logger struct {
	enabledTags LoggerTag
	tag         LoggerTag
	out         io.Writer
}

var globalLogger = logger{0, 0, os.Stderr}

// EnableLog enables log for flags defined by tag. Tags can be combined with '|'.
func EnableLog(tag LoggerTag) {
	globalLogger.enabledTags |= tag
}

// DisableLog stops logging for flags defined by tag. Tags can be combined with '|'.
func DisableLog(tag LoggerTag) {
	globalLogger.enabledTags &^= tag
}

// NowLogging declares what is now logging with tag.
func NowLogging(tag LoggerTag) {
	globalLogger.tag = tag
}

// SetLogWriter sets a target to write log. Default is os.Stderr
func SetLogWriter(w io.Writer) {
	// XXX: Need to protect the assignment with mutex for thread safety
	globalLogger.out = w
}

func output(text string, calldepth int) {
	if globalLogger.tag&globalLogger.enabledTags == 0 {
		return
	}

	// XXX: Need to protect runtime.Caller() with mutex for thread safety
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
