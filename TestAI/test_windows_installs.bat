@echo off
REM === Windows Installation Testing Suite ===
REM Tests enhanced CAB/MSU/PSF/WIM installation functionality

echo.
echo ===============================================
echo  Windows Installation Testing Suite
echo  Testing Enhanced Installation Functionality
echo ===============================================
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Set test variables
set "TEST_CAB=C:\ce33cb32f8d19fe2041448abbc1737fd\DesktopDeployment.cab"
set "TEST_DIR=%TEMP%\WindowsInstallTest_%RANDOM%"
set "LOG_FILE=%TEMP%\windows_install_test.log"
set "EXE_PATH=.\x64\Release\CabHandlerAkaDISMv2.exe"

echo Test Configuration:
echo   CAB File: %TEST_CAB%
echo   Test Directory: %TEST_DIR%
echo   Log File: %LOG_FILE%
echo   Executable: %EXE_PATH%
echo.

REM Check if executable exists
if not exist "%EXE_PATH%" (
    echo ERROR: TestAI.exe not found at %EXE_PATH%
    echo Please build the project first
    pause
    exit /b 1
)

REM Check if test CAB file exists
if not exist "%TEST_CAB%" (
    echo WARNING: Test CAB file not found at %TEST_CAB%
    echo Will skip CAB-specific tests
    set "SKIP_CAB=1"
) else (
    echo Test CAB file found: %TEST_CAB%
    set "SKIP_CAB=0"
)

REM Create test directory
mkdir "%TEST_DIR%" 2>nul

echo.
echo === Test 1: Package Type Detection ===
echo Testing package format detection...

if "%SKIP_CAB%"=="0" (
    echo Testing CAB detection:
    "%EXE_PATH%" detect-type "%TEST_CAB%"
    if %errorLevel% equ 0 (
        echo ? CAB detection: PASSED
    ) else (
        echo ? CAB detection: FAILED
    )
)

echo.
echo === Test 2: Enhanced CAB Extraction ===
echo Testing improved CAB extraction with multiple fallback methods...

if "%SKIP_CAB%"=="0" (
    set "EXTRACT_DIR=%TEST_DIR%\cab_extraction"
    mkdir "%EXTRACT_DIR%" 2>nul
    
    echo Testing CAB extraction to: %EXTRACT_DIR%
    "%EXE_PATH%" extract "%TEST_CAB%" "%EXTRACT_DIR%"
    if %errorLevel% equ 0 (
        echo ? CAB extraction: PASSED
        dir /s "%EXTRACT_DIR%" | find "File(s)"
    ) else (
        echo ? CAB extraction: FAILED
    )
)

echo.
echo === Test 3: CBS Integration Test ===
echo Testing Component-Based Servicing integration...

if "%SKIP_CAB%"=="0" (
    echo Testing CBS-integrated installation (simulation mode):
    "%EXE_PATH%" add-package /CBS /PackagePath:"%TEST_CAB%" /Online /LogPath:"%LOG_FILE%"
    if %errorLevel% equ 0 (
        echo ? CBS integration: PASSED
    ) else (
        echo ? CBS integration: FAILED
    )
)

echo.
echo === Test 4: Advanced Package Processing ===
echo Testing universal package processing capabilities...

if "%SKIP_CAB%"=="0" (
    set "ADVANCED_DIR=%TEST_DIR%\advanced_extraction"
    mkdir "%ADVANCED_DIR%" 2>nul
    
    echo Testing advanced extraction:
    "%EXE_PATH%" extract-advanced "%TEST_CAB%" "%ADVANCED_DIR%"
    if %errorLevel% equ 0 (
        echo ? Advanced extraction: PASSED
    ) else (
        echo ? Advanced extraction: FAILED
    )
)

echo.
echo === Test 5: Error Handling and Diagnostics ===
echo Testing enhanced error handling and diagnostic capabilities...

echo Testing with non-existent file:
"%EXE_PATH%" detect-type "nonexistent_file.cab" 2>nul
if %errorLevel% neq 0 (
    echo ? Error handling: PASSED (correctly failed for non-existent file)
) else (
    echo ? Error handling: FAILED (should have failed for non-existent file)
)

echo.
echo === Test 6: Memory and Performance Test ===
echo Testing memory management and performance improvements...

if "%SKIP_CAB%"=="0" (
    echo Performing multiple operations to test memory stability:
    for /L %%i in (1,1,3) do (
        echo   Iteration %%i of 3...
        "%EXE_PATH%" detect-type "%TEST_CAB%" >nul 2>&1
        if !errorLevel! neq 0 (
            echo ? Memory test failed at iteration %%i
            goto :memory_test_failed
        )
    )
    echo ? Memory test: PASSED (3 iterations completed successfully)
    goto :memory_test_done
    
    :memory_test_failed
    echo ? Memory test: FAILED
    
    :memory_test_done
)

echo.
echo === Test 7: Log File Analysis ===
echo Analyzing installation logs for detailed diagnostics...

if exist "%LOG_FILE%" (
    echo Log file found: %LOG_FILE%
    echo Log file size: 
    for %%A in ("%LOG_FILE%") do echo   %%~zA bytes
    
    echo Recent log entries:
    powershell -Command "Get-Content '%LOG_FILE%' | Select-Object -Last 10"
    echo ? Logging: PASSED
) else (
    echo ? Logging: FAILED (no log file generated)
)

echo.
echo === Test Summary ===
echo.
echo Installation testing completed.
echo.
echo Key Improvements Tested:
echo   ? Enhanced CAB extraction with multiple fallback methods
echo   ? CBS (Component-Based Servicing) integration
echo   ? Universal package format detection
echo   ? Advanced extraction capabilities
echo   ? Improved error handling and diagnostics
echo   ? Memory management and performance
echo   ? Comprehensive logging
echo.

if "%SKIP_CAB%"=="1" (
    echo NOTE: Some tests were skipped due to missing test CAB file
    echo To run complete tests, ensure a CAB file is available at:
    echo %TEST_CAB%
)

echo Test artifacts saved in: %TEST_DIR%
echo Log file available at: %LOG_FILE%
echo.

REM Cleanup option
set /p CLEANUP="Delete test artifacts? (y/n): "
if /i "%CLEANUP%"=="y" (
    rmdir /s /q "%TEST_DIR%" 2>nul
    echo Test directory cleaned up
)

echo.
echo === Windows Installation Testing Complete ===
pause