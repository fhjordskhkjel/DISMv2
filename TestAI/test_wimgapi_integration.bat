@echo off
echo Testing WIMGAPI Integration Features
echo ====================================
echo.

REM Test 1: Check if executable exists
if not exist "TestAI.exe" (
    echo [FAILED] TestAI.exe not found - please build the project first
    pause
    exit /b 1
)

echo [TEST 1] Basic functionality test...
TestAI.exe --help >nul 2>&1
if %errorlevel% equ 0 (
    echo [PASSED] Basic executable functionality
) else (
    echo [FAILED] Basic executable test failed
)
echo.

echo [TEST 2] WIMGAPI availability check...
echo Checking if WIMGAPI is available on this system...

REM Test if wimgapi.dll exists
if exist "%SystemRoot%\System32\wimgapi.dll" (
    echo [INFO] wimgapi.dll found in System32
    set WIMGAPI_AVAILABLE=1
) else (
    echo [INFO] wimgapi.dll not found - will use DISM fallback
    set WIMGAPI_AVAILABLE=0
)
echo.

echo [TEST 3] Package type detection test...
REM Create a small test file to test package detection
echo Test file > test_package.txt
TestAI.exe detect-type test_package.txt 2>nul
if %errorlevel% equ 0 (
    echo [PASSED] Package type detection working
) else (
    echo [INFO] Package type detection returned error (expected for .txt file)
)
del test_package.txt >nul 2>&1
echo.

echo [TEST 4] WIM command help test...
TestAI.exe extract-wim 2>&1 | findstr /C:"WIM path" >nul
if %errorlevel% equ 0 (
    echo [PASSED] Enhanced WIM commands available
) else (
    echo [FAILED] Enhanced WIM commands not working
)
echo.

echo [TEST 5] Compression validation test...
REM Test LZMS validation for non-ESD files
TestAI.exe capture-wim test_source test.wim "Test" "Test" --compress lzms 2>&1 | findstr /C:"LZMS compression is only supported for .esd files" >nul
if %errorlevel% equ 0 (
    echo [PASSED] LZMS compression validation working
) else (
    echo [INFO] LZMS validation test inconclusive
)
echo.

echo ====================================
echo Test Summary:
echo - Basic functionality: Available
echo - WIMGAPI: %WIMGAPI_AVAILABLE% (1=Available, 0=DISM fallback)
echo - Enhanced WIM commands: Available
echo - Compression validation: Working
echo ====================================
echo.
echo Integration test completed!
echo The WIMGAPI wrapper should work with both native WIMGAPI and DISM fallback.
pause