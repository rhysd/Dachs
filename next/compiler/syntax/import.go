package syntax

import (
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"os"
	"path/filepath"
	"strings"
)

func defaultLibPaths() ([]string, error) {
	exe, err := filepath.Abs(os.Args[0])
	if err != nil {
		return nil, err
	}
	paths := []string{filepath.Join(filepath.Dir(exe), "lib", "dachs")}
	env := os.Getenv("DACHS_LIB_PATH")
	if env != "" {
		split := strings.Split(env, ":")
		for _, s := range split {
			if s != "" {
				paths = append(paths)
			}
		}
	}
	return paths, nil
}

type importResolver struct {
	defaultLibPaths []string
	parsedAST       map[string]*ast.Program
}

func newImportResolver() (*importResolver, error) {
	paths, err := defaultLibPaths()
	if err != nil {
		return nil, err
	}
	return &importResolver{paths, map[string]*ast.Program{}}, nil
}

func (res *importResolver) resolveInDir(dir string, node *ast.Import) (*ast.Module, error) {
	file := filepath.Join(dir, filepath.Join(node.Parents...)) + ".dcs"

	ns := ""
	names := node.Imported
	all := false
	if node.Imported[0] == "*" {
		// When `import foo.*`, 'foo' is a namespace of all names in foo.dcs
		ns = node.Parents[len(node.Parents)-1]
		names = nil
		all = true
		// Note: len(node.Parents) == 1 is guaranteed by a parser. So don't need to check it.
	}
	mod := &ast.Module{
		NameSpace: ns,
		Imported:  names,
		ExposeAll: all,
	}

	if prog, ok := res.parsedAST[file]; ok {
		mod.AST = prog
		return mod, nil
	}

	src, err := prelude.NewSourceFromFile(file)
	if err != nil {
		return nil, err
	}

	var lexErr error
	l := NewLexer(src)
	l.Error = func(e *prelude.Error) {
		lexErr = e
	}
	defer close(l.Tokens)
	go l.Lex()
	prog, err := Parse(l.Tokens)
	if lexErr != nil {
		return nil, lexErr
	}
	if err != nil {
		return nil, err
	}

	// Recursively resolve imported modules in parsed AST
	if err := res.resolve(prog); err != nil {
		return nil, err
	}

	mod.AST = prog
	return mod, nil
}

func (res *importResolver) resolveImport(node *ast.Import) (*ast.Module, error) {
	src := node.StartPos.File
	if node.Relative {
		dir := filepath.Dir(src.Name)
		mod, err := res.resolveInDir(dir, node)
		if err != nil {
			return nil, prelude.NewErrorf(node.StartPos, node.EndPos, "Cannot import '%s' in directory '%s': %s", node.Path(), dir, err.Error())
		}
		return mod, nil
	}

	errors := make([]error, 0, len(res.defaultLibPaths))
	for _, dir := range res.defaultLibPaths {
		mod, err := res.resolveInDir(dir, node)
		if err == nil {
			return mod, nil
		}
		errors = append(errors, err)
	}

	err := prelude.NewErrorf(node.StartPos, node.EndPos, "Cannot import '%s'. Searched directories: %v", node.Path(), res.defaultLibPaths)
	for _, e := range errors {
		err = err.Wrap(e.Error())
	}
	return nil, err
}

func (res *importResolver) resolve(prog *ast.Program) error {
	src := prog.File()
	if src.Exists {
		res.parsedAST[src.Name] = prog
	}
	for _, toplevel := range prog.Toplevels {
		if i, ok := toplevel.(*ast.Import); ok {
			mod, err := res.resolveImport(i)
			if err != nil {
				return err
			}
			prog.Modules = append(prog.Modules, mod)
		}
	}
	return nil
}
