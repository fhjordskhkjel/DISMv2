@echo off
echo Testing C++20 Enhanced CAB Handler
echo.

echo Building project...
msbuild TestAI.vcxproj /p:Configuration=Release /p:Platform=x64 /verbosity:minimal

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful!
echo.

echo Testing ExtractedDir functionality...
echo.

REM Test 1: Show help
echo === Test 1: Show Usage ===
TestAI.exe
echo.

REM Test 2: Test basic CAB operations
echo === Test 2: List CAB contents ===
if exist test.cab (
    TestAI.exe list test.cab
) else (
    echo test.cab not found, skipping this test
)
echo.

REM Test 3: Test extracted directory installation (if directory exists)
echo === Test 3: Test ExtractedDir functionality ===
if exist "C:\temp\extracted_test" (
    echo Testing installation from extracted directory...
    TestAI.exe add-package /ExtractedDir:C:\temp\extracted_test /Image:C:\mount /LogPath:test.log
) else (
    echo No test extracted directory found at C:\temp\extracted_test
    echo You can test with: TestAI.exe add-package /ExtractedDir:YOUR_PATH /Online
)
echo.

echo === Summary ===
echo C++20 Enhanced CAB Handler test completed
echo.
echo New features available:
echo - 11+ MSU extraction methods
echo - ExtractedDir support: /ExtractedDir:PATH
echo - Modern C++20 features
echo - Enhanced error handling
echo.

pause