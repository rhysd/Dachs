# Primitive Type

Primitive types start with lower case.

- `int` : signed 64bit integer
- `uint` : unsigned 64bit integer
- `bool` : boolean type (`true` or `false`)
- `float` : 64bit floating point number
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

# Type Variable

Type variable is a placeholder which can be replaced by actual type. The name starts with `'`.
This is used for making a variant type.

```
type Pair of {'a, 'b}
```

Above `Pair` type has two fields. But actual types of the fields are not defined. It is determined
at the point of instantiation.

```
!! Instantiate Pair{int, str}
Pair{42, "foo"}

!! Instantiate Pair{Person, Grade}
Pair{Person{12, "John"}, Grade{4}}
```

Even if the definition is the same, types instantiated with different types are different from each other.

Generally one character is used such as `'a` or `'b` for this. And if types of fields are omitted as below,
type variables are assigned by a compiler. Automatically assigned type variables are named `'0`, `'1`, ...

```
type Query of {question, answer}

!! Above is the same as
type Query of {question: '0, answer: '1}
```

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

# `typeof()`

```
typeof({expression})
```

`typeof()` is expanded to the type of `{expression}`.

```
!! int
typeof(42)

type MyInt of int
a := 42 as MyInt
b := 10 as typeof(a)

!! b has the same type of a
print(a == b)
```

# Function Type

```
({param type}, {param type}, {param type}, ...) -> {return type}
```

e.g.

```
(int, str) -> float
```

# Type Instantiation

```
Type{param1, param2, ...}
```

e.g.

```
!! Generic type User{'0, '1}
type User of {name, data}

!! Instantiate User with str and int
type IntUser of User{str, int}

!! Instantiate User with str and float
type FloatUser of User{str, float}

!! Only set an actual type to the first parameter
type User2 of User{str, '1}
```

# Pointer Type

Pointer type is provided for low-level implementations. Please see [internal.md](./internal.md).

---
[Top](./README.md)
