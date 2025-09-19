@echo off
REM Enhanced Build Script for HIPS GUI (VC++)
REM Optimized for Visual Studio 2022 with enhanced error detection and recovery

echo ===============================================
echo   HIPS GUI Build Script - VC++ Enhanced
echo ===============================================
echo.

REM Set console colors for better visibility
color 0F

echo Checking build environment...
echo.

REM Check for Visual Studio 2022 installations
set VS2022_FOUND=0
set MSBUILD_PATH=""

REM Check Professional/Enterprise
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    set VS2022_FOUND=1
    echo ✓ Visual Studio 2022 Professional found
)

REM Check Community
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    set VS2022_FOUND=1
    echo ✓ Visual Studio 2022 Community found
)

REM Check Enterprise
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    set VS2022_FOUND=1
    echo ✓ Visual Studio 2022 Enterprise found
)

REM Fallback to PATH-based MSBuild detection
if %VS2022_FOUND%==0 (
    where msbuild >nul 2>&1
    if %errorlevel% equ 0 (
        set MSBUILD_PATH=msbuild
        set VS2022_FOUND=1
        echo ✓ MSBuild found in PATH
    )
)

if %VS2022_FOUND%==0 (
    echo.
    echo ❌ ERROR: Visual Studio 2022 not found!
    echo.
    echo Please install Visual Studio 2022 with the following components:
    echo   • C++ Desktop Development workload
    echo   • Windows 10/11 SDK
    echo   • MFC and ATL support
    echo.
    echo Download from: https://visualstudio.microsoft.com/downloads/
    echo.
    echo Alternative: Run this script from a Visual Studio Developer Command Prompt
    echo.
    pause
    exit /b 1
)

REM Check for MFC support
echo Checking for MFC support...
set MFC_FOUND=0
for /f "tokens=*" %%i in ('dir /s /b "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\atlmfc\include\afx.h" 2^>nul') do (
    set MFC_FOUND=1
    echo ✓ MFC support found
    goto mfc_check_done
)
:mfc_check_done

if %MFC_FOUND%==0 (
    echo ❌ WARNING: MFC support not found
    echo Please install MFC components via Visual Studio Installer
    echo.
)

REM Check project file exists
if not exist "HipsGui.vcxproj" (
    echo ❌ ERROR: HipsGui.vcxproj not found in current directory
    echo Please run this script from the hips\gui_vcpp directory
    echo.
    pause
    exit /b 1
)

echo ✓ Project file found
echo.

REM Build for x64 Release
echo Building x64 Release configuration...
echo Command: %MSBUILD_PATH% HipsGui.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:minimal
echo.

%MSBUILD_PATH% HipsGui.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:minimal

if %errorlevel% neq 0 (
    echo.
    echo ❌ Build failed! 
    echo.
    echo Common solutions:
    echo   1. Install missing Windows SDK components
    echo   2. Install MFC and ATL support in Visual Studio
    echo   3. Run from Visual Studio Developer Command Prompt
    echo   4. Check project dependencies are installed
    echo.
    echo For detailed error information, run with verbose output:
    echo   %MSBUILD_PATH% HipsGui.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:detailed
    echo.
    pause
    exit /b 1
)

echo.
echo ===============================================
echo ✅ Build completed successfully!
echo ===============================================
echo.

REM Verify output file exists
if exist "x64\Release\HipsGui.exe" (
    echo ✓ Output file created: x64\Release\HipsGui.exe
    echo.
    
    REM Show file information
    for %%I in ("x64\Release\HipsGui.exe") do (
        echo File size: %%~zI bytes
        echo Created: %%~tI
    )
    echo.
) else (
    echo ❌ WARNING: Expected output file not found
    echo Check the build output above for potential issues
    echo.
)

echo To run the HIPS GUI:
echo   1. Ensure the HIPS kernel driver is installed and running
echo   2. Run as Administrator: x64\Release\HipsGui.exe
echo   3. Click "Connect Driver" to establish communication
echo.

echo For development:
echo   • Open HipsGui.sln in Visual Studio 2022
echo   • Use Debug configuration for development
echo   • Refer to README.md for complete setup instructions
echo.

pause