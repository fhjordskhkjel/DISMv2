@echo off
setlocal enabledelayedexpansion

echo ===============================================
echo Advanced HIPS Build Script for Windows
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
set INSTALL_DIR=install

echo Build Configuration:
echo - Build Type: !BUILD_TYPE!
echo - Build Directory: !BUILD_DIR!
echo - Install Directory: !INSTALL_DIR!
echo.

REM Check for required tools
echo Checking for required tools...

where cmake >nul 2>&1
if !errorlevel! neq 0 (
    echo ERROR: CMake not found. Please install CMake and add it to PATH.
    pause
    exit /b 1
)
echo ✓ CMake found

where cl >nul 2>&1 || where gcc >nul 2>&1
if !errorlevel! neq 0 (
    echo ERROR: No C++ compiler found. Please install Visual Studio or MinGW.
    pause
    exit /b 1
)
echo ✓ C++ compiler found

echo.

REM Create and enter build directory
if exist !BUILD_DIR! (
    echo Cleaning existing build directory...
    rmdir /s /q !BUILD_DIR!
)

mkdir !BUILD_DIR!
cd !BUILD_DIR!

echo Configuring project with CMake...
cmake .. -DCMAKE_BUILD_TYPE=!BUILD_TYPE! -DCMAKE_INSTALL_PREFIX=../!INSTALL_DIR!
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
echo Executable location: !INSTALL_DIR!\bin\hips.exe
echo.
echo To run the HIPS system:
echo 1. Open Command Prompt as Administrator
echo 2. Navigate to the install directory
echo 3. Run: hips.exe
echo.
echo Configuration file: config\hips_config.json
echo Documentation: docs\README.md
echo.

pause