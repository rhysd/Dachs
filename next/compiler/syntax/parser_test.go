package syntax

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
)

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

			prog, err := Parse(s)
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
			got = got[1:] // Omit header

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
				t.Fatalf("Number of lines is unexpected. want: %d, got: %d. Output was '%s'", len(want), len(got), strings.Join(got, "\n"))
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

func TestParseFailed(t *testing.T) {
	cases := []struct {
		what     string
		code     string
		expected string
	}{
		{
			what:     "lexer error",
			code:     "12 # 3",
			expected: "but got '#'",
		},
		{
			what:     "mixing unnamed field in record type",
			code:     "42 as {a: int, _: bool}",
			expected: "Mixing unnamed and named fields are not permitted in record type",
		},
		{
			what:     "mixing unnamed field in tuple type",
			code:     "x as {_: bool, a: int}",
			expected: "Mixing unnamed and named fields is not permitted in tuple type",
		},
		{
			what:     "invalid if expression",
			code:     "(if true then 42 end)",
			expected: "'else' block of 'if' expression is incorrect",
		},
		{
			what:     "invalid switch expression",
			code:     "(switch case 42 then let x = 42; else 10 end)",
			expected: "1st 'case' block of 'switch' expression is incorrect",
		},
		{
			what:     "invalid match expression",
			code:     "(match 42 with 42 then true else a = 42 end)",
			expected: "'else' block of 'match' expression is incorrect",
		},
		{
			what:     "invalid expression block of lambda body",
			code:     "-> a, b do x = 42 end",
			expected: "Blocks in expression must end with an expression",
		},
		{
			what:     "invalid expression block of lambda body (part 2)",
			code:     "-> do a = 42 end",
			expected: "Blocks in expression must end with an expression",
		},
		{
			what:     "mixing unnamed field in tuple literal",
			code:     "{_: true, a: 42}",
			expected: "2nd field of tuple literal must be unnamed",
		},
		{
			what:     "mixing unnamed field in record literal",
			code:     "{a: 42, _: true}",
			expected: "2nd field of record literal must be named",
		},
		{
			what:     "too large integer literal",
			code:     "123456789123456789123456789123456789123456789",
			expected: "value out of range",
		},
		{
			what:     "invalid float literal",
			code:     "1.7976931348623159e308",
			expected: "value out of range",
		},
		{
			what:     "invalid string literal",
			code:     "\"\\129\"",
			expected: "invalid syntax",
		},
		{
			what:     "too large integer literal pattern",
			code:     "match x with 123456789123456789123456789123456789123456789 then true end",
			expected: "value out of range",
		},
		{
			what:     "invalid float literal pattern",
			code:     "match x with 1.7976931348623159e308 then true end",
			expected: "value out of range",
		},
		{
			what:     "invalid string literal pattern",
			code:     "match x with \"\\129\" then true end",
			expected: "invalid syntax",
		},
	}

	for _, tc := range cases {
		t.Run(tc.what, func(t *testing.T) {
			code := fmt.Sprintf("func main; %s; end\n", tc.code)
			src := prelude.NewDummySource(code)
			_, err := Parse(src)
			if err == nil {
				t.Fatalf("No error occurred with wrong source '%s'", code)
			}
			msg := err.Error()
			if !strings.Contains(msg, tc.expected) {
				t.Fatalf("Unexpected error message '%s'. Expected to contain '%s' in it.", msg, tc.expected)
			}
		})
	}
}
