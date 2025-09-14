#include <iostream>
#include <cassert>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include "CabHandler_test.h"

// Simple test framework
namespace SimpleTestFramework {
    static int tests_run = 0;
    static int tests_passed = 0;
    
    void test_assert(bool condition, const std::string& test_name) {
        tests_run++;
        if (condition) {
            tests_passed++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            std::cout << "[FAIL] " << test_name << std::endl;
        }
    }
    
    void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests passed: " << tests_passed << std::endl;
        std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
        std::cout << "Success rate: " << (tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0) << "%" << std::endl;
    }
    
    bool all_passed() {
        return tests_run == tests_passed;
    }
}

namespace fs = std::filesystem;

// Test the SimpleLogger functionality
void test_simple_logger() {
    std::cout << "\n=== Testing SimpleLogger ===" << std::endl;
    
    // Redirect cout to capture output
    std::ostringstream captured_output;
    std::streambuf* original_cout = std::cout.rdbuf();
    std::cout.rdbuf(captured_output.rdbuf());
    
    SimpleLogger::info("Test info message");
    SimpleLogger::warning("Test warning message");
    SimpleLogger::debug("Test debug message");
    
    // Restore cout
    std::cout.rdbuf(original_cout);
    
    std::string output = captured_output.str();
    
    SimpleTestFramework::test_assert(
        output.find("[INFO] Test info message") != std::string::npos,
        "SimpleLogger info message format"
    );
    
    SimpleTestFramework::test_assert(
        output.find("[WARN] Test warning message") != std::string::npos,
        "SimpleLogger warning message format"
    );
    
    SimpleTestFramework::test_assert(
        output.find("[DEBUG] Test debug message") != std::string::npos,
        "SimpleLogger debug message format"
    );
}

// Test file signature constants
void test_file_signatures() {
    std::cout << "\n=== Testing File Signatures ===" << std::endl;
    
    SimpleTestFramework::test_assert(
        FileSignatures::CAB_SIGNATURE_SIZE == 4,
        "CAB signature size is correct"
    );
    
    SimpleTestFramework::test_assert(
        std::string(FileSignatures::CAB_SIGNATURE) == "MSCF",
        "CAB signature value is correct"
    );
    
    SimpleTestFramework::test_assert(
        FileSignatures::SIGNATURE_SIZE == 8,
        "Signature buffer size is correct"
    );
    
    SimpleTestFramework::test_assert(
        std::string(FileSignatures::ZIP_SIGNATURE) == "PK",
        "ZIP signature value is correct"
    );
}

// Test CabHandler basic functionality
void test_cab_handler_basic() {
    std::cout << "\n=== Testing CabHandler Basic Functionality ===" << std::endl;
    
    CabHandler handler;
    
    // Test initial state
    SimpleTestFramework::test_assert(
        handler.getLastError().empty(),
        "CabHandler initial error state is empty"
    );
    
    // Test error handling
    handler.setLastError("Test error message");
    SimpleTestFramework::test_assert(
        handler.getLastError() == "Test error message",
        "CabHandler error setting and getting"
    );
}

// Test file validation logic
void test_file_validation() {
    std::cout << "\n=== Testing File Validation ===" << std::endl;
    
    // Create a temporary test file for validation
    std::string temp_dir = "/tmp/dismv2_test";
    std::string test_file = temp_dir + "/test_file.txt";
    
    try {
        // Create test directory
        fs::create_directories(temp_dir);
        
        // Create test file
        std::ofstream test_stream(test_file);
        test_stream << "Test content";
        test_stream.close();
        
        SimpleTestFramework::test_assert(
            fs::exists(test_file),
            "Test file creation successful"
        );
        
        SimpleTestFramework::test_assert(
            fs::is_regular_file(test_file),
            "Test file is recognized as regular file"
        );
        
        // Clean up
        fs::remove(test_file);
        fs::remove(temp_dir);
        
    } catch (const std::exception& ex) {
        std::cout << "File validation test error: " << ex.what() << std::endl;
        SimpleTestFramework::test_assert(false, "File validation test setup");
    }
}

// Test signature detection logic
void test_signature_detection() {
    std::cout << "\n=== Testing Signature Detection ===" << std::endl;
    
    // Test CAB signature detection
    char cab_sig[8] = {'M', 'S', 'C', 'F', 0, 0, 0, 0};
    bool is_cab = (memcmp(cab_sig, FileSignatures::CAB_SIGNATURE, FileSignatures::CAB_SIGNATURE_SIZE) == 0);
    
    SimpleTestFramework::test_assert(is_cab, "CAB signature detection");
    
    // Test ZIP signature detection
    char zip_sig[8] = {'P', 'K', 0, 0, 0, 0, 0, 0};
    bool is_zip = (memcmp(zip_sig, FileSignatures::ZIP_SIGNATURE, FileSignatures::ZIP_SIGNATURE_SIZE) == 0);
    
    SimpleTestFramework::test_assert(is_zip, "ZIP signature detection");
    
    // Test invalid signature
    char invalid_sig[8] = {'X', 'Y', 'Z', 'W', 0, 0, 0, 0};
    bool is_invalid_cab = (memcmp(invalid_sig, FileSignatures::CAB_SIGNATURE, FileSignatures::CAB_SIGNATURE_SIZE) == 0);
    
    SimpleTestFramework::test_assert(!is_invalid_cab, "Invalid signature rejection");
}

// Test edge cases and error conditions
void test_edge_cases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;
    
    CabHandler handler;
    
    // Test extraction with non-existent file
    bool result = handler.extractCab("/nonexistent/file.cab", "/tmp");
    SimpleTestFramework::test_assert(
        !result,
        "Extraction fails gracefully with non-existent file"
    );
    
    SimpleTestFramework::test_assert(
        !handler.getLastError().empty(),
        "Error message is set for non-existent file"
    );
    
    // Test extraction with non-existent destination
    // This should fail gracefully
    result = handler.extractCab("/tmp/test.cab", "/nonexistent/destination");
    SimpleTestFramework::test_assert(
        !result,
        "Extraction fails gracefully with non-existent destination"
    );
}

int main() {
    std::cout << "DISMv2 Unit Test Suite" << std::endl;
    std::cout << "======================" << std::endl;
    
    try {
        test_simple_logger();
        test_file_signatures();
        test_cab_handler_basic();
        test_file_validation();
        test_signature_detection();
        test_edge_cases();
        
        SimpleTestFramework::print_summary();
        
        return SimpleTestFramework::all_passed() ? 0 : 1;
        
    } catch (const std::exception& ex) {
        std::cerr << "Test suite error: " << ex.what() << std::endl;
        return 1;
    }
}