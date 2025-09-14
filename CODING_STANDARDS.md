# DISMv2 Coding Standards and Best Practices

This document outlines the coding standards and best practices for the DISMv2 Universal Windows Package Manager project.

## General Principles

1. **Code Clarity** - Write code that is easy to read and understand
2. **Consistency** - Follow established patterns throughout the codebase
3. **Safety** - Use modern C++ features to prevent common errors
4. **Performance** - Write efficient code while maintaining readability
5. **Maintainability** - Structure code for easy modification and extension

## C++ Standards

### Language Version
- Use **C++20** features throughout the project
- Prefer `std::filesystem` over platform-specific file operations
- Use `std::string_view` for string parameters when appropriate
- Leverage `std::optional` for nullable values

### Memory Management
- Use **RAII** (Resource Acquisition Is Initialization) principles
- Prefer `std::unique_ptr` and `std::shared_ptr` over raw pointers
- Use smart pointers for dynamic allocation
- Avoid manual `new`/`delete` calls

```cpp
// Preferred
{
    std::ifstream file(path, std::ios::binary);
    // File automatically closed when scope ends
}

// Avoid
FILE* file = fopen(path.c_str(), "rb");
// Manual cleanup required
fclose(file);
```

## Error Handling

### Exception Safety
- Use exceptions for exceptional conditions
- Provide strong exception safety guarantees where possible
- Use RAII to ensure cleanup in the presence of exceptions

### Error Reporting
- Use the `SimpleLogger` class for consistent error reporting
- Set meaningful error messages using `setLastError()`
- Validate inputs early and return descriptive error messages

```cpp
// Preferred
if (inputPath.empty()) {
    SimpleLogger::error("Input path cannot be empty");
    return false;
}

// Avoid
std::cerr << "Error\n";  // Non-descriptive error
```

## Constants and Magic Numbers

### Use Named Constants
- Define constants in appropriate namespaces
- Use `constexpr` for compile-time constants
- Group related constants together

```cpp
// Preferred
namespace FileSignatures {
    constexpr char CAB_SIGNATURE[] = "MSCF";
    constexpr size_t CAB_SIGNATURE_SIZE = 4;
}

// Avoid
if (memcmp(sig, "MSCF", 4) == 0)  // Magic numbers
```

## Naming Conventions

### Functions and Variables
- Use `camelCase` for functions and variables
- Use descriptive names that indicate purpose
- Avoid abbreviations unless they're widely understood

### Classes and Types
- Use `PascalCase` for class names
- Use descriptive names for custom types
- Prefix interfaces with 'I' if needed

### Constants and Enums
- Use `UPPER_CASE` for macro constants
- Use `PascalCase` for enum values
- Use `camelCase` for constexpr variables

## File Organization

### Headers
- Use `#pragma once` for header guards
- Include system headers before project headers
- Group includes logically and separate with blank lines

```cpp
#pragma once

// System headers
#include <string>
#include <vector>
#include <memory>

// Project headers
#include "CabHandler.h"
#include "Logger.h"
```

### Source Files
- Implement functions in the same order as declared in headers
- Group related functions together
- Use anonymous namespaces for internal helpers

## Logging and Debugging

### Use Structured Logging
- Use `SimpleLogger` for all log output
- Choose appropriate log levels (INFO, WARNING, ERROR, DEBUG)
- Remove debug output before production

```cpp
// Preferred
SimpleLogger::info("Processing file: " + filename);
SimpleLogger::warning("Non-standard signature detected");
SimpleLogger::error("Failed to open file: " + error);

// Avoid
std::cout << "DEBUG: something happened\n";  // Debug output in production
```

## Input Validation

### Validate Early
- Check function parameters at the beginning of functions
- Validate file paths and other inputs before use
- Provide meaningful error messages for validation failures

```cpp
bool processFile(const std::string& filePath) {
    // Validate inputs early
    if (filePath.empty()) {
        SimpleLogger::error("File path cannot be empty");
        return false;
    }
    
    if (!std::filesystem::exists(filePath)) {
        SimpleLogger::error("File does not exist: " + filePath);
        return false;
    }
    
    // Continue with processing...
}
```

## Platform-Specific Code

### Conditional Compilation
- Use `#ifdef` guards for platform-specific code
- Provide appropriate abstractions for cross-platform compatibility
- Keep platform-specific code isolated

```cpp
#ifdef _WIN32
    // Windows-specific implementation
#else
    // Generic or Linux implementation
#endif
```

## Testing

### Unit Test Guidelines
- Write tests for all public interfaces
- Test both success and failure cases
- Use descriptive test names
- Keep tests focused and independent

### Mock Objects
- Use mock implementations for external dependencies
- Provide test-only headers for cross-platform testing
- Keep mock interfaces minimal but functional

## Documentation

### Code Comments
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document complex algorithms and business logic
- Avoid obvious comments

```cpp
// Preferred
// Parse DISM-style parameters like /PackagePath:value
std::string parseParameter(const std::string& arg);

// Avoid
int i = 0;  // Initialize i to 0
```

### Function Documentation
- Document public API functions
- Explain parameters and return values
- Include usage examples for complex functions

## Performance Considerations

### File Operations
- Use buffered I/O for large files
- Minimize file system calls
- Use memory mapping for very large files when appropriate

### String Operations
- Use `std::string_view` for read-only string parameters
- Prefer `std::string::reserve()` for known sizes
- Avoid unnecessary string copies

### Memory Usage
- Use stack allocation when possible
- Be mindful of object lifetimes
- Profile memory usage for large operations

## Security Considerations

### Input Sanitization
- Validate all external inputs
- Check file paths for directory traversal attacks
- Validate file signatures before processing

### Error Information
- Don't leak sensitive information in error messages
- Log security-relevant events appropriately
- Handle authentication and authorization properly

## Code Review Checklist

Before submitting code, ensure:

- [ ] Code follows naming conventions
- [ ] Functions are reasonably sized and focused
- [ ] Error handling is appropriate and consistent
- [ ] Constants are used instead of magic numbers
- [ ] Memory management follows RAII principles
- [ ] Code is documented appropriately
- [ ] Unit tests cover new functionality
- [ ] No debug output remains in production code
- [ ] Platform-specific code is properly isolated
- [ ] Input validation is comprehensive

## Tools and Automation

### Recommended Tools
- **Visual Studio** - Primary development environment
- **g++** with C++20 support - For cross-platform testing
- **Makefile** - For automated Linux testing
- **Git** - Version control with proper .gitignore setup

### Build Process
- Use Visual Studio for Windows builds
- Use provided Makefile for Linux development testing
- Ensure clean builds before submission
- Test on both Debug and Release configurations

## Conclusion

These standards help ensure that the DISMv2 codebase remains maintainable, reliable, and professional. When in doubt, prioritize code clarity and safety over cleverness or brevity.