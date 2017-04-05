package prelude

import (
	"bytes"
	"fmt"
	"strings"
)

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

func (err *Error) Wrap(msg string) *Error {
	err.Messages = append(err.Messages, msg)
	return err
}

func (err *Error) Wrapf(format string, args ...interface{}) *Error {
	err.Messages = append(err.Messages, fmt.Sprintf(format, args...))
	return err
}

func NewError(start, end Position, msg string) *Error {
	return &Error{start, end, []string{msg}}
}

func NewErrorAt(pos Position, msg string) *Error {
	return NewError(pos, Position{}, msg)
}

func NewErrorf(start, end Position, format string, args ...interface{}) *Error {
	return NewError(start, end, fmt.Sprintf(format, args...))
}

func NewErrorfAt(pos Position, format string, args ...interface{}) *Error {
	return NewError(pos, Position{}, fmt.Sprintf(format, args...))
}

func WithRange(start, end Position, err error) *Error {
	return NewError(start, end, err.Error())
}

func WithPos(pos Position, err error) *Error {
	return NewErrorAt(pos, err.Error())
}

func Wrap(start, end Position, err error, msg string) *Error {
	return &Error{start, end, []string{err.Error(), msg}}
}

func WrapAt(pos Position, err error, msg string) *Error {
	return Wrap(pos, Position{}, err, msg)
}

func Wrapf(start, end Position, err error, format string, args ...interface{}) *Error {
	return Wrap(start, end, err, fmt.Sprintf(format, args...))
}

func WrapfAt(pos Position, err error, format string, args ...interface{}) *Error {
	return Wrap(pos, Position{}, err, fmt.Sprintf(format, args...))
}
