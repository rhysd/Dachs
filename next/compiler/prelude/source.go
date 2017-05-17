package prelude

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
)

// Source represents Dachs source code file. It may be a file on filesystem, stdin or dummy file.
type Source struct {
	// Name of the file. <stdin> if it is stdin. <dummy> if it is a dummy source.
	Name string
	// Code contained in this source.
	Code []byte
	// Exists indicates this source exists in filesystem or not.
	Exists bool
}

// NewSourceFromFile make *Source object from file path.
func NewSourceFromFile(file string) (*Source, error) {
	path, err := filepath.Abs(file)
	if err != nil {
		return nil, err
	}
	b, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, err
	}
	return &Source{path, b, true}, nil
}

// NewSourceFromStdin make *Source object from stdin. User will need to input source code into stdin.
func NewSourceFromStdin() (*Source, error) {
	b, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		return nil, err
	}
	return &Source{"<stdin>", b, false}, nil
}

// NewDummySource make *Source with passed code. The source is actually does not exist in filesystem (so dummy). This is used for tests.
func NewDummySource(code string) *Source {
	return &Source{"<dummy>", []byte(code), false}
}

// BaseName makes a base name from the name of source. If the source does not exist in filesystem, its base name will be 'out'.
func (src *Source) BaseName() string {
	if !src.Exists {
		return "out"
	}
	b := filepath.Base(src.Name)
	return strings.TrimSuffix(b, filepath.Ext(b))
}
