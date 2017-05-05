package syntax

import (
	"bufio"
	"bytes"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func testParseSource(s *prelude.Source) (*ast.Program, error) {
	l := NewLexer(s)
	defer close(l.Tokens)

	var lexErr error
	l.Error = func(e *prelude.Error) { lexErr = e }

	go l.Lex()
	prog, err := Parse(l.Tokens)

	if lexErr != nil {
		return nil, lexErr
	}
	if err != nil {
		return nil, err
	}
	return prog, nil
}

func TestParseOK(t *testing.T) {
	inputs, err := filepath.Glob("testdata/*.dcs")
	if err != nil {
		panic(err)
	}

	outputs, err := filepath.Glob("testdata/*.ast")
	if err != nil {
		panic(err)
	}

	if len(inputs) == 0 {
		panic("No test found")
	}

	for _, input := range inputs {
		base := filepath.Base(input)
		expect := ""
		outputFile := strings.TrimSuffix(input, filepath.Ext(input)) + ".ast"
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

			prog, err := testParseSource(s)
			if err != nil {
				t.Fatal(err)
			}

			var buf bytes.Buffer
			ast.Fprint(&buf, prog)

			got := make([]string, 0, 200)
			for {
				line, err := buf.ReadString('\n')
				if err != nil {
					// Last line does not have traling newline
					got = append(got, line)
					break
				}
				got = append(got, line[:len(line)-1])
			}

			f, err := os.Open(expect)
			if err != nil {
				panic(err)
			}

			want := make([]string, 0, 200)
			scan := bufio.NewScanner(f)
			for scan.Scan() {
				want = append(want, scan.Text())
			}

			if len(got) != len(want) {
				t.Fatalf("Number of tokens is unexpected. want: %d, got: %d. Output was '%s'", len(want), len(got), strings.Join(got, ""))
			}

			for i, w := range want {
				g := got[i]
				if g != w {
					t.Errorf("Printed AST at line %d mismatch.\n  Want '%s'\n   Got '%s'  ", i+1, w, g)
				}
			}
		})
	}
}
