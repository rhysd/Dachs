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

e.g.

```
!! Import all names as 'array' namespace. e.g. array.Array
import std.array

!! Import all names from DACHS_LIB_ROOT/std/array.dcs with exposing
import std.array.*

!! Import 'Array' only from DACHS_LIB_ROOT/std/array.dcs with exposing
import std.array.Array

!! Import 'is_digit' and 'to_string' from DACHS_LIB_ROOT/std/numeric.dcs with exposing
import std.numeric.{is_digit, to_string}
```

Note that `std.array` is implicitly imported if array is used in a program.

## Export

TBD. Currently all names are exposed in public.

## `DACHS_LIB_ROOT`

TBD. Currently `DACHS_LIB_ROOT` is fixed to `lib` directory at compiler repository.

# Implementation

Module dependency is resolved before starting semantic analysis.
If the target program contains some import paths, compiler parses the imported files, and unify
names in the result (AST) into the main module following the `import` statement.

So currently Dachs cannot separate compilation unit into each file. All is compiled as one compilation unit.

# Module of Main Program

File which contains `main` function is implicitly treated as special module `main`.

---
[Top](./README.md)
