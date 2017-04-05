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
)

var tagTable = [...]string{
	Lexing: "Lexing",
}

type logger struct {
	EnabledTags LoggerTag
	Tag         LoggerTag
	Out         io.Writer
}

var globalLogger = logger{0, 0, os.Stderr}

func EnableLog(tag LoggerTag) {
	globalLogger.EnabledTags |= tag
}

func EnableLogAll() {
	globalLogger.EnabledTags = ^0
}

func DisableLog(tag LoggerTag) {
	globalLogger.EnabledTags &^= tag
}

func NowLogging(tag LoggerTag) {
	globalLogger.Tag = tag
}

func Log(args ...interface{}) {
	if globalLogger.Tag&globalLogger.EnabledTags == 0 {
		return
	}

	// XXX: Need to protect runtime.Caller() with mutex for thread safety
	_, file, line, ok := runtime.Caller(2)
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

	header := fmt.Sprintf("[%s]%s:%d: ", tagTable[globalLogger.Tag], file, line)
	io.WriteString(globalLogger.Out, header+fmt.Sprintln(args...))
}
