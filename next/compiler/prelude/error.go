package prelude

import (
	"bytes"
	"fmt"
	"strings"
)

// Error represents a compilation error with positional information and stacked messages.
type Error struct {
	Start    Position
	End      Position
	Messages []string
}

func (err *Error) Error() string {
	var buf bytes.Buffer
	s := err.Start

	buf.WriteString(fmt.Sprintf("Error at %s:%d:%d", s.File.Name, s.Line, s.Column))
	for _, msg := range err.Messages {
		buf.WriteString("\n  ")
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

// NewError makes a new complation error with the range.
func NewError(start, end Position, msg string) *Error {
	return &Error{start, end, []string{msg}}
}

// NewErrorAt makes a new complation error with the position.
func NewErrorAt(pos Position, msg string) *Error {
	return NewError(pos, Position{}, msg)
}

// NewErrorf makes a new complation error with the range and formatted message.
func NewErrorf(start, end Position, format string, args ...interface{}) *Error {
	return NewError(start, end, fmt.Sprintf(format, args...))
}

// NewErrorfAt makes a new complation error with the position and formatted message.
func NewErrorfAt(pos Position, format string, args ...interface{}) *Error {
	return NewError(pos, Position{}, fmt.Sprintf(format, args...))
}

// WithRange adds range information to the passed error.
func WithRange(start, end Position, err error) *Error {
	return NewError(start, end, err.Error())
}

// WithPos adds positional information to the passed error.
func WithPos(pos Position, err error) *Error {
	return NewErrorAt(pos, err.Error())
}

// Wrap adds range information and stack additional message to the original error.
func Wrap(start, end Position, err error, msg string) *Error {
	return &Error{start, end, []string{err.Error(), msg}}
}

// WrapAt adds positional information and stack additional message to the original error.
func WrapAt(pos Position, err error, msg string) *Error {
	return Wrap(pos, Position{}, err, msg)
}

// Wrapf adds range information and stack additional formatted message to the original error.
func Wrapf(start, end Position, err error, format string, args ...interface{}) *Error {
	return Wrap(start, end, err, fmt.Sprintf(format, args...))
}

// WrapfAt adds positional information and stack additional formatted message to the original error.
func WrapfAt(pos Position, err error, format string, args ...interface{}) *Error {
	return Wrap(pos, Position{}, err, fmt.Sprintf(format, args...))
}
