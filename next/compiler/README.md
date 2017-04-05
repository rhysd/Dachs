Compiler for Dachs Programming Language
=======================================

Compiler consists of some packages written in Go.

## Prerequisities

- [Go 1.7+](https://golang.org/)
- [bdwgc](https://github.com/ivmai/bdwgc)
- [clang](https://clang.llvm.org/)
- `make`
- Git
- [cmake](https://cmake.org/) for building LLVM

To install libgc (bdwgc), use pacakge manager for your OS.

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

