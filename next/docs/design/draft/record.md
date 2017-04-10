Record is a compound type which contains zero or more fields. Field is a pair of name and its type.

Field names **MUST** start with lower case.

# Syntax

## Definition

```
{field1[: Type1], field2[: Type2], ...}
{Type1, Type2, ...}
```

Example:

```
!! Empty
{}

!! It has two fields, age (typed as int) and name (typed as str).
{age: int, name: str}

!! Field name only
{age, name}

!! Partially typed
{age: int, name}

!! Nested
{name: str, {age: int, has_dog: bool}}
```

## Literal

```
{field1: expr1, field2: expr2, ...}
```

Example:

```
!! Empty
{}

!! Record which has two fields (anonymous)
{age: 12, name: "John"}

!! Record which has two fields (named)
type Person of {age, name}
Person{age: 12, name: "John"}

!! Unnamed fields (typed as {int, str})
{12, "John"}
```

# How to Define Named Type

Type alias (`type {name} of {type}`) is available to define named type. Type alias defines new
type *strongly*. So it cannot be converted from original type implicitly.

```
type Empty of {}

type Person of {
    name: str
    age: int
}

type Student of {name, grade}
```

# Unnamed Field

Generally, field has its name. But Dachs allows field not to have a name, called unnamed field.
To define an unnamed field, omit field names. It means 'not named yet'.

Currently both named field and unnamed field cannot live in the same record. If a record has
an unnamed record, all records must be unnamed in the record.

```
!! First field is int, second field is str. But they don't have name yet.
{int, str}

!! Types are omitted. It has two fields.
{'a, 'b}
```

Basically this is not intended to be written by hand. As described in below 'Instantiation'
section, Records containing unnamed fields is used like a tuple.

```
!! This is typed as {int, str}
{12, "John"}
```

Unnamed field can be named after.

```
!! Typed as {age: int, name: str}
{12, "John"} as {age, name}

type Person of {age, name}

!! Typed as Person{age: 12, name: str}
{12, "John"} of Person

!! The same as above
Person{12, "John"}
```

To access to unnamed field directly, index number is available instead.

```
r := {12, "John"}

r.0 == 12
r.1 == "John"
```

Unnamed field is used for enum type (please see [types.md](types.md) for more detail of enum type).
This is used to name its field at pattern in `match` expression (or statement).

```
type Option of
    case Some{'a}
    case None

!! Type is Option{int}
some := Some{42}

match some
case Some{answer}
    print(answer)
case None
    print("no answer!")
end
```

Here, `Some` has one unmaed field. The field name 'answer' is determined at `case` arm at `match`.

This is useful for the data structure which contains any value (field name should be changed
depending on what is contained).

# Instantiation

Anonymous record value is like

```
{age: 12, name: "John"}
```

And named record value is like

```
type Person of {age, name}
Person{age: 12, name: "John"}
```

For records which have unnamed fields,

```
{12, "John"}
```

# Coercion

## Unnamed Fields to Named Fields

Unamed fields can be named after by coercing the record which have unnamed fields into the record
which have named fields.

```
{12, "John"} as {age, name}
```

In above example, the type will be `{age: int, name: str}`. This happens because `{int, str}`
is coreced into `{age: 'a, name: 'b}`.

Though above example is for anonymous record `{age, name}`, named record also can coerce a record
having unnamed fields.

```
type Person of {age, name}

{12, "John"} as Person
```

In above snippet, the type will be instantiated as `Person(int, str)` (`Person{age: int, name: str}`).

Additionally, above named record coercing can be written with shorter special form as below:

```
Person{12, "John"}
```

This is useful when constructing an instance of named record.

# Pattern matching

Pattern matching is available with `match` statement/expression.

```
!! Normal record
person := {age: 12, name: "John"}

match person
case {age, "John"}
    print("Hi, John. You are "); print(age); print(" years old")
case {20, name}
    print(name); print(", you are no longer a child!")
else
    print("hello "); print(person.name);
end

!! Unnamed fields (tuples)
person := {12, "John"}

match person
case {height, "John"}
case {12, nickname}
else
end
```

For normal records, field names in pattern of `match` MUST be the same as record definition.
In above first example, using `height` at the pattern of first `case` arm causes compilation error.

In above second example, the record has unnamed fields. Unnamed fields can be named as you like at pattern
of the `case` arm.

# Discussion Point

## Alternative Syntax with `rec`

```
rec Foo{age, name}

rec Foo
    age: int
    name: str
end

rec Number
case Integer{int}
case Rational{denominator, numerator}
case Complex
    real: float
    imaginary: float
end

rec Option
case Some{'a}
case None
end
```

## Should use `{` and `}` instead of `(` and `)`?

```
type Person of (age, name)

type Person of (
    age: int
    name: str
)

type Number of
    )ase Integer(int)
    case Rational(denominator, numerator)
    case Complex(
        real: float
        imaginary: float
    )

type Option of
    case Some('a)
    case None

!! Instantiation
Person(12, "John")
Person(age: 17, name: "Ken")

!! Anonymous record
(age: 12, name: "John")

!! Unamed fields
(12, "John")
```

- `(` and `)`
  - `(...)` is more tuple like. (+1)
  - Instantiation with `Foo(...)` is very similar to function call (-1)
    - But defining constructor function `Foo()` also looks good (rather than Foo{...} special coercion) (+1)
- `{` and `}`
  - `{...}` is more distinct than `(...)`. (`(expr)` is one expression rather than record) (+1)
  - If using `{...}` for other syntax, it may conflict. `{...}` is also useful for representing 'region' (-1)
    - I want to use `{...}` for dictionary literal... (-1)

---
[Top](./README.md)
