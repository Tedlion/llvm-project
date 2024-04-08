# About Class Wrapper
Class Wrapper is designed to be an independent tool to wrap all code of a build target with one single class. So that multiple objects of the class can be created without copying the text section. Furthermore, functions and variables of different objects can be visited with the universal interface. 

## Design Details
### Input Multiple CompilationDatabase

### Scanning Once
Find all TypeDecl, FunctionDecl, and VarDecl.

One map for each target, record (**name**(as key), src_location(for replacement), hash(to check consistency)) 

Decide the namespace for each TypeDecl.

Decide the class each function and variable should be in.

Generate new headers and sources(by apply replacements) for each target.

### Recoverable
Unrecoverable? Try compiling first?

### Std Lib Functions
Functions declared in system headers should never be wrapped. How to distinguish those functions?

---> Check the path of header file where the functions declare.
If the function is not declared in user dir, it supposed to be std lib functions, and should not be wrapped.

### Should source files be parsed as C++?
#### Pros:
- Some platform-dependent functions are implemented with c++ std lib, such as functions in n_atomic.h.
#### Cons:
- We need to change compile command from the compilation database.
- Some casting is not allowed in C++, "-fpermissive" is required to avoid compiling errors.
#### Current Solution:
- Parse all source files as C.
- Regard n_atomic.h as system header, so that functions declared in it will not be wrapped or removed.

### Math Functions
Some functions do not rely on status.

Make them global, or static?

Even being non-static seems OK, which may lose some efficiency.

### Type:
in namespace
### Global/Static Variable:
class member
### Global/Static Function:
class member
### Function Body:
static variable in body -> class member

others -> nothing to do

### Class Define Contains:
All global symbols.

### Macros:
Replace macros which define/declare global symbols.

Leave others unchanged for readability.

### Conflicting Symbols:
- Treat all type and non-static symbol conflicting as error.
- Record all static symbol usages. If a conflict is found later, declarations and usages need to be renamed.
- Record all function ptrs usages. They need to be replaced with member functions. 


### Function Ptrs:


### Replacements to apply:
#### Generate New Headers:
- DutIn.h
```cpp
#ifndef DUTIN_H
#define DUTIN_H
namespace test_plat::dut {
struct common_type {
  ...
};

class DUT {
public:
  void common_func(common_type arg);

  virtual void func_same_inf_differnet_impl(common_type arg) = 0;
};
};  // namespace test_plat::dut
#endif
```

- CCOIn.h
```cpp
#ifndef CCOIN_H
#define CCOIN_H
#include "DutIn.h"
namespace test_plat::dut::cco{
struct type1{
    ...
};

struct type2 {
    ...
};
class CCO : public DUT{
public:
    void func_same_inf_differnet_impl(common_type arg) override;

    void func1(type1 arg);
    
    void func2(type2 arg);
};
};  // namespace test_plat::dut::cco
#endif
```

- STAIn.h
```cpp
#ifndef STAIN_H
#define STAIN_H
#include "DutIn.h"
namespace test_plat::dut::sta{
struct type1{
    ...
};

struct type3 {
    ...
};

class STA : public DUT{
public:
    void func_same_inf_differnet_impl(common_type arg) override;

    void func1(type1 arg);
    
    void func3(type3 arg);
};
};  // test_plat::dut::sta
```
- Pre-compiled headers when compiling the transferred project. (Not the function of this tool)


#### For Headers:
- Delete all declarations, include type, function and variable declarations.
- What's remaining? Macros, anything else?
- Move inline function definitions to new headers.

#### For Sources:
- Include new headers and using namespace at beginning
- Delete all declarations, include type, function and variable declarations.
- Add class name before all function definitions.

### Check Code Consistency _(Advanced)_:
If a type is completely same among all module types, make it in common namespace.

If a variable/function is completely same among all module types, make it in base class.

If a function's declaration is same among all module types, make it **pure** and **virtual** in base class.

How to judge the same?

## TODO Lists:
- [x] Accept multiple compilation database argument in command line
- [ ] Scan files for each database, record all declarations
