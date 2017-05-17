# Syntax

## Import

```
import {module path}
import {module path}.*
import {module path}.{name}
import {module path}.{{name1}, {name2}, ...}
```

Module path is a dot (`.`) separated entries. `.` will be converted into file path separater.
For example, `std.array` represents `array.dcs` under `std` directory.

If the module path starts with a dot, it means relative path from the source file. For example,
`.foo.bar` means `./foo/bar.dcs`.

e.g.

```
!! Import all names from DACHS_LIB_PATH/std/array.dcs exposing into 'array' namespace
import std.array.*

!! Import and expose only 'Array' from DACHS_LIB_PATH/std/array.dcs
import std.array.Array

!! Import 'is_digit' and 'to_string' from DACHS_LIB_PATH/std/numeric.dcs with exposing
import std.numeric.{is_digit, to_string}

!! Import 'Blah' from local file './foo/bar.dcs'
import .foo.bar.Blah
```

Note that `std.array` is implicitly imported if array is used in a program.

### `import foo.bar` or `import foo.{a, b, ...}`

`foo` must be a file. A compiler exposes `bar` into local directly. If there are multiple names
surrounded with `{` and `}`, it exposes all the names into local directly.
So `import foo.bar` has the same meaning as `import foo.{bar}`.

### `import foo.*`

A compiler assumes `foo` is a file and exposes all names directly in file local.
It exposes all names in file `foo` into `foo` namespace. For example, if file `foo` contains name
`piyo`, you can access it via `foo.piyo`.

### `import .foo.bar` or `import .foo.{a, b, ...}` or `import .foo.*`

If import path starts with `.`, it means a root of search path is the parent directory of the source
file. Above all kinds of import paths are available for this.

TODO: Add syntax to make an alias of imported module (e.g. `import std.array as A`) for avoiding name conflicts.

## Export

TBD. Currently all names are exposed in public.

## `DACHS_LIB_PATH`

`DACHS_LIB_PATH` is an environment variable separated with `:`. Each separated entry should represent
path to a directory. Compiler searches files in the directories and resolves `import` paths.

The default value is `../lib/dachs` relative path to the parent directory of `dachs` executable.
For example if `dachs` is installed in `/usr/local/bin/dachs`, `/usr/local/lib/dachs` will be
searched. And if `dachs` is built in `/path/to/Dachs/next/compiler/dachs`, `/path/to/Dachs/next/lib/dachs`
will be searched.

(TBD)

# Implementation

Module dependency is resolved before starting semantic analysis.
If the target program contains some import paths, compiler parses the imported files, and unify
names in the result (AST) into the main module following the `import` statement.

So currently Dachs cannot separate compilation unit into each file. All is compiled as one compilation unit.

---
[Top](./README.md)
