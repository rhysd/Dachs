package syntax

import (
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"testing"
)

func TestTokenString(t *testing.T) {
	s := prelude.NewDummySource("a bc d")
	tok := &Token{
		Kind:  TokenIdent,
		Start: prelude.Pos{2, 1, 3, s},
		End:   prelude.Pos{4, 1, 5, s},
	}
	want := "<IDENT:bc>(1:3:2-1:5:4)"
	got := tok.String()
	if want != got {
		t.Fatalf("Expected '%s' but actually '%s'", want, got)
	}
}
