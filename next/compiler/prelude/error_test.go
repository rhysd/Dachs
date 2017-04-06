package prelude

import (
	"fmt"
	"testing"
)

func testMakeRange() (Pos, Pos) {
	s := NewDummySource(
		`package prelude

import (
	"testing"
)`,
	)

	start := Pos{4, 1, 4, s}
	end := Pos{20, 3, 3, s}
	return start, end
}

func testMakePos() Pos {
	p, _ := testMakeRange()
	return p
}

func TestNewError(t *testing.T) {
	want :=
		`Error at <dummy>:1:4
  This is error text!

> age prelude
> 
> imp

`

	s, e := testMakeRange()
	errs := []*Error{
		NewError(s, e, "This is error text!"),
		NewErrorf(s, e, "This %s error %s!", "is", "text"),
	}
	for _, err := range errs {
		got := err.Error()
		if got != want {
			t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
		}
	}
}

func TestNewErrorAt(t *testing.T) {
	want := "Error at <dummy>:1:4\n  This is error text!"
	for _, err := range []*Error{
		NewErrorAt(testMakePos(), "This is error text!"),
		NewErrorfAt(testMakePos(), "This is %s text!", "error"),
	} {
		got := err.Error()
		if got != want {
			t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
		}
	}
}

func TestWithRange(t *testing.T) {
	want :=
		`Error at <dummy>:1:4
  This is an error text!

> age prelude
> 
> imp

`

	s, e := testMakeRange()
	err := WithRange(s, e, fmt.Errorf("This is an error text!"))
	got := err.Error()
	if got != want {
		t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
	}
}

func TestWithPos(t *testing.T) {
	want := "Error at <dummy>:1:4\n  This is wrapped error text!"
	got := WithPos(testMakePos(), fmt.Errorf("This is wrapped error text!")).Error()
	if got != want {
		t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
	}
}

func TestWrap(t *testing.T) {
	want :=
		`Error at <dummy>:1:4
  This is original error text!
  This is additional error text!

> age prelude
> 
> imp

`

	s, e := testMakeRange()
	errs := []*Error{
		Wrap(s, e, fmt.Errorf("This is original error text!"), "This is additional error text!"),
		Wrapf(s, e, fmt.Errorf("This is original error text!"), "This is %s error text!", "additional"),
	}
	for _, err := range errs {
		got := err.Error()
		if got != want {
			t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
		}
	}
}

func TestWrapAt(t *testing.T) {
	want := "Error at <dummy>:1:4\n  This is original error text!\n  This is additional error text!"
	pos := testMakePos()
	original := fmt.Errorf("This is original error text!")
	for _, err := range []*Error{
		WrapAt(pos, original, "This is additional error text!"),
		WrapfAt(pos, original, "This is additional %s!", "error text"),
	} {
		got := err.Error()
		if got != want {
			t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
		}
	}
}

func TestWrapMethods(t *testing.T) {
	want :=
		`Error at <dummy>:1:4
  This is original error text!
  This is additional error text!

> age prelude
> 
> imp

`

	s, e := testMakeRange()
	errs := []*Error{
		NewError(s, e, "This is original error text!").Wrap("This is additional error text!"),
		NewError(s, e, "This is original error text!").Wrapf("This is %s!", "additional error text"),
	}
	for _, err := range errs {
		got := err.Error()
		if got != want {
			t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
		}
	}
}

func TestCodeIsEmpty(t *testing.T) {
	s := NewDummySource("")
	p := Pos{0, 1, 1, s}
	err := NewError(p, p, "This is error text!")
	want := "Error at <dummy>:1:1\n  This is error text!"
	got := err.Error()

	if want != got {
		t.Fatalf("Unexpected error message. want: '%s', got: '%s'", want, got)
	}
}
