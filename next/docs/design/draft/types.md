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
This is used for making a variable type.

```
type Pair = {'a, 'b}
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
type Query = {question, answer}

!! Above is the same as
type Query = {question: '0, answer: '1}
```

# Type Coercion

`{expr} as {type}` binary operator coerces `{expr}` to `{type}`.
It basically works as type assertion, and works as type conversion in some primitive conversion.
Conversion by coercion is permitted only for changing interpretation of the value for some
**built-in** conversion.
Or aliased type with `type ... = ...` can be also converted from underlying type.

```
!! Primitive types conversion
42 as uint
3.14 as int
3u as float

!! Conversion from underlying type to aliased type
type MyInt = int
42 as MyInt
```
# Type Alias

```
type {Name} of {type}
```

Defines `{Name}` as an alias of `{type}`. `{Name}` is defined as a new type. So it is typed *strongly* and cannot be converted from original type implicitly.

```
type MyInt = int

let my = 42 as MyInt
let i = my as int
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
It defines `Number`, `Integer`, `Rational`, `Imaginary` types.

```
type Number =
    case Integer{_: 'a}
    case Rational{numerator, denominator}
    case Imaginary{real, imaginary}
```

You can use `::` separator to refer a variant of the enum type. For example, `Number::Integer` refers
`Integer` variant of `Number` enum type. So you can make an object like record creation with `::` as
below.

```
Number::Integer{_: 42}
Number::Rational{numerator: 3, denominator: 10}
Number::Imaginary{real: 1, imaginary: 5}
```

You can also use constructor function like record type.

```
Number::Integer(42)
Number::Rational(3, 10)
Number::Imaginary(1, 5)
```

Enum type can be specified with its name. For example, below `add_one` function receives one `Number`
object and returns `Number` object. It can take any of `Number::Integer`, `Number::Rational` or
`Number::Imaginary`.

```
func add_one(n: Number): Number
    ...
end
```

Contained value can be retrieved by pattern matching.

```
let n = Number::Ratinal(3, 10)

match n
with Number::Integer{i}
    print(i)
with Number::Rational{numerator, denominator}
    print(numerator)
    print(denominator)
with Number::Imaginary{real, imaginary}
    print(real)
    print(imaginary)
end
```

# `typeof()`

```
typeof({expression})
```

`typeof()` is expanded to the type of `{expression}`.

```
!! int
typeof(42)

type MyInt = int
let a = 42 as MyInt
let b = 10 as typeof(a)

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
type User = {name, data}

!! Instantiate User with str and int
type IntUser = User{str, int}

!! Instantiate User with str and float
type FloatUser = User{str, float}

!! Only set an actual type to the first parameter
type User2 = User{str, '1}
```

# Pointer Type

Pointer type is provided for low-level implementations. Please see [internal.md](./internal.md).

---
[Top](./README.md)
