// Package prelude provides some common utilities used among packages of Dachs compiler.
package prelude

import (
	"fmt"
)

// Pos represents some point in a source code.
type Pos struct {
	// Offset from the beginning of code
	Offset int
	// Line number
	Line int
	// Column number
	Column int
	// File of this position.
	File *Source
}

func (p Pos) String() string {
	return fmt.Sprintf("%s:%d:%d", p.File.Name, p.Line, p.Column)
}
