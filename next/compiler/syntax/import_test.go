package syntax

import (
	"fmt"
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

	paths := res.libPaths
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
	env := fmt.Sprintf("%s:%s", dir, filepath.Join("testdata", "import", "externallib"))
	os.Setenv("DACHS_LIB_PATH", env)
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
		got := res.libPaths[1:]
		if len(tc.want) != len(got) {
			t.Fatalf("Number of paths for %s incorrect. want: %v, got: %v", tc.env, tc.want, got)
		}
		for i, w := range tc.want {
			g := got[i]
			if !strings.HasSuffix(g, w) {
				t.Errorf("%dth path mismatched for env %s. want: %v, got: %v", i+1, tc.env, tc.want, got)
			}
		}
	}
}

type moduleExpected struct {
	file      string
	namespace string
	exposeAll bool
	symbols   []string
}

func testModule(m *ast.Module, spec string, expected moduleExpected) string {
	f := filepath.FromSlash("testdata/import/" + expected.file)
	n := m.AST.File().Name
	if !strings.HasSuffix(n, f) {
		return fmt.Sprintf("Expected file path of '%s' to be '%s' but actuall '%s' was imported", spec, f, n)
	}
	if m.ExposeAll != expected.exposeAll {
		return fmt.Sprintf("ExposeAll for '%s' is incorrect. want: %v, have: %v", spec, expected.exposeAll, m.ExposeAll)
	}
	if len(m.Imported) != len(expected.symbols) {
		return fmt.Sprintf("Number of imported symbol for '%s' is incorrect. want: %d, have: %d. Note: %v", spec, len(expected.symbols), len(m.Imported), m.Imported)
	}
	for i, want := range expected.symbols {
		have := m.Imported[i]
		if want != have {
			return fmt.Sprintf("%dth imported symbol from '%s' mismatch. want: %s, have: %s", i, spec, want, have)
		}
	}
	if m.NameSpace != expected.namespace {
		return fmt.Sprintf("Namespace for '%s' is incorrect. want: %s, have: %s", spec, expected.namespace, m.NameSpace)
	}
	return ""
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

	if msg := testModule(mods[0], "foo.*", moduleExpected{
		file:      "nested/foo.dcs",
		namespace: "foo",
		exposeAll: true,
	}); msg != "" {
		t.Error(msg)
	}

	if msg := testModule(mods[1], "b.bar.piyo", moduleExpected{
		file:    "nested/b/bar.dcs",
		symbols: []string{"piyo"},
	}); msg != "" {
		t.Error(msg)
	}

	ms := mods[0].AST.Modules
	if len(ms) != 0 {
		t.Fatal("foo.dcs does not import anything but modules is not empty", ms)
	}

	ms = mods[1].AST.Modules
	if len(ms) != 1 {
		t.Fatal("Unexpected number of modules for b.bar.dcs:", len(ms))
	}

	if msg := testModule(ms[0], "foo.{a, b, c}", moduleExpected{
		file:    "nested/foo.dcs",
		symbols: []string{"a", "b", "c"},
	}); msg != "" {
		t.Error(msg)
	}
}

func TestLocalImports(t *testing.T) {
	prog, err := testParseAndResolveModules("local")
	if err != nil {
		t.Fatal(err)
	}
	mods := prog.Modules
	if len(mods) != 1 {
		t.Fatal("Unexpected number of modules for main.dcs:", len(mods))
	}

	if msg := testModule(mods[0], ".aaa.foo.bar", moduleExpected{
		file:    "local/aaa/foo.dcs",
		symbols: []string{"bar"},
	}); msg != "" {
		t.Error(msg)
	}

	mods = mods[0].AST.Modules
	if len(mods) != 2 {
		t.Fatal("Unexpected number of modules for aaa/foo.dcs:", len(mods))
	}
	if msg := testModule(mods[0], ".bar.*", moduleExpected{
		file:      "local/aaa/bar.dcs",
		exposeAll: true,
		namespace: "bar",
	}); msg != "" {
		t.Error(msg)
	}
	if msg := testModule(mods[1], ".bbb.piyo.wow", moduleExpected{
		file:    "local/aaa/bbb/piyo.dcs",
		symbols: []string{"wow"},
	}); msg != "" {
		t.Error(msg)
	}
	if len(mods[1].AST.Modules) != 0 {
		t.Fatal("Unexpected number of modules for aaa/bar.dcs:", len(mods[1].AST.Modules))
	}

	mods = mods[0].AST.Modules
	if len(mods) != 1 {
		t.Fatal("Unexpected number of modules for aaa/foo.dcs:", len(mods))
	}
	if msg := testModule(mods[0], ".bbb.piyo.{a, b, wow}", moduleExpected{
		file:    "local/aaa/bbb/piyo.dcs",
		symbols: []string{"a", "b", "wow"},
	}); msg != "" {
		t.Error(msg)
	}
}

func TestImportExternalLib(t *testing.T) {
	prog, err := testParseAndResolveModules("uselib")
	if err != nil {
		t.Fatal(err)
	}
	mods := prog.Modules
	if len(mods) != 1 {
		t.Fatal("Unexpected number of modules for main.dcs:", len(mods))
	}

	if msg := testModule(mods[0], "mylib.aaa.api", moduleExpected{
		file:    "externallib/mylib/aaa.dcs",
		symbols: []string{"api"},
	}); msg != "" {
		t.Error(msg)
	}

	mods = mods[0].AST.Modules
	if len(mods) != 2 {
		t.Fatal("Unexpected number of modules for main.dcs:", len(mods))
	}

	if msg := testModule(mods[0], "<mylib>.foo.bbb.piyo", moduleExpected{
		file:      "externallib/mylib/foo/bbb.dcs",
		namespace: "bbb",
		exposeAll: true,
	}); msg != "" {
		t.Error(msg)
	}

	if msg := testModule(mods[1], "<mylib>.ccc.{a, b, c}", moduleExpected{
		file:    "externallib/mylib/ccc.dcs",
		symbols: []string{"a", "b", "c"},
	}); msg != "" {
		t.Error(msg)
	}
}

func TestImportFailed(t *testing.T) {
	cases := []struct {
		what     string
		expected string
	}{
		{"error_myself", "Cannot import myself"},
		{"error_unknown", "Cannot import '.unknown.foo'"},
		{"error_unknown_external", "Cannot import 'unknown.lib'"},
		{"error_parse", "Cannot import '.foo.piyo'"},
	}

	for _, tc := range cases {
		t.Run(tc.what, func(t *testing.T) {
			_, err := testParseAndResolveModules(tc.what)
			if err == nil {
				t.Fatal("Importing myself should cause an error", tc.what)
			}
			if !strings.Contains(err.Error(), tc.expected) {
				t.Errorf("Expected error message '%s' but actually it was '%s'", tc.expected, err.Error())
			}
		})
	}
}
