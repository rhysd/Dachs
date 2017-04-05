package prelude

import (
	"io/ioutil"
	"os"
)

type Source struct {
	Name   string
	Code   []byte
	IsFile bool
}

func NewSourceFromFile(file string) (*Source, error) {
	b, err := ioutil.ReadFile(file)
	if err != nil {
		return nil, err
	}
	return &Source{file, b, true}, nil
}

func NewSourceFromStdin() (*Source, error) {
	b, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		return nil, err
	}
	return &Source{"<stdin>", b, false}, nil
}

func NewDummySource(code string) *Source {
	return &Source{"dummy", []byte(code), false}
}
