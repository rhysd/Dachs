package syntax

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"os"
	"path/filepath"
	"strings"
)

func libraryPaths() ([]string, error) {
	exe, err := filepath.Abs(os.Args[0])
	if err != nil {
		return nil, err
	}
	paths := []string{filepath.Join(filepath.Dir(exe), "lib", "dachs")}
	env := os.Getenv("DACHS_LIB_PATH")
	if env != "" {
		split := strings.Split(env, ":")
		for _, s := range split {
			if s == "" {
				continue
			}
			p, err := filepath.Abs(s)
			if err != nil {
				prelude.Log("Failed to make an absolute path from", s)
				continue
			}
			paths = append(paths, p)
		}
	}
	prelude.Log("Default library path:", paths)
	return paths, nil
}

type importResolver struct {
	libPaths    []string
	projectPath string
	parsedAST   map[string]*ast.Program
}

func newImportResolver() (*importResolver, error) {
	paths, err := libraryPaths()
	if err != nil {
		return nil, err
	}
	return &importResolver{paths, "", map[string]*ast.Program{}}, nil
}

func (res *importResolver) resolveInDir(dir string, node *ast.Import) (*ast.Module, error) {
	file := filepath.Join(dir, filepath.Join(node.Parents...)) + ".dcs"

	if node.StartPos.File.Name == file {
		return nil, fmt.Errorf("Cannot import myself")
	}

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
		prelude.Log("Resolved module from cache:", file)
		return mod, nil
	}

	src, err := prelude.NewSourceFromFile(file)
	if err != nil {
		return nil, err
	}
	prelude.Log("Will parse module:", src)

	prog, err := Parse(src)
	if err != nil {
		return nil, err
	}
	prelude.Log("Parsed module:", src)

	// Recursively resolve imported modules in parsed AST
	if err := res.resolve(prog); err != nil {
		return nil, err
	}

	mod.AST = prog
	return mod, nil
}

func (res *importResolver) findRelativePath(node *ast.Import) (string, error) {
	dir := filepath.Dir(node.StartPos.File.Name)
	prelude.Log("Will find lib path for relative import in", dir)
	if res.projectPath != "" && filepath.HasPrefix(dir, res.projectPath) {
		// Target directory is in project local
		return res.projectPath, nil
	}
	for _, libpath := range res.libPaths {
		if filepath.HasPrefix(dir, libpath) {
			// When
			//   dir == '/foo/lib/libname/bar/piyo'
			//   libpath == '/foo/lib'
			// we want '/foo/lib/libname' to obtain the root directory of the library.
			// This is needed because relative import path `.foo.bar` in a library should be
			// resolved as a relative path to the root of the library.

			dir, entry := filepath.Split(dir)

			// filepath.Clean() is necessary because first return value of filepath.Split() is NOT
			// cleaned. Trailing '/' remains in the value.
			for filepath.Clean(dir) != libpath {
				dir, entry = filepath.Split(dir)
			}
			return filepath.Join(libpath, entry), nil
		}
	}
	return "", prelude.NewErrorf(node.StartPos, node.EndPos, "Cannot import '%s' because directory '%s' does not belong to project path '%s' nor library paths %v", node.Path(), dir, res.projectPath, res.libPaths)
}

func (res *importResolver) resolveImport(node *ast.Import) (*ast.Module, error) {
	if node.Local {
		// When `import .foo.bar`, `import .foo.*` or `import .foo.{a, b}`
		path, err := res.findRelativePath(node)
		if err != nil {
			return nil, err
		}
		prelude.Log("Found lib path:", path)
		mod, err := res.resolveInDir(path, node)
		if err != nil {
			return nil, prelude.Wrapf(node.StartPos, node.EndPos, err, "Cannot import '%s' in project '%s'", node.Path(), res.projectPath)
		}
		return mod, nil
	}

	errors := make([]error, 0, len(res.libPaths))
	for _, dir := range res.libPaths {
		mod, err := res.resolveInDir(dir, node)
		if err == nil {
			return mod, nil
		}
		errors = append(errors, err)
	}

	err := prelude.NewErrorf(node.StartPos, node.EndPos, "Cannot import '%s'. Searched directories: %v", node.Path(), res.libPaths)
	for _, e := range errors {
		err = err.Wrap(e.Error())
	}
	return nil, err
}

func (res *importResolver) resolve(prog *ast.Program) error {
	src := prog.File()
	prelude.Log("Will resolve imports in", src, prog.Pos())
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

// ResolveImports resolves all `import` statements in the program recursively.
// The results will be set to `Modules` field of `Program` node.
func ResolveImports(root *ast.Program) error {
	res, err := newImportResolver()
	if err != nil {
		return prelude.NewErrorfAt(root.Pos(), "Error occurred while getting library path: %s", err.Error())
	}

	src := root.File()
	if src.Exists {
		res.projectPath = filepath.Dir(src.Name)
	}

	prelude.Log("Finished resolving modules successfully")
	return res.resolve(root)
}
