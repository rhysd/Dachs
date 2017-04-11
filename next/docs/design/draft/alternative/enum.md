# Syntax

Variant type represents contains one of several possible variants. The syntax is based on type alias.

## Definition

`enum` keyword is used for defining enum types.

e.g.

```
enum Number
case Integer(int)
case Rational{denom, num}
case Complex
    real: float
    imag: float
end
```

Above example defines variant type `Number`. It can contain one of type `Integer`, `Rational` or
`Complex` value.

`case` arm defines possible type with record syntax or tuple syntax. `(...)` means
[tuple type](tuple.md) and `{...}` means [record type](record.md).

```
!! Record containing two fields 'foo' and 'bar'
case Blah{foo: int, bar: str}

!! Record containing two fields 'foo' and 'bar'. Types are not specified
case Blah{foo, bar}

!! Tuple containing two elements whose types are int and str
case Blah(int, str)

!! Tuple containing two elements whose types are generic (type variable)
case Blah('a, 'b)
```

## Literal

Instance of variant type can be made with `{Type}::{SubType}` syntax. `Type` is a defined enum type.
`SubType` is one of possible types under the `Type` enum type.

```
i := Number::Integer(42)
r := Number::Rational{denom: 3, num: 10}
c := Number::Complex{real: 3.0, imag: 2.0}
```

# Pattern Matching

`match` statement/expression is available pattern matching of enum type values.

```
i := Number::Integer(42)

match i
case Integer(i)
    print(i)
case Rational{denom, num}
    print(denom); print("/"); print(num)
case Complex{real, imag}
    print(real); print(" i"); print(imag)
end
```

Almost the same as record and tuple values pattern matching. Subtypes (`Integer`, `Rational` or
`Complex` in above example) are required in `case` arm. `Type::` prefix is not necessary here.

---
[Top](./README.md)
