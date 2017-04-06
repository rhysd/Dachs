// Package prelude provides some common utilities used among packages of Dachs compiler.
package prelude

// Position represents some point in a source code.
type Position struct {
	// Offset from the beggining of code
	Offset int
	// Line number
	Line int
	// Column number
	Column int
	// File of this position.
	File *Source
}
