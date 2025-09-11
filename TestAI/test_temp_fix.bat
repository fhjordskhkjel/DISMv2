@echo off
echo Testing Temp Directory Creation Fix
echo ===================================

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

echo Testing the fixed temp directory creation...
echo Target CAB file: Windows11.0-KB5064097-x64.cab
echo.

if not exist "Windows11.0-KB5064097-x64.cab" (
    echo NOTE: Test CAB file not found. This test will still show the improved temp directory creation.
    echo The application should now provide much better error details.
    echo.
)

echo Running CAB installation with fixed temp directory creation...
echo This should now work without the previous directory creation errors.
echo.

TestAI.exe add-package /PackagePath:"Windows11.0-KB5064097-x64.cab" /Online /LogPath:temp_fix_test.log

echo.
echo Test completed. 

if exist temp_fix_test.log (
    echo.
    echo === LOG FILE CONTENTS ===
    type temp_fix_test.log
    echo === END LOG ===
) else (
    echo No log file was created.
)

echo.
echo Key improvements in this version:
echo - Uses GetTempFileNameA for proper Windows temp naming
echo - Better error handling with detailed error codes
echo - Improved path validation and creation
echo - Enhanced debugging output
echo.
pause