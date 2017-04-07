package syntax

import (
	"bufio"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/rhysd/Dachs/next/compiler/prelude"
)

func testLex(s *prelude.Source) ([]*Token, error) {
	l := NewLexer(s)
	defer close(l.Tokens)

	var err error
	l.Error = func(e *prelude.Error) { err = e }
	tokens := make([]*Token, 0, 200)

	go l.Lex()
Loop:
	for {
		select {
		case t := <-l.Tokens:
			tokens = append(tokens, t)
			switch t.Kind {
			case TokenEOF, TokenIllegal:
				break Loop
			}
		}
	}

	return tokens, err
}

func TestLexingOK(t *testing.T) {
	inputs, err := filepath.Glob("testdata/*.dcs")
	if err != nil {
		panic(err)
	}

	outputs, err := filepath.Glob("testdata/*.tokens")
	if err != nil {
		panic(err)
	}

	if len(inputs) == 0 {
		panic("No test found")
	}

	for _, input := range inputs {
		base := filepath.Base(input)
		expect := ""
		outputFile := strings.TrimSuffix(input, filepath.Ext(input)) + ".tokens"
		for _, e := range outputs {
			if e == outputFile {
				expect = e
				break
			}
		}

		// If there is no .tokens file, skip it.
		if expect == "" {
			continue
		}

		t.Run(base, func(t *testing.T) {
			s, err := prelude.NewSourceFromFile(input)
			if err != nil {
				t.Fatal(err)
			}

			tokens, err := testLex(s)

			if err != nil {
				t.Fatal(err)
			}

			f, err := os.Open(expect)
			if err != nil {
				panic(err)
			}

			lines := make([]string, 0, 200)
			scan := bufio.NewScanner(f)
			for scan.Scan() {
				lines = append(lines, scan.Text())
			}

			if len(tokens) != len(lines) {
				t.Fatal("Number of tokens is unexpected. want: %d, got: %d", len(lines), len(tokens))
			}

			for i, tok := range tokens {
				line := lines[i]
				s := tok.String()
				if !strings.HasPrefix(s, "<"+line) {
					t.Errorf("Token mismatch. want '%s' but actually '%s'", line, s)
				}
			}
		})
	}
}

func TestLexingInvalid(t *testing.T) {
	for _, input := range []string{
		"3.14e",
		"0xg",
		"0b2",
		"?",
		"\"unclosed string",
		"a | b",
		"a & b",
		"\xDF\xFF",
	} {
		_, err := testLex(prelude.NewDummySource(input))
		if err == nil {
			t.Errorf("%s: Expected that an error occurred, but there was no error", input)
		}
	}
}
