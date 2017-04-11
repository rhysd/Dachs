# Syntax

## Type

```
(type1, type2, ...)
```

Example:

```
!! Unit type (empty)
()

!! Pair of integer and string
(int, str)
```

## Literal

```
(expr1, expr2, ...)
```

Tuple is an anonymous `struct` type which contains zero or more indexed elements. You can access
to its elements with index or other positional way.

```
!! Empty value (unit)
()

!! pair of integer and string
(42, "hello")
```

# Accessing to Elements

Different from record, tuple's element isn't named. So you can access to it via index.

```
t := (12, "John")

!! Access to first element
age := t.0
!! Access to second element
name := t.1

!! Compilation error
unknown := t.2
```

# Pattern Matching

Tuple's elements are available for pattern matching with `match` statement/expression

```
pair := (true, false)

xor :=
    match pair
    case (true, false)
        true
    case (false, true)
        true
    else
        true
    end
```

---
[Top](./README.md)
