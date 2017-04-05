package prelude

import (
	"fmt"
	"io"
	"os"
	"runtime"
)

type LoggerTag int

const (
	None   LoggerTag = 0
	Lexing           = 1 << iota
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

func EnableLog(tag LoggerTag) {
	globalLogger.enabledTags |= tag
}

func EnableLogAll() {
	globalLogger.enabledTags = ^0
}

func DisableLogAll() {
	globalLogger.enabledTags = 0
}

func DisableLog(tag LoggerTag) {
	globalLogger.enabledTags &^= tag
}

func NowLogging(tag LoggerTag) {
	globalLogger.tag = tag
}

func SetLogWriter(w io.Writer) {
	// XXX: Need to protect runtime.Caller() with mutex for thread safety
	globalLogger.out = w
}

func Log(args ...interface{}) {
	if globalLogger.tag&globalLogger.enabledTags == 0 {
		return
	}

	// XXX: Need to protect runtime.Caller() with mutex for thread safety
	_, file, line, ok := runtime.Caller(1)
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
	io.WriteString(globalLogger.out, header+fmt.Sprintln(args...))
}
