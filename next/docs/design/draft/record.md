Record is a compound type which contains zero or more fields. Field is a pair of name and its type.

# Anonymous Record

## Definition

```
{field1[: Type1], field2[: Type2], ...}
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

Syntax for creating anonymous record value is like

```
{field1: expr1, field2: expr2, ...}
```

Example:

```
!! Empty
{}

!! Record which has two fields (anonymous)
{age: 12, name: "John"}
```

# Named Record

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

## Literal

Named record value is made like

```
type Person of {age, name}

Person{age: 12, name: "John"}
```

As the feature of `type ... of`, it defines constructor function implicitly.

```
type Person of {age, name}

func Person(age: 'a, name: 'b)
    ret {age: age, name: name}
end
```

It's not a special function and enable to make instance of `Person` without field names.

```
Person(12, "John")

!! Above is the same as below
Person{age: 12, name: "John"}
```

# Unnamed Field

Generally, field has its name. But Dachs allows field not to have a name, called unnamed field.
To define an unnamed field, specify `_` as field name. It means 'not named yet'.

Currently both named field and unnamed field cannot live in the same record. If a record has
an unnamed record, all records must be unnamed in the record.

```
!! First field is int, second field is str. But they don't have name yet.
{_: int, _: str}

!! Types are omitted. It has two fields.
{_, _}
```

Unnamed field doesn't have its name and field in record is accessed via its name. So unnamed field
cannot be accessed at first. You need to give a name to the field to access.
To give a name to unnamed field, you need to use pattern matching or destructuring.

```
r := {_: 12, _: "John"}

match r
case {12, name}
    print(name)
case {age, _}
    print(age)
end

{age, name} := r
print(age)
print(name)
```

Unnamed field is used for enum type (please see [types.md](types.md) for more detail of enum type).
This is used to name its field at pattern in `match` expression (or statement).

For example, `Option` type can be written as below.

```
!! Option::Some has one unnamed field.
type Option of
    case Some{_}
    case None

!! Type is Option{_: int}.
!! Use constructor function to make a value easily.
some := Some(42)

!! Unnamed field can firstly access in the case arm
match some
case Option::Some{answer}
    print(answer)
case Option::None
    print("no answer!")
end
```

Here, `Some` has one unmaed field. The field name 'answer' is determined at `case` arm at `match`
statement.

This is useful for the data structure which contains any value (field name should be changed
depending on what is contained).

## Instantiation

For records which have unnamed fields,

```
pair := {_: 12, _: "John"}
```

Named record which has unnamed fields can be made with constructor function.

```
type Pair of {_: int, _: str}

pair := Pair(12, "John")

type Option of
    case Some{_}
    case None

some := Option::Some(42)
!! Instead of Option::Some{_: 42}
```

# Field Name Punning

Like JavaScript's property punning, field of record at literal can be punned. It means that
`{foo: foo}` can be squashed into `{foo}`.

```
age, name := 12, "John"

person := {age, name}

!! This is the same as {age: age, name: name}
!! Type of person is {age: int, name: str}
```

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
person := {_: 12, _: "John"}

match person
case {height, "John"}
case {12, nickname}
else
end
```

For normal records, field names in pattern of `match` MUST be the same as record definition.
In above first example, using `height` at the pattern of first `case` arm causes compilation error.

In above second example, the record has unnamed fields. Unnamed fields can be named as you like
at pattern of the `case` arm.

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
