# About Class Wrapper
Class Wrapper is designed to be an independent tool to wrap all code of a build target with one single class. So that multiple objects of the class can be created without copying the text section. Furthermore, functions and variables of different objects can be visited with the universal interface. 

## Design Details
### Input Multiple CompilationDatabase

### Scanning Once
Find all TypeDecl, FunctionDecl, and VarDecl.

One map for each target, record (**name**(as key), src_location, hash) 

Decide the namespace for each TypeDecl.

Decide the namespace for each function and variable.

Generate new headers and sources(by apply replacements) for each target.

### Recoverable
Unrecoverable? Try compiling first?

### Std Lib Functions
Functions declared in system headers should never be wrapped. How to distinguish those functions?

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

### Headers:
Pre-compile

### Check Code Consistency _(Advanced)_:
If a type is completely same among all module types, make it in common namespace.

If a variable/function is completely same among all module types, make it in base class.

If a function's declaration is same among all module types, make it **pure** and **virtual** in base class.

How to judge the same?