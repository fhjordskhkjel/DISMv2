@echo off
REM Build script for HIPS GUI (VC++)
REM Requires Visual Studio 2022 with VC++ and MFC components

echo Building HIPS GUI...
echo.

REM Check if MSBuild is available
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: MSBuild not found. Please ensure Visual Studio 2022 is installed.
    echo You may need to run this from a Visual Studio Developer Command Prompt.
    pause
    exit /b 1
)

REM Build for x64 Release
echo Building x64 Release configuration...
msbuild HipsGui.vcxproj /p:Configuration=Release /p:Platform=x64 /m

if %errorlevel% neq 0 (
    echo.
    echo Build failed! Please check the error messages above.
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.
echo The GUI executable should be located at:
echo   x64\Release\HipsGui.exe
echo.
echo To run the GUI:
echo   1. Make sure the HIPS driver is installed and running
echo   2. Run HipsGui.exe as Administrator
echo.
pause