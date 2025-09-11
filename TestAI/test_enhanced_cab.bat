@echo off
echo Enhanced CAB Extraction Diagnostics
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

REM Use the extracted directory from the previous test
set "TEST_DIR=C:\Windows\SoftwareDistribution\Download\9a8aecd6dc9f8df7fbebfb2b0e2ed76c"
set "TEST_CAB=DesktopDeployment.cab"

if not exist "%TEST_DIR%" (
    echo NOTE: Test directory '%TEST_DIR%' not found.
    echo Please update the path to point to your extracted MSU directory.
    pause
    exit /b 1
)

if not exist "%TEST_DIR%\%TEST_CAB%" (
    echo NOTE: CAB file '%TEST_CAB%' not found in '%TEST_DIR%'.
    echo Listing available files in the directory:
    dir "%TEST_DIR%" /b
    echo.
    echo Please update the script with the correct CAB file name.
    pause
    exit /b 1
)

echo Testing enhanced CAB extraction diagnostics...
echo Source directory: %TEST_DIR%
echo Target CAB file: %TEST_CAB%
echo.

echo Examining the CAB file first...
echo File details:
dir "%TEST_DIR%\%TEST_CAB%"
echo.

echo Checking file signature...
powershell -Command "$bytes = Get-Content '%TEST_DIR%\%TEST_CAB%' -Encoding Byte -TotalCount 4; $signature = [System.Text.Encoding]::ASCII.GetString($bytes); Write-Host 'First 4 bytes:' $signature '(Hex:' ([BitConverter]::ToString($bytes)) ')'"
echo.

echo Now testing the online installation with enhanced diagnostics...
TestAI.exe add-package /ExtractedDir:"%TEST_DIR%" /Online /LogPath:enhanced_cab_test.log

echo.
echo Test completed. Results:

if exist enhanced_cab_test.log (
    echo.
    echo === LOG FILE CONTENTS ===
    type enhanced_cab_test.log
    echo === END LOG ===
) else (
    echo No log file was created.
)

echo.
echo The enhanced diagnostics should now show:
echo - Detailed FDI error codes
echo - CAB file signature validation
echo - Alternative extraction method attempts
echo - Step-by-step debugging information
echo.
pause