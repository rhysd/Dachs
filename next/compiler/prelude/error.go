package prelude

import (
	"bytes"
	"fmt"
	"strings"
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
	buf.WriteString("Error: ")
	buf.WriteString(err.Messages[0])
	buf.WriteString(" (at ")
	buf.WriteString(s.String())
	buf.WriteString(")")
	for _, msg := range err.Messages[1:] {
		buf.WriteString("\n  Note: ")
		buf.WriteString(msg)
	}

	if err.End.File == nil {
		return buf.String()
	}

	snip := string(s.File.Code[s.Offset:err.End.Offset])
	if snip == "" {
		return buf.String()
	}

	buf.WriteString("\n\n> ")
	buf.WriteString(strings.Replace(snip, "\n", "\n> ", -1))
	buf.WriteString("\n\n")

	// TODO:
	// If the code snippet for the token is too long, skip lines with '...' except for starting N lines
	// and ending N lines

	return buf.String()
}

// Wrap stacks the additional message upon current error.
func (err *Error) Wrap(msg string) *Error {
	err.Messages = append(err.Messages, msg)
	return err
}

// Wrapf stacks the additional formatted message upon current error.
func (err *Error) Wrapf(format string, args ...interface{}) *Error {
	err.Messages = append(err.Messages, fmt.Sprintf(format, args...))
	return err
}

// WrapAt stacks the additional message upon current error with position.
func (err *Error) WrapAt(pos Pos, msg string) *Error {
	err.Messages = append(err.Messages, fmt.Sprintf("%s (at %s)", msg, pos.String()))
	return err
}

// WrapfAt stacks the additional formatted message upon current error with poisition.
func (err *Error) WrapfAt(pos Pos, format string, args ...interface{}) *Error {
	return err.WrapAt(pos, fmt.Sprintf(format, args...))
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

// Wrap adds range information and stack additional message to the original error.
func Wrap(start, end Pos, err error, msg string) *Error {
	if err, ok := err.(*Error); ok {
		return err.WrapAt(start, msg)
	}
	return &Error{start, end, []string{err.Error(), msg}}
}

// WrapAt adds positional information and stack additional message to the original error.
func WrapAt(pos Pos, err error, msg string) *Error {
	return Wrap(pos, Pos{}, err, msg)
}

// Wrapf adds range information and stack additional formatted message to the original error.
func Wrapf(start, end Pos, err error, format string, args ...interface{}) *Error {
	return Wrap(start, end, err, fmt.Sprintf(format, args...))
}

// WrapfAt adds positional information and stack additional formatted message to the original error.
func WrapfAt(pos Pos, err error, format string, args ...interface{}) *Error {
	return Wrap(pos, Pos{}, err, fmt.Sprintf(format, args...))
}
