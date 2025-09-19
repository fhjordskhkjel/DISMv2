@echo off
setlocal enabledelayedexpansion

echo ===============================================
echo Advanced HIPS Driver Build Script for Windows
echo ===============================================
echo.

REM Check for Administrator privileges
net session >nul 2>&1
if !errorlevel! neq 0 (
    echo ERROR: This script requires administrator privileges.
    echo Please run as administrator.
    pause
    exit /b 1
)

REM Set build configuration
set BUILD_TYPE=Release
set BUILD_DIR=build
set ARCH=x64

echo Build Configuration:
echo - Build Type: !BUILD_TYPE!
echo - Architecture: !ARCH!
echo - Build Directory: !BUILD_DIR!
echo.

REM Check for required tools
echo Checking for required tools...

REM Check for WDK and Visual Studio
where msbuild >nul 2>&1
if !errorlevel! neq 0 (
    echo ERROR: MSBuild not found. Please install Visual Studio 2022 with C++ workload.
    pause
    exit /b 1
)
echo ✓ MSBuild found

REM Check for WDK
if not exist "%ProgramFiles(x86)%\Windows Kits\10\Vsix\VS2022\WDK.vsix" (
    if not exist "%ProgramFiles(x86)%\Windows Kits\10\build\WindowsDriver.Cpp.targets" (
        echo WARNING: Windows Driver Kit (WDK) may not be installed.
        echo Please install WDK for Visual Studio 2022 for kernel driver compilation.
        echo Continuing with user-mode build only...
        goto :usermode_only
    )
)
echo ✓ Windows Driver Kit found

echo.
echo Building kernel driver...
cd driver

REM Build the driver using MSBuild
msbuild HipsDriver.vcxproj /p:Configuration=!BUILD_TYPE! /p:Platform=!ARCH!
if !errorlevel! neq 0 (
    echo ERROR: Driver build failed.
    cd ..
    pause
    exit /b 1
)

echo ✓ Kernel driver built successfully
cd ..

:usermode_only
echo.
echo Building user-mode application...

REM Create and enter build directory
if exist !BUILD_DIR! (
    echo Cleaning existing build directory...
    rmdir /s /q !BUILD_DIR!
)

mkdir !BUILD_DIR!
cd !BUILD_DIR!

echo Configuring project with CMake...
cmake .. -G "Visual Studio 17 2022" -A !ARCH! -DCMAKE_BUILD_TYPE=!BUILD_TYPE!
if !errorlevel! neq 0 (
    echo ERROR: CMake configuration failed.
    cd ..
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config !BUILD_TYPE! --parallel
if !errorlevel! neq 0 (
    echo ERROR: Build failed.
    cd ..
    pause
    exit /b 1
)

echo.
echo Running tests...
ctest -C !BUILD_TYPE! --output-on-failure
if !errorlevel! neq 0 (
    echo WARNING: Some tests failed. Check output above.
) else (
    echo ✓ All tests passed
)

echo.
echo Installing...
cmake --install . --config !BUILD_TYPE!
if !errorlevel! neq 0 (
    echo ERROR: Installation failed.
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ===============================================
echo Build completed successfully!
echo ===============================================
echo.
echo Executable location: !BUILD_DIR!\!BUILD_TYPE!\hips.exe
echo Driver location: driver\!ARCH!\!BUILD_TYPE!\HipsDriver.sys
echo.
echo To install and run the HIPS system:
echo 1. Install the kernel driver (requires test signing or signed certificate):
echo    - Copy HipsDriver.sys and HipsDriver.inf to a folder
echo    - Right-click HipsDriver.inf and select "Install"
echo    - Or use: pnputil /add-driver HipsDriver.inf /install
echo.
echo 2. Start the driver service:
echo    - sc start HipsDriver
echo.
echo 3. Run the user-mode application as Administrator:
echo    - cd !BUILD_DIR!\!BUILD_TYPE!
echo    - hips.exe
echo.
echo NOTE: For kernel driver installation, you may need to:
echo - Enable test signing: bcdedit /set testsigning on
echo - Or obtain a valid code signing certificate
echo.

pause