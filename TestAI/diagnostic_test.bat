@echo off
echo Testing Directory Creation - Diagnostic Tool
echo ===========================================

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator.
    echo Please right-click and "Run as administrator"
    pause
    exit /b 1
)

echo Administrative privileges confirmed.
echo.

echo Testing temp directory access...
echo Temp directory: %TEMP%
echo Current user: %USERNAME%
echo.

REM Create a test directory similar to what the application creates
set "TEST_DIR=%TEMP%\cab_diagnostic_test_%RANDOM%"
echo Creating test directory: %TEST_DIR%

mkdir "%TEST_DIR%" 2>nul
if %errorLevel% equ 0 (
    echo ? SUCCESS: Test directory created successfully
    
    REM Test file creation
    echo test > "%TEST_DIR%\test.txt" 2>nul
    if %errorLevel% equ 0 (
        echo ? SUCCESS: File creation in temp directory works
        del "%TEST_DIR%\test.txt" 2>nul
    ) else (
        echo ? FAILED: Cannot create files in temp directory
    )
    
    rmdir "%TEST_DIR%" 2>nul
    if %errorLevel% equ 0 (
        echo ? SUCCESS: Directory cleanup works
    ) else (
        echo ??  WARNING: Could not clean up test directory
    )
) else (
    echo ? FAILED: Cannot create test directory in temp location
    echo This indicates a permissions or disk space issue
)

echo.
echo Testing CAB file with detailed diagnostics...

REM Check if CAB file exists
set "TEST_CAB_FILE=Windows11.0-KB5064097-x64.cab"
if not exist "%TEST_CAB_FILE%" (
    echo NOTE: Test CAB file '%TEST_CAB_FILE%' not found.
    echo Skipping CAB installation test.
    echo Please place a test CAB file in the current directory for full testing.
) else (
    echo Running CAB installation with enhanced diagnostics...
    TestAI.exe add-package /PackagePath:"%TEST_CAB_FILE%" /Online /LogPath:diagnostic_test.log
    
    echo.
    if exist diagnostic_test.log (
        echo === DIAGNOSTIC LOG ===
        type diagnostic_test.log
        echo === END LOG ===
    )
)

echo.
echo Diagnostic test completed.
pause