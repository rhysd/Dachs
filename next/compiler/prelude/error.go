package prelude

import (
	"bytes"
	"fmt"
	"github.com/fatih/color"
	"runtime"
	"strings"
)

func init() {
	if runtime.GOOS == "windows" {
		// FIXME:
		// On Windows, ANSI sequences are not available on cmd.exe.
		// go-colorable cannot help to solve this because it only provides writers to stdout or
		// stderr.
		// At least I need to improve the check. Even if running on windows, WLS would be able to
		// handle ANSI sequences.
		color.NoColor = true
	}
}

var (
	gray = color.New(color.FgHiBlack).SprintfFunc()
	bold = color.New(color.Bold).SprintFunc()
)

// Error represents a compilation error with positional information and stacked messages.
type Error struct {
	Start    Pos
	End      Pos
	Messages []string
}

func (err *Error) Error() string {
	var buf bytes.Buffer
	s := err.Start

	// Error: {msg} (at {pos})
	//   {note1}
	//   {note2}
	//   ...
	buf.WriteString(color.RedString("Error: "))
	buf.WriteString(bold(err.Messages[0]))
	buf.WriteString(gray(" (at %s)", s.String()))
	for _, msg := range err.Messages[1:] {
		buf.WriteString(color.GreenString("\n  Note: "))
		buf.WriteString(msg)
	}

	if err.End.File == nil {
		return buf.String()
	}

	snip := string(s.File.Code[s.Offset:err.End.Offset])
	if snip == "" {
		return buf.String()
	}

	// TODO:
	// Compensate for indentation at the first line of code snippet.
	buf.WriteString("\n\n> ")
	buf.WriteString(strings.Replace(snip, "\n", "\n> ", -1))
	buf.WriteString("\n\n")

	// TODO:
	// If the code snippet for the token is too long, skip lines with '...' except for starting N lines
	// and ending N lines

	return buf.String()
}

// Note stacks the additional message upon current error.
func (err *Error) Note(msg string) *Error {
	err.Messages = append(err.Messages, msg)
	return err
}

// Notef stacks the additional formatted message upon current error.
func (err *Error) Notef(format string, args ...interface{}) *Error {
	err.Messages = append(err.Messages, fmt.Sprintf(format, args...))
	return err
}

// NoteAt stacks the additional message upon current error with position.
func (err *Error) NoteAt(pos Pos, msg string) *Error {
	at := gray("(at %s)", pos.String())
	err.Messages = append(err.Messages, fmt.Sprintf("%s %s", msg, at))
	return err
}

// NotefAt stacks the additional formatted message upon current error with poisition.
func (err *Error) NotefAt(pos Pos, format string, args ...interface{}) *Error {
	return err.NoteAt(pos, fmt.Sprintf(format, args...))
}

// NewError makes a new compilation error with the range.
func NewError(start, end Pos, msg string) *Error {
	return &Error{start, end, []string{msg}}
}

// NewErrorAt makes a new compilation error with the position.
func NewErrorAt(pos Pos, msg string) *Error {
	return NewError(pos, Pos{}, msg)
}

// NewErrorf makes a new compilation error with the range and formatted message.
func NewErrorf(start, end Pos, format string, args ...interface{}) *Error {
	return NewError(start, end, fmt.Sprintf(format, args...))
}

// NewErrorfAt makes a new compilation error with the position and formatted message.
func NewErrorfAt(pos Pos, format string, args ...interface{}) *Error {
	return NewError(pos, Pos{}, fmt.Sprintf(format, args...))
}

// WithRange adds range information to the passed error.
func WithRange(start, end Pos, err error) *Error {
	return NewError(start, end, err.Error())
}

// WithPos adds positional information to the passed error.
func WithPos(pos Pos, err error) *Error {
	return NewErrorAt(pos, err.Error())
}

// Note adds range information and stack additional message to the original error.
func Note(start, end Pos, err error, msg string) *Error {
	if err, ok := err.(*Error); ok {
		return err.NoteAt(start, msg)
	}
	return &Error{start, end, []string{err.Error(), msg}}
}

// NoteAt adds positional information and stack additional message to the original error.
func NoteAt(pos Pos, err error, msg string) *Error {
	return Note(pos, Pos{}, err, msg)
}

// Notef adds range information and stack additional formatted message to the original error.
func Notef(start, end Pos, err error, format string, args ...interface{}) *Error {
	return Note(start, end, err, fmt.Sprintf(format, args...))
}

// NotefAt adds positional information and stack additional formatted message to the original error.
func NotefAt(pos Pos, err error, format string, args ...interface{}) *Error {
	return Note(pos, Pos{}, err, fmt.Sprintf(format, args...))
}
