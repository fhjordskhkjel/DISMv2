@echo off
echo Testing Enhanced MSU Extraction
echo ================================
echo.

REM Check if we have an MSU file to test with
if not exist "%1" (
    echo Usage: test_msu.bat "path_to_msu_file"
    echo Example: test_msu.bat "E:\Downloads\windows11.0-kb5065426-x64.msu"
    echo.
    echo This script will test the enhanced MSU extraction capabilities
    echo including the fallback to expand.exe for complex MSU structures.
    goto end
)

set MSU_FILE=%1
echo Testing MSU file: %MSU_FILE%
echo.

REM Test 1: List MSU contents (if possible)
echo Test 1: Attempting to list MSU contents...
TestAI.exe list "%MSU_FILE%"
if errorlevel 1 (
    echo MSU listing failed - this is expected for complex MSU files
) else (
    echo MSU listing succeeded
)
echo.

REM Test 2: Extract MSU to test directory
echo Test 2: Extracting MSU contents...
mkdir msu_test_extract 2>nul
TestAI.exe extract "%MSU_FILE%" msu_test_extract
if errorlevel 1 (
    echo Standard extraction failed - this will trigger fallback method in package installation
) else (
    echo Standard extraction succeeded
    echo Extracted files:
    dir /s /b msu_test_extract
)
echo.

REM Test 3: Test the alternative extraction method directly via expand.exe
echo Test 3: Testing Windows expand.exe fallback method...
mkdir msu_expand_test 2>nul
expand.exe "%MSU_FILE%" -F:*.* msu_expand_test
if errorlevel 1 (
    echo Expand.exe extraction also failed - MSU may have unsupported structure
) else (
    echo Expand.exe extraction succeeded
    echo Extracted files via expand.exe:
    dir /s /b msu_expand_test
)
echo.

echo Test Summary:
echo ==============
echo The enhanced MSU handler now includes:
echo 1. Detailed FDI error reporting
echo 2. Fallback to expand.exe for complex MSU structures
echo 3. Improved error handling in FDI callbacks
echo.
echo This should resolve the "FDI extraction failed" error you encountered
echo with real Windows Update MSU files.

REM Cleanup
rmdir /s /q msu_test_extract 2>nul
rmdir /s /q msu_expand_test 2>nul

:end
pause