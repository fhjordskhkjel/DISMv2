@echo off
echo Testing CAB File Handler - Online Installation Fix (Enhanced Diagnostics)
echo ========================================================================

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator for online installation testing.
    echo Please right-click and "Run as administrator"
    pause
    exit /b 1
)

echo Administrative privileges confirmed.
echo.

REM Test with a sample CAB file (you'll need to provide a real CAB file)
set "TEST_CAB_FILE=Windows11.0-KB5064097-x64.cab"

if not exist "%TEST_CAB_FILE%" (
    echo NOTE: Test CAB file '%TEST_CAB_FILE%' not found.
    echo Please place a test CAB file in the current directory and update this script.
    echo.
    echo For testing purposes, you can:
    echo 1. Download a Windows Update CAB file
    echo 2. Extract a CAB from an MSU file using: wusa.exe update.msu /extract:temp_dir
    echo 3. Use any existing CAB file for testing
    echo.
    echo Running diagnostic test without CAB file...
    call diagnostic_test.bat
    pause
    exit /b 1
)

echo Testing online CAB installation with enhanced error diagnostics...
echo Target CAB file: %TEST_CAB_FILE%
echo Temp directory: %TEMP%
echo Current user: %USERNAME%
echo.

REM Run the online installation test with enhanced diagnostics
TestAI.exe add-package /PackagePath:"%TEST_CAB_FILE%" /Online /LogPath:online_install_test.log

if %errorLevel% equ 0 (
    echo.
    echo ? SUCCESS: Online CAB installation completed successfully!
    echo Check online_install_test.log for detailed information.
) else (
    echo.
    echo ??  Installation completed with warnings or errors.
    echo Check online_install_test.log for detailed information.
    echo.
    echo The enhanced diagnostics should provide more details about the failure.
    echo Common issues:
    echo - Insufficient permissions
    echo - Temp directory access problems  
    echo - CAB file format issues
    echo - System file conflicts
)

echo.
echo Test completed. Log file: online_install_test.log
if exist online_install_test.log (
    echo.
    echo === LOG FILE CONTENTS ===
    type online_install_test.log
    echo === END LOG ===
)

echo.
echo If the installation failed, try running diagnostic_test.bat for more information.
pause