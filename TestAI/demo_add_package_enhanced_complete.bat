@echo off
echo ========================================
echo  ?? Enhanced Package Addition Complete Demo
echo  CBS Integration + /ExtractedDir Support
echo ========================================
echo.

echo Testing all add-package-enhanced functionality...
echo.

echo ========================================
echo 1. BASIC FUNCTIONALITY TESTS
echo ========================================
echo.

echo Test 1a: Show enhanced help
echo ----------------------------
DISMv2.exe add-package-enhanced
echo.

echo Test 1b: Basic package addition
echo --------------------------------
DISMv2.exe add-package-enhanced "test-package.msu" --dry-run
echo.

echo Test 1c: Package with security validation
echo ------------------------------------------
DISMv2.exe add-package-enhanced "security-update.msu" --security-validation --dry-run
echo.

echo ========================================
echo 2. EXTRACTED DIRECTORY TESTS
echo ========================================
echo.

echo Creating test extracted directory...
mkdir test-extracted-demo 2>nul
echo Test manifest > test-extracted-demo\update.mum
echo Test component > test-extracted-demo\component.xml
echo Test DLL > test-extracted-demo\testfile.dll
echo.

echo Test 2a: Basic extracted directory installation
echo ------------------------------------------------
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted-demo" --dry-run
echo.

echo Test 2b: Extracted directory with security validation
echo -------------------------------------------------------
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted-demo" --security-validation --dry-run
echo.

echo Test 2c: Extracted directory with force mode
echo ----------------------------------------------
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted-demo" --force --dry-run
echo.

echo ========================================
echo 3. CBS INTEGRATION TESTS
echo ========================================
echo.

echo Test 3a: CBS integration dry run (package)
echo -------------------------------------------
DISMv2.exe add-package-enhanced "windows-update.msu" --cbs-integration --dry-run
echo.

echo Test 3b: CBS integration dry run (extracted)
echo ---------------------------------------------
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted-demo" --cbs-integration --dry-run
echo.

echo Test 3c: CBS with security validation
echo --------------------------------------
DISMv2.exe add-package-enhanced "secure-package.msu" --cbs-integration --security-validation --dry-run
echo.

echo ========================================
echo 4. COMBINED OPTIONS TESTS
echo ========================================
echo.

echo Test 4a: All options combined (package)
echo ----------------------------------------
DISMv2.exe add-package-enhanced "complex-package.msu" --security-validation --force --cbs-integration --dry-run
echo.

echo Test 4b: All options combined (extracted)
echo -------------------------------------------
DISMv2.exe add-package-enhanced "/ExtractedDir:test-extracted-demo" --security-validation --force --cbs-integration --dry-run
echo.

echo ========================================
echo 5. SYNTAX COMPATIBILITY TESTS
echo ========================================
echo.

echo Note: Current syntax uses:
echo   /ExtractedDir:path (supported)
echo   --cbs-integration (supported)
echo.
echo Your attempted syntax:
echo   /CBS and /Online are not yet supported but could be added
echo.

echo Cleaning up test files...
rmdir /s /q test-extracted-demo 2>nul
echo.

echo ========================================
echo  ? Enhanced Package Addition Demo Complete!
echo ========================================
echo.
echo Demonstrated features:
echo  ? /ExtractedDir:path support
echo  ? --security-validation
echo  ? --force override
echo  ? --dry-run simulation
echo  ? --cbs-integration (Component-Based Servicing)
echo  ? All option combinations
echo.
echo To match your desired syntax, we could add:
echo  /CBS (shorthand for --cbs-integration)
echo  /Online (for online vs offline mode)
echo.

pause