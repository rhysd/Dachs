# Primitive Type

Primitive types start with lower case.

- `int` : signed 64bit integer
- `uint` : unsigned 64bit integer
- `bool` : boolean type (`true` or `false`)
- `flaot` : 64bit floating point number
- `string` : string type

# Record Type

Please see [record.md](record.md) for more detail.

```
{field1[: Type1], field2[: Type2], ...}
{Type1, Type2, ...}
```

# Array Type

Example:

```
[int]
```

TBW

# Dictionary Type

Example:

```
[str => float]
```

TBW

# Type Coercion

`{expr} as {type}` binary operator coerces `{expr}` to `{type}`.
Coercion is permitted only for changing interpretation of the value for some **built-in** conversion

- Primitive type conversion between `int` and `uint`.
- Conversion between anonymous record and named record. They should have the same layout.

# Type Alias

```
type {Name} of {type}
```

Defines `{Name}` as an alias of `{type}`. `{Name}` is defined as a new type. So it is typed *strongly* and cannot be converted from original type implicitly.

```
type MyInt of int

my := 42 as MyInt
i := my as int
```

Note that aliased type's name **MUST** start with upper case.

# Variant Type

Variant type represents contains one of several possible variants. The syntax is based on type alias.

```
type {Name} of
    case {Variant1}
    case {Variant2}
    ...
```

Below is an example of `Number`. `Number` consists of `Integer`, `Rational` and `Imaginary` here.

```
type Number of
    case Integer{'a}
    case Rational{numerator, denominator}
    case Imaginary{real, imaginary}

func add_one(n: Number): Number
    ...
end

add_one(Number::Integer{42})
add_one(Number::Rational{3, 10})
add_one(Number::Imaginary{1, 5})
```

It defines `Number`, `Integer`, `Rational`, `Imaginary` types.
Contained value can be retrieved by pattern matching.

---
[Top](./README.md)
