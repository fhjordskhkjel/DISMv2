# DISMv2 Testing Infrastructure

This directory contains unit tests and testing infrastructure for the DISMv2 Universal Windows Package Manager.

## Structure

- `unit_tests.cpp` - Core unit test suite
- `CabHandler_test.h` - Test-only header with simplified interface for cross-platform testing
- `mock_cabhandler.cpp` - Mock implementation for Linux testing
- `Makefile` - Build system for Linux development testing

## Running Tests

### On Linux (Development Testing)

```bash
# Build and run all tests
make test

# Build tests only
make all

# Clean build artifacts
make clean

# Show available targets
make help
```

### On Windows (Production Environment)

The main project should be built using Visual Studio. The existing batch files in the parent directory provide integration testing:

- `comprehensive_validation_test.bat` - Complete feature validation
- `test_enhanced_features.bat` - Enhanced functionality tests
- `test_cbs_integration.bat` - CBS integration tests

## Test Coverage

The unit tests cover:

1. **SimpleLogger functionality** - Message formatting and level handling
2. **File signature detection** - CAB, ZIP, 7z, GZIP signature validation
3. **CabHandler basic operations** - Error handling, state management
4. **File validation logic** - File existence and type checking
5. **Edge cases** - Error conditions and boundary cases

## Adding New Tests

To add new unit tests:

1. Add test functions following the pattern `test_feature_name()`
2. Use `SimpleTestFramework::test_assert()` for assertions
3. Call your test function from `main()`
4. Update this README with new test coverage

## Architecture Notes

The testing infrastructure uses a mock/stub approach to enable cross-platform development:

- **Windows-specific APIs** (fci.h, fdi.h, windows.h) are abstracted out
- **Core logic** is testable on Linux using mock implementations
- **Integration testing** remains Windows-specific using the actual APIs

This allows developers to:
- Run quick unit tests on any platform
- Validate core logic without Windows dependencies
- Perform full integration testing on Windows systems

## Best Practices

1. **Keep tests focused** - Each test should validate one specific behavior
2. **Use descriptive names** - Test names should clearly indicate what is being tested
3. **Validate both success and failure cases** - Test happy path and error conditions
4. **Mock external dependencies** - Keep unit tests isolated from system dependencies
5. **Update tests when code changes** - Maintain test coverage as features evolve