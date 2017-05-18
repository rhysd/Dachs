package syntax

import (
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestDefaultPath(t *testing.T) {
	res, err := newImportResolver()
	if err != nil {
		t.Fatal(err)
	}

	paths := res.defaultLibPaths
	if len(paths) != 1 {
		t.Fatal("Unexpected number of default library path:", len(paths))
	}

	path := paths[0]
	if !strings.HasSuffix(path, filepath.Join("lib", "dachs")) {
		t.Fatal("Default library path must end with 'lib/dachs':", path)
	}
}

func testParseAndResolveModules(dir string) (*ast.Program, error) {
	saved := os.Getenv("DACHS_LIB_PATH")
	defer os.Setenv("DACHS_LIB_PATH", saved)
	dir = filepath.Join("testdata", "import", dir)
	os.Setenv("DACHS_LIB_PATH", dir)
	src, err := prelude.NewSourceFromFile(filepath.Join(dir, "main.dcs"))
	if err != nil {
		return nil, err
	}
	prog, err := Parse(src)
	if err != nil {
		return nil, err
	}
	if err := ResolveImports(prog); err != nil {
		return nil, err
	}
	return prog, nil
}

func TestLibraryPathsFromEnv(t *testing.T) {
	cases := []struct {
		env  string
		want []string
	}{
		{"foo", []string{"foo"}},
		{"foo:bar", []string{"foo", "bar"}},
		{"foo:bar:piyo", []string{"foo", "bar", "piyo"}},
		{":foo", []string{"foo"}},
		{"foo:", []string{"foo"}},
		{":foo:", []string{"foo"}},
	}

	saved := os.Getenv("DACHS_LIB_PATH")
	defer os.Setenv("DACHS_LIB_PATH", saved)

	for _, tc := range cases {
		os.Setenv("DACHS_LIB_PATH", tc.env)
		res, err := newImportResolver()
		if err != nil {
			t.Fatal(err)
		}
		got := res.defaultLibPaths[1:]
		if len(tc.want) != len(got) {
			t.Fatalf("Number of paths for %s incorrect. want: %v, got: %v", tc.env, tc.want, got)
		}
		for i, w := range tc.want {
			g := got[i]
			if w != g {
				t.Errorf("%dth path mismatched for env %s. want: %v, got: %v", i+1, tc.env, tc.want, got)
			}
		}
	}
}

func TestNestedModules(t *testing.T) {
	prog, err := testParseAndResolveModules("nested")
	if err != nil {
		t.Fatal(err)
	}
	mods := prog.Modules
	if len(mods) != 2 {
		t.Fatal("Unexpected number of modules for main.dcs:", len(mods))
	}

	m := mods[0]
	f := filepath.FromSlash("testdata/import/nested/foo.dcs")
	n := m.AST.File().Name
	if !strings.HasSuffix(n, f) {
		t.Fatalf("Expected %s but actuall %s was imported", f, n)
	}
	if !m.ExposeAll {
		t.Fatal("import * did not expose all names")
	}
	if len(m.Imported) != 0 {
		t.Fatal("Exposed all names with * but symbol is also imported", len(m.Imported))
	}
	if m.NameSpace != "foo" {
		t.Fatal("`import foo.*` must define namespace 'foo'", m.NameSpace)
	}

	m = mods[1]
	f = filepath.FromSlash("testdata/import/nested/b/bar.dcs")
	n = m.AST.File().Name
	if !strings.HasSuffix(n, f) {
		t.Fatalf("Expected %s but actuall %s was imported", f, n)
	}
	if m.ExposeAll {
		t.Fatal("import foo.bar should not expose all names")
	}
	if len(m.Imported) != 1 {
		t.Fatal("Number of imported symbol is wrong", len(m.Imported))
	}
	if m.Imported[0] != "piyo" {
		t.Fatal("Imported name was unexpected", m.Imported[0])
	}
	if m.NameSpace != "" {
		t.Fatal("`import foo.bar` must not define namespace", m.NameSpace)
	}

	ms := mods[0].AST.Modules
	if len(ms) != 0 {
		t.Fatal("foo.dcs does not import anything but modules is not empty", ms)
	}

	ms = mods[1].AST.Modules
	if len(ms) != 1 {
		t.Fatal("Unexpected number of modules for b.bar.dcs:", len(ms))
	}

	m = ms[0]
	f = filepath.FromSlash("testdata/import/nested/foo.dcs")
	n = m.AST.File().Name
	if !strings.HasSuffix(n, f) {
		t.Fatalf("Expected %s but actuall %s was imported", f, n)
	}
	if m.ExposeAll {
		t.Fatal("import foo.{a, b, c} should not expose all names")
	}
	if len(m.Imported) != 3 {
		t.Fatal("Number of imported symbol is wrong", len(m.Imported))
	}
	for i, want := range []string{"a", "b", "c"} {
		have := m.Imported[i]
		if have != want {
			t.Fatalf("As imported symbols from foo.{a, b, c}, %s is expected but actually %s", want, have)
		}
	}
}

func TestRelativeImports(t *testing.T) {
	// TODO
}
