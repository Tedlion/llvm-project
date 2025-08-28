# About Class Wrapper
Class Wrapper is designed to be an independent tool to wrap all code of a build target with one single class. So that multiple objects of the class can be created without copying the text section. Furthermore, types, functions and variables of different objects can be accessed by the universal class members interface. 

# Design Details
## Input Multiple CompilationDatabase

## Scanning Each Source File
Find all TypeDecl, FunctionDecl, and VarDecl.

One map for each target, record (**name**(as key), src_location(for replacement), hash(to check consistency)) 

Decide the class for each TypeDecl.

Decide the class each function and variable should be in.

Generate new headers and sources(by apply replacements) for each target.

## Recoverable
Unrecoverable? Try compiling first?

## Std Lib Functions
Functions declared in system headers should never be wrapped. How to distinguish those functions?

---> Check the path of header file where the functions declare.
If the function is not declared in user dir, it supposed to be std lib functions, and should not be wrapped.

## Should source files be parsed as C++?
### Pros:
- Some platform-dependent functions are implemented with c++ std lib, such as functions in n_atomic.h.
### Cons:
- We need to change compile command from the compilation database.
- Some casting is not allowed in C++, "-fpermissive" is required to avoid compiling errors.
### Current Solution:
- Parse all source files as C.
- Regard n_atomic.h as system header, so that functions declared in it will not be wrapped or removed.

## Math Functions
Some functions do not rely on status.

Make them global, or static?

Even being non-static seems OK, which may lose some efficiency.

## Type:
in class
## Global/Static Variable:
class member
## Global/Static Function:
class member
## Function Body:
static variable in body -> class member

others -> nothing to do

## Class Define Contains:
All global symbols.

## Macros:
Replace macros which define/declare global symbols.

Leave others unchanged for readability.

## Conflicting Symbols:
- Treat all type and non-static symbol conflicting as error.
- Record all static symbol usages. If a conflict is found later, declarations and usages need to be renamed.
- Record all function ptrs usages. They need to be replaced with member functions. 


## Function Ptrs:


## Replacements to apply:
### Generate Modules:
- dut.cppm
```cpp
export module dut;

class DUT {
public:
  struct common_type {
    ...
  };
  
  void common_func(common_type arg);

  virtual void func_same_inf_differnet_impl(common_type arg) = 0;
};
```

- cco.cppm
```cpp
export module dut.cco;
import dut;

class CCO : public DUT{
public:
  struct type1{
    ...
  };
  
  struct type2 {
    ...
  };

  void func_same_inf_differnet_impl(common_type arg) override;
  
  void func1(type1 arg);
  
  void func2(type2 arg);
};
```

- sta.cppm
```cpp
export module dut.sta;
import dut;

class STA : public DUT{
public:
  struct type1 {
    ...
  };
  
  struct type3 {
    ...
  };
  
  void func_same_inf_differnet_impl(common_type arg) override;

  void func1(type1 arg);
    
  void func3(type3 arg);
};
```


### For Headers:
- Delete all declarations, include type, function and variable declarations.
- What's remaining? Macros, anything else?
- Move inline function definitions to new modules.

### For Sources:
- Import modules at beginning
- Delete all declarations, including type, function and variable declarations.
- Add class name before all function definitions.

## Check Code Consistency _(Advanced)_:
If a type is completely same among all dut types, it is regarded to be **consistent**, and it is supposed to be put in common class.
To be more specific, a pointer type is considered to be consistent if and only if the pointed type is consistent. A struct is considered to be consistent if and only if all its fields, including **types and names**, are consistent.

A non-local variable is considered to be consistent if and only if its type and name is consistent. A consistent non-local variable is supposed to be put in base class.

A function's declaration is considered to be consistent if and only if all its return type, parameter types and names are consistent. A function's implementation is considered to be consistent if and only if its declaration is consistent and statements in its definition body are totally consistent, **including types and names of local variables**. An implementation-consistent function is supposed to be put in base class. Otherwise, if the function is only declaration-consistent, it is supposed to be **pure** and **virtual** in base class and actually implemented in derived class.

### Hash function design:
#### Problems with clang::ODRHash:
The hash value for RecordDecl Type is not calculated. i.e., if struct A is inconsistent, the hash value of a struct contains a field of struct A may have same hash value among all module types. Similar problems may happen to functions.

#### Solutions:
We judge two symbols are consistent if and only if the following two conditions are satisfied simultaneously: 
- Their proprocesser result (after macro replacement) is exactly the same.
- All symbols they relie on are consistent.

## Dependency Chains:
### Why necessary?
- RecordDecls must be defined in dependency orders
- Solve the insufficient visiting problem of ODRHash RecordDecl 
### Record what?
#### RecordDecls:
- non-built-in types of all fields
#### VarDecls (not local):
- the type if it is not built-in
- variables and non-built-in types in the initialization expression (if exists)
#### FunctionDecls (not the definition body):
- non-built-in types of all parameters and return type.
#### FunctionDecls (definition body):
- All RecordDecls ref in the function body
- All non-local var ref
- All functions ref
### How to record?
- Some of RecordsDecls/global variables maybe not defined.
- Set of (Names, Source Range)?

## Scanning From the Source and Preprocessed:

| Kind       | Typedef | Record | Enum  | VarDecl | FuncDecl(in .h)  | FuncDecl(in .c)  |
|------------|---------|--------|-------|---------|------------------|------------------|
| Name       | S       | S      | -     | S       | S                | S                |
| Path       | S, PP   | S, PP  | S, PP | S, PP   | S, PP            | S, PP            |
| FullRange  | S, PP   | S, PP  | S, PP | S, PP   | S, PP            | S, PP            |
| InfHash    | -       | -      | -     | -       | PP               | PP               |
| ImplHash   | PP      | PP     | PP    | PP      | PP (for def)     | PP (for def)     |
| ToRemove   | Y       | Y      | Y     | Y       | Y                | N (except macro) |
| AddToClass | Y       | Y      | Y     | Y       | Y (include body) | N (except body)  |

## TODO Lists:
- [x] Accept multiple compilation database argument in command line
- [ ] Scan files for each database, record all declarations
- [ ] Design hash algorithm for RecordDecl to make the hash values equal if and only if when the struct layout is totally same
