## LUCA - CPP


### Prerequisites

- C++23 compiler
- CMake 3.20 or higher - A cross-platform build system generator.
- [vcpkg](https://github.com/microsoft/vcpkg) - A C++ package manager to install dependencies.  
  Set environment variable `VCPKG_ROOT` to the vcpkg installation path.
- re2c - Regular Expressions to Code.  
  (For Windows) You can install it via Chocolatey: `choco install re2c` or download it from [re2c.org](https://re2c.org/).

###  Building the Project

For msvc debugging, you can use the following commands:
```
cd src
cmake --preset vsdbg
cmake --build --preset vsdbg
```

### Language Reference
For the full syntax reference, see [Language Reference](src/readme.md).

