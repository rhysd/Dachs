Compiler for Dachs Programming Language
=======================================

Compiler consists of some packages written in Go.

## Packages

A compiler consists of some separated Go packages.

- [driver](./driver): Driver is a mediator to glue all separated packages
  - [driver/dachs](./driver/dachs): Command line interface of a compiler
- [ast](./ast): Syntax tree of Dachs. It contains node definitions and visitor API
- [syntax](./syntax): Syntax analysis (lexing and parsing)
- [types](./types): Type definitions
- [semantics](./semantics): Semantic analysis (type inference, various semantic checks and conversion to MIR)
- [mir](./mir): MIR (Midlevel Intermediate Representation) definitions
- [codegen](./codegen): LLVM IR code generation using [LLVM Go bindings](https://godoc.org/llvm.org/llvm/bindings/go/llvm)

## Prerequisites

- [Go 1.7+](https://golang.org/)
- [bdwgc](https://github.com/ivmai/bdwgc)
- [clang](https://clang.llvm.org/)
- `make`
- Git
- [cmake](https://cmake.org/) for building LLVM

To install libgc (bdwgc), use package manager for your OS.

- Debian family Linux: `apt-get install libgc-dev`
- macOS: `brew install bdw-gc`

## How to build

### 1. Download source

```
$ go get -d github.com/rhysd/Dachs/next/compiler
```

### 2. Build with `make` command

```
$ make
```

It downloads and builds [LLVM 4.0](llvm.org) (if you don't set `USE_SYSTEM_LLVM`)
as dependency before building a compiler.

### Use system-installed LLVM

Set `$USE_SYSTEM_LLVM` to `TRUE` on running `make`.

- On Linux:

```
$ sudo apt-get install libllvm4.0 llvm-4.0-dev
$ export LLVM_CONFIG=llvm-config-4.0
$ USE_SYSTEM_LLVM=true make
```

- On macOS:

```
$ brew install llvm
$ USE_SYSTEM_LLVM=true make
```

## How to Run Tests

```
$ go test ./...
```

## License

MIT License

> Copyright (c) 2017 rhysd

