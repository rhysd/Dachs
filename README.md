![Dachs Programming Language](misc/dachs-logo.jpg)

[![Build Status](https://travis-ci.org/rhysd/Dachs.svg?branch=master)](https://travis-ci.org/rhysd/Dachs)
=========================================================================================================

Goals :dog2:
- Light to write
- Strongly and statically typed
- Native code efficiency
- Full OOP
- Immutability-aware
- Dog-friendly

<pre>
<i># Type of parameter and returned value can be inferred</i>
<b>func</b> abs(n)
    <b>ret</b> (<b>if</b> n > 0.0 <b>then</b> n <b>else</b> -n)
<b>end</b>

<i># Parameters and variables are defined as immutable value by default</i>
<b>func</b> sqrt(x)
    <b>var</b> z, <b>var</b>p := x, 0.0   <i># but 'var' is available to declare a mutable variable</i>
    <b>for</b> abs(p-z) > 0.00001
        p, z = z, z-(z*z-x)/(2*z)
    <b>end</b>
    <b>ret</b> z
<b>end</b>

<b>func</b> main
    print(sqrt(10.0))
<b>end</b>
</pre>

<!--
# Type of parameter and returned value can be inferred
func abs(n)
    ret (if n > 0.0 then n else -n)
end

# Parameters and variables are defined as immutable value by default
func sqrt(x)
    var z, p := x, 0.0   # but 'var' is available to declare a mutable variable
    for abs(p-z) > 0.00001
        p, z = z, z-(z*z-x)/(2*z)
    end
    ret z
end

func main
    print(sqrt(10.0))
end
-->

## Progress Report

- [x] Literals
- [x] Expressions
- [x] Statements
- [x] Functions
- [x] Operator functions
- [x] Overload resolution
- [x] Simple return type and variable type deduction
- [ ] Class
- [ ] Lambda
- [ ] Block
- [ ] Variadic arguments
- [ ] Module
- [x] Semantic checks
- [x] LLVM IR code generation
- [x] Executable generation
- [x] Test
- [x] CMakeLists.txt
- [x] Travis-CI
- [ ] Option parser
- [ ] Allocator customization
- [ ] Introduce [OvenToBoost](https://github.com/faithandbrave/OvenToBoost)

This software is disributed under [The MIT License](http://opensource.org/licenses/MIT) if not specified in a source file.

    Copyright (c) 2014 rhysd

This software uses [Boost C++ Libraries](http://www.boost.org/), which is licensed by [The Boost License](http://www.boost.org/users/license.html).

> Boost Software License - Version 1.0 - August 17th, 2003

This software uses [LLVM](http://llvm.org/), which is licensed by [University of Illinois/NCSA Open Source License](http://opensource.org/licenses/UoI-NCSA.php).

> Copyright (c) 2003-2014 University of Illinois at Urbana-Champaign
