![Dachs Programming Language](https://raw.githubusercontent.com/rhysd/Dachs/master/misc/dachs-logo.jpg)

[![Build Status](https://travis-ci.org/rhysd/Dachs.svg?branch=master)](https://travis-ci.org/rhysd/Dachs)
=========================================================================================================

Goals :dog2: :
- Light to write
- Strongly and statically typed
- Native code efficiency
- Full OOP
- Immutability-aware
- Dog-friendly

```
import std.io as io

# Type of parameter and returned value can be inferred
func abs(n)
    return if n > 0.0 then n else -n
end

# Parameters and variables are defined as immutable value by default
func sqrt(x)
    var z, p := x, 0.0   # but 'var' is available to declare a mutable variable
    for abs(p-z) > 0.00001
        p, z = z, z-(z*z-x)/(2*z)
    end
    return z
end

func main()
    io.println(sqrt(10.0))
end
```

## Progress Report

-  Parser
  - [x] Literals
  - [x] Expressions
  - [x] Statements
  - [x] Functions
  - [x] Operator functions
  - [ ] Class
  - [ ] Lambda
  - [ ] Block
  - [ ] Variadic arguments
  - [ ] Module
  - [ ] Test

-  Sematic analysis
  - [x] Scope tree
  - [x] Symbol tables
  - [ ] Overload resolution
  - [ ] Type inference
  - [ ] Semantic checks
  - [ ] Test

-  Code generation
  - [ ] LLVM IR code generation
  - [ ] Test

-  Misc
  - [x] CMakeLists.txt
  - [x] Travis-CI
  - [ ] Option parsing
  - [ ] Allocator customization
  - [ ] Exception safety
  - [ ] Parallel processing

This software is disributed under [The MIT License](http://opensource.org/licenses/MIT).

    Copyright (c) 2014 rhysd

This software uses [Boost C++ Libraries](http://www.boost.org/), which is licensed by [The Boost License](http://www.boost.org/users/license.html).

    Boost Software License - Version 1.0 - August 17th, 2003
