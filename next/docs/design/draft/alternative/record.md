# Syntax

Record is a compound type which contains zero or more fields. Field is a pair of name and its type.

## Anonymous Record Type

```
{field1[: type1], field2[: type2], ...}
```

Example:

```
!! It has two fields, age (typed as int) and name (typed as str).
{age: int, name: str}

!! Field name only
{age, name}

!! Partially typed
{age: int, name}

!! Nested
{name: str, {age: int, has_dog: bool}}
```

Record cannot be empty. `{}` represents empty dictionary. You can use unit type (empty tuple) for the purpose.

## Named Record Type

```
rec Name {field1[: type1], field2[: type2], ...}

rec {Name}
    field1[: type1]
    field2[: type2]
    ...
end
```

Example:

```
rec Person{age, name}

rec Person
    age: int
    name: str
end
```

## Literal

```
{field1: expr1, field2: expr2, ...}

Name{field1: expr1, field2: expr2, ...}
```

Example:

```
!! Record which has two fields (anonymous)
!! Type is {age: int, name: str}
{age: 12, name: "John"}

!! Record which has two fields (named)
rec Person{age, name}
Person{age: 12, name: "John"}
```

# Field Punning

Like ECMAScript, record literal `{foo: foo}` can be squash into `{foo}`.

```
rec Person{age, name}

age, name := 42, "John"

person := Person{age, name}

!! the same as
person := Person{age: age, name: name}
```

# Constructor Function

By defining named record with `rec`, constructor function is also defined implicitly.
Constructor function is a normal function for factory of the record.
You can make record instance by positional argument with it.

```
rec Person{age, name}

func Person(age: 'a, name: 'b)
    ret Person{age, name}
end

!! Invoke constructor function
person := Person(12, "John")
```

Constructor function has the same name as its record because namespace of types and namespace of
variables (including functions) are different.

# Pattern Matching

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
```

For normal records, field names in pattern of `match` MUST be the same as record definition.
In above first example, using `height` at the pattern of first `case` arm causes compilation error.

---
[Top](./README.md)
