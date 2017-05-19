// Package prelude provides some common utilities used among packages of Dachs compiler.
package prelude

import (
	"fmt"
	"os"
	"path/filepath"
)

var currentDir string

func init() {
	currentDir, _ = filepath.Abs(filepath.Dir(os.Args[0]))
}

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
	f := p.File.Name
	if p.File.Exists && currentDir != "" && filepath.HasPrefix(f, currentDir) {
		f, _ = filepath.Rel(currentDir, f)
	}
	return fmt.Sprintf("%s:%d:%d", f, p.Line, p.Column)
}
