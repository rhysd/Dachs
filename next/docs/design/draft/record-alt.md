Record is a compound type which contains zero or more fields. Field is a pair of name and its type.

# Definition

```
(field1[: Type1], field2[: Type2], ...}
(Type1, Type2, ...)
```

Example:

```
!! Empty
()

!! It has two fields, age (typed as int) and name (typed as str).
(age: int, name: str)

!! Field name only
(age, name)

!! Types only
(MyType, int, str)

!! Types only, and types are variable. It means pair (two elements tuple).
('a, 'b)

!! Partially typed
(age: int, name)

!! Nested
(name: str, (age: int, has_dog: bool))
```

Records defined with types only are known as tuple. It has unnamed fields. Instead of names,
it matches its fields with position.

# Anonymous Record Literal

```
(field1: expr1, field2: expr2, ...)
(expr1, expr2, ...)
```

Example:

```
!! Empty
()

!! Record which has two fields (anonymous)
!! Typed as (age: int, name: str)
(age: 12, name: "John")

!! Unnamed fields (typed as (int, str))
(12, "John")
```

Constructing a record with unnamed fields is tuple creation. Their fields are positionally matched.

# How to Define Named Record

Use `rec` syntax to define named record types. This is similar to `struct` syntax in C.

```
rec {Name}
    {field1} [: {type1}]
    {field2} [: {type2}]
    ...
end

rec {Name}()
```

```
rec Empty()

rec Person(age, name)
rec Person(age: int, name: str)
rec Person
    age, name
end
rec Person
    age: int
    name: str
end
```

Record type definition has zero or more field definitions. Field definition can omit its type.
When omitting a type, type of the field will be a type variable and will be instantiated
following its usage. For example, `Foo(bar)` is the same as `Foo(bar: '0)`. When it is
instantiated with `Foo(42)`, `Foo(bar: int)` will be instantiated.

# Creating Named Record

When defining named record with `rec` syntax, it also define a constructor function implicitly.
Constructor function is a normal function to make the record instance.

```
rec Person(age, name)

!! Above definition make below function implicitly
func Person(age: '0, name: '1): Person
    !! implementation-defined
end

!! Make an instance of Person

!! Function call
Person(12, "John")
!! Function call with named arguments
Person(age: 12, name: "John")
```

So named record can be constructed with positional arguments and named arguments. This is
compatible with anonymous record creations such as `(12, "John")` or `(age: 12, name: "John")`.

Names of the function and record don't make a collision because namespace of types and
namespace of functions are different.

XXX: constructor function is incompatible with unnamed fields (tuples). When the type is
defined as `Person(int, str)`, how can we name the parameters of constructor function of `Person`?

# Access to Field of Instance

```
{expr}.{field}
```

For example:

```
p := {name: "John", age: 12}

!! Access to 'age' field
p.age
```

For records having unnamed fields (tuples), can be accessible with field index.

```
p := ("John", 12)

!! Access to second field directly
p.1
```

# Enum Record

## Definition

Enum record defines variant type which represents one of several possible variants.

```
enum Number
case Integer(int)
case Rational(int, int)
case Complex
    real: float
    imag: float
end
```

The spec of `(...)` in `case` arm is the same as `enum`'s body syntax. So it accepts field name,
type, field name with type as its fields.
So, for example, if you omit field name and specify its type as type variable, Option type
can be represented as below.

```
enum Option
case Some('a)
case None
end
```

## Make an Instance

Enumerated record types by `enum` syntax can be access by `Type::EnumType`.

```
Number::Integer(42)
Number::Rational(3, 10)
Number::Complex(real: 3.0, imag: 2.0)
Number::Complex(3.0, 2.0)
```

Arguments are the same as normal named records (constructor function).

## Access to Enum Values with Pattern Matching

`match` statement/expression is available.

```
i := Number::Integer(42)
match i
case Integer(i)
    print("integer"); print(i)
case Rational(denom, num)
    print("rational"); print(denom); print(num)
case Complex(real, imag)
    print("complex"); print(real); print(imag)
end

!! Note: Writing 'else' causes an error because compiler checks exhaustivity
```

In above example, fields of `Integer` and `Rational` were defined as unnamed. So the name `i`,
`denom` and `num` can be named freely at the `case` arms.
In contrast, field names of `Complex` are fixed to `real` and `imag` because they were named
at definition. Specifying different name causes compilation error.

So, for example, `Option` type can be used as below.

```
enum Option
case Some('a)
case None
end

o := Option::Some(42)

match o
case Some(i)
    print("Found"); print(i)
case None
    print("Not found")
end
```

---
[Top](./README.md)

