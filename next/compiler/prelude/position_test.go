package prelude

import (
	"testing"
)

func TestStringizePos(t *testing.T) {
	src := NewDummySource("test")
	p := Pos{4, 1, 3, src}
	want := "<dummy>:1:3"
	if p.String() != want {
		t.Fatal("Unknown position format: ", p.String(), "wanted", want)
	}
}
