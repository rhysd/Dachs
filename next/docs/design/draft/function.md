## Function Definition

```
func {name}({args}...)[: {return type}]
    {statements}
end
```

Example:

```
func add_with_print(a: int, b: int): int
    c := a + b
    print(c)
    ret c
end

func add_with_print(a, b)
    c := a + b
    print(c)
    ret c
end
```

Parameter types and return type can be omitted.
Type-omitted parameters or return type will be type parameter and they will be inferred by type
inference from usage of the function.

TBW

## Applying Function

TBW

## Lambda

TBW

---
[Top](./README.md)
