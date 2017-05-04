## Function Definition

```
func {name}({args}...)[: {return type}]
    {statements}
end
```

e.g.

```
func add_with_print(a: int, b: int): int
    let c = a + b
    print(c)
    ret c
end

func add_with_print(a, b)
    let c = a + b
    print(c)
    ret c
end
```

Parameter types and return type can be omitted.
Type-omitted parameters or return type will be type parameter and they will be inferred by type
inference from usage of the function.

TBW

## Applying Function

```
{function}({args})

{function}({args}) do {args}
    {sequence expression}
end
```

Body of `do ... end` block is a sequence expression like as body of `if` expression. So it can contain
zero or more statements and the last expression will be returned as the value of the block.

e.g.

```
!! Normal case
f(a, b, c)

!! Call function with block like Ruby
sort([{1, 5}, {2, 6}, {3, 4}]) do l, r
    let x = l.0 + l.1
    let y = r.0 + r.1
    x - y
end

!! For one-line usage, use lambda expression.
sort([1, 2, 3, 4, 5], -> l, r in l - r)
```

TBW

## Lambda

Lambda expression always starts with `->`.

```
!! One-line (expression only)
-> {args} in {expression}
-> {expression}

!! Multi-line (statements)
-> {args} do
    {statements}
end
-> do
    {statements}
end
```

TBW

---
[Top](./README.md)
