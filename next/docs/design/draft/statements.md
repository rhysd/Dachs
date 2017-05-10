# Variable Definition

```
let {names} = {expression}

var {names} = {expression}
```

e.g.

```
!! Define 'i' containing int value
let i = 42
print(i)

var s = "foo"
```

When `let` is specified to the definition, the variable will be immutable.
And when `var` is specified to the definition, the variable will be mutable.

## Destructuring

From fields of [record](record.md), variables can be defined with destructuring like JavaScript.

```
let {...} = some_record_value

var {...} = some_record_value
```

e.g.

```
!! From a record which has unnamed fields (like tuple)
let r = {1, true, "aaa"}
let {i, b, s} = r
print(i); print(b); print(s)

!! From a record. Field names must be the same as RHS's ones.
let r2 = {age: 12, name: "John"}
let {age, name} = r2
print(age); print(name)

!! Match with names. So below is also OK.
let {name, age} = r2

!! Named record
type Person = {name, age}
let p = Person("John", 12)
let {age, name} = p
print(age); print(name)
```

If the record at right hand side doesn't have unnamed fields, the names of destructured variables
MUST have the same name as corresponding fields. For example, `let {foo} = {foo: 42}` is OK but
`let {bar} = {foo: 42}` is not OK because of field name mismatch (`foo` v.s. `bar`).

If the record at right hand side has unnamed fields, the names of destructured variables can be
named as you like.

```
let r = {_: 42, _: "foo"}

let {foo, bar} = r
let {f, b} = r
```

Unnamed field is a field which is not named *yet*. So you can set its name freely.

Destructuring can be nested. In the case, defined variables' names also must match to
the corresponding fields.

```
let r = {a: 1, {b: 3.14, c: "aaa"}}

let {a, {b, c}} = r
```

Instead of JavaScript, array cannot be used for destructuring (`[x, y] = [1, 2]`) because length of
array is determined at runtime. Compiler cannot check the length at compile time.

Properties not appearing in destructuring are ignored.

```
!! 'c' property is ignored
let {a, b} = {a: 42, b: "a", c: 3.14}
```

# Variable Assignment

```
{names} = {expression}
```

e.g.

```
var i = 42
i = 21
```

Assign RHS value to variables specified at LHS. If there is multiple names at LHS, RHS must be
record which has the same number of fields as names at LHS.

Since it changes the value(s) contained in variable(s), the variables must have been defined as
mutable with `var` keyword before.

# Return

```
ret {expression}[, {expressions...}]
```

e.g.

```
func fib(i) {
    if i <= 1
        ret 1
    end
    ret fib(i - 1) + fib(i - 2)
}

func triple(i) {
    return i, i * 2, i * 3
}

let ten, twenty, thirty = triple(10)
```

`ret` returns value(s) from function. When multiple expressions are specified, They will be
returned as record.

# Control Flows

## Branch

```
if {cond} then {statements} else {statements} end
if {cond}
    {statements}
else
    {statements}
end

if {cond} then {statements} end
if {cond}
    {statements}
end
```

e.g.

```
if ok(something)
    print("ok")
end
```

`if` statement. `else` can be omitted.

## Switch

```
switch
case {cond} then {statements}
case {cond}
    {statements}
else
    {statements}
end
```

e.g.

```
switch
case i % 15 == 0
    print("fizzbuzz")
case i % 3  == 0
    print("fizz")
case i % 5  == 0
    print("buzz")
else
    print(i)
end
```

Switch statement to execute one of `case` clauses selectively. `else` can be omitted.

## Match

```
match {expression}
case {pattern} then {statements}
case {pattern}
    {statements}
else
    {statements}
end
```

e.g.

```
match status
case 200 then print("ok")
case 404 then print("not found")
else          print("???")
end

type Person =
    case Student(age)
    case Engineer(skill)
    case Baby

// let person = ...

match person
case Student(name, _)
    print("Student"); print(age)
case Engineer(skill)
    print("Engineer"); print(skill)
case Baby
    print("Hello, world")
else
    print("who are you?")
end
```

`match` statement to execute one of `case` clauses selectively matching to each patterns.
`else` clause can be omitted.

## Loop

```
for {cond}
    {statements}
end

for {name} in {array expression}
    {statements}
end
```

e.g.

```
for true
    print("unfinite loop!!")
end

for elem in [1, 2, 3, 4]
    print(elem)
end
```

First form is a 'while' loop and second is a 'foreach' loop.

### Break loop

`break` statement is used for the purpose

```
var i = 0
for true
    if i > 10
        break
    end
    i = i + 1
end
```

### Continue loop

`next` statement is used for the purpose

```
var i = 0
for i < 10
    i = i + 1
    if i % 2 != 0
        next
    end
    print(i)
end
```

# Discussion Point

## Arm of `match` statement

Currently `match` statement uses `case` as keyword to define its arms. But it's also used for `switch` statement (`case {cond} then ... case {cond} then ... else ... end`). Should we use `case` also for `match`'s arms?

It's confusing to use the same `case` keyword for similar syntax if there is a big `match` statement. I'm considering `with` for alternative.

```
match i
with 42 then ...
with 21
  ...
else
  ...
end
```

---
[Top](./README.md)
