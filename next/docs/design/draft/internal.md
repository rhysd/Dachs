This document describes some internal low-level APIs.

These APIs are used for cooperation with C, interaction with OS, or implementation of essential
some data structures like arrays or dictionaries.

# `unsafe` module

All intrinsic functions and types for dangerous operations are defined in `std.unsafe` module.

```
import std.unsafe
```

# Pointer

## Pointer Type

```
unsafe.ptr(T)
```

`ptr(T)` represents `T *` in C. It is created by `unsafe.malloc` intrinsic. Compiler supports to create a string

## Pointer Operations

TBW

# Malloc

```
unsafe.malloc(n: uint): unsafe.ptr(T)
```

`unsafe.malloc` intrinsic function allocates `n` bytes memory by `GC_malloc` and returns
a pointer which points the head of the memory. The type of pointer must be inferred by type
inference.

e.g.

```
!! Array is mutable and has a `T *` pointer as data
type Array = {
    data: unsafe.ptr('a)
    length: uint
    capacity: uint
}

!! String is immutable and has a `char *` pointer as data
type String = {
    data: unsafe.ptr(i8)
    size: uint
}
```

# Static Array

TBW

---
[Top](./README.md)
