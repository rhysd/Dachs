# Constants

## Primitive Constants

```
!! int
42

!! uint
42u

!! bool
true
false

!! float
3.14
3.0e10
3.0e+10
3.0e-10
3e-10
3e+4

!! binary
0b01010011

!! hexadecimal
0xc0ffee

!! string
"This is string"
```

# Arithmetic Operators

```
!! For numbers

-42
1 + 2
1 - 2
1 * 2
1 / 2
1 % 2

-42u
1u + 2u
1u - 2u
1u * 2u
1u / 2u
1u % 2u

-3.14u
1.0 + 2.0
1.0 - 2.0
1.0 * 2.0
1.0 / 2.0
```

# Relational/Logical Operators

```
!! Compare two values
1 == 1
1 != 2
1 < 2
1 <= 2
2 > 1
2 >= 1

!! Logical operators
true && true
false || true
!false
```

# Bitwise Operators

TBW

# Control Flows

`{block expression}` means zero or more statements followed by an expression (`[{statement} ...] {expression}`)

## Branch

```
if {cond} then {block expression} else {block expression} end

if {cond}
    {block expression}
else
    {block expression}
end
```

Being different from `if` statement, it must have `else` clause. Both `then` and `else` clauses
must end with an expression and they must be the same type. `if` expression is evaluated to the
last expression of selected branch.

There is no `elseif` clause because you should use switch expression described below in the case.

## Switch

Test each `case` clause's `{cond}` expression and execute the block of the clause whose `{cond}`
is evaluated to `true`. All `case` arms' last expressions must be the same type.

```
case {cond} then {block expression}
case {cond}
    {block expression}
else
    {block expression}
end
```

`else` is required.

## Match

`match` expression selectively executes matched `case` clause and evalueted to the last expression
of the clause. All `case` arms' last expressions must be the same type.

```
match {expression}
case {pattern} then {block expression}
case {pattern}
    {block expression}
else
    {block expression}
end
```

`else` is required. (TODO: check exhausitivity of patterns)

# Type Assertion

```
{expression} : {type}
```

Declare `{expression}` is typed as `{type}`. It is referred by type inference. It is a hint for
typing an expression and will be checked by compiler. For example, `42 : int` is OK but
`3.14 : int` causes compilation error.

# Array Operations

## Literal

TBW

## Access to Element

TBW

## Make a Slice

TBW

# Dictionary Operations

TBW

# String Operations

## String Literal

TBW (raw string, multi-line string, escape characters...)

## String Interpolation

TBW

## Substring

TBW

## Concatenation

TBW

---
[Top](./README.md)
